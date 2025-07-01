#include "editor.h"
#include "rawmode.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h> // For memcpy, strlen

editorConfig E; // 定义全局编辑器配置实例

// 初始化编辑器配置
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.dirty = 0; // 初始化为未修改

    // --- 搜索相关字段初始化 START ---
    E.query = NULL; // 初始搜索词为空
    E.last_match_row = -1; // -1 表示未找到或未开始搜索
    E.last_match_col = -1;
    E.search_direction = 1; // 默认向下搜索
    E.current_match_row = -1; // 默认没有当前高亮匹配项
    E.current_match_col = -1;
    // --- 搜索相关字段初始化 END ---

    // 获取终端大小
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        E.screenrows = 24; // 默认值
        E.screencols = 80; // 默认值
    } else {
        E.screencols = ws.ws_col;
        E.screenrows = ws.ws_row;
    }
    E.screenrows -= 2; // 减去状态栏和消息栏的行数
}

// 刷新屏幕
void editorRefreshScreen() {
    editorScroll(); // 在刷新前处理滚动

    struct abuf ab = ABUF_INIT; // 使用缓冲区

    abAppend(&ab, "\x1b[?25l", 6); // 隐藏光标
    abAppend(&ab, "\x1b[H", 3);    // 将光标移动到屏幕左上角(1,1)

    // 绘制文本行
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = E.rowoff + y;
        if (filerow >= E.numrows) {
            // 如果是空行，显示欢迎消息或波浪线
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Hello world");
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(&ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(&ab, " ", 1);
                abAppend(&ab, welcome, welcomelen);
            } else {
                abAppend(&ab, "~", 1); // 显示波浪线
            }
        } else {
            // 绘制文件内容 (使用渲染后的文本 render)
            int len = E.row[filerow].rsize - E.coloff; // 使用 rsize
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;

            char *c = &E.row[filerow].render[E.coloff];
            size_t current_render_col = E.coloff; // 当前渲染的列在 render 字符串中的索引

            for (int i = 0; i < len; i++) {
                // 判断是否是当前匹配项的高亮区域
                // 仅当 filerow 是当前高亮行 且 当前渲染位置在匹配项范围内时高亮
                if (filerow == E.current_match_row &&
                    current_render_col >= (size_t)E.current_match_col &&
                    current_render_col < (size_t)E.current_match_col + (E.query ? strlen(E.query) : 0))
                {
                    abAppend(&ab, "\x1b[7m", 4); // 反色显示 (高亮)
                }
                abAppend(&ab, &c[i], 1); // 绘制字符
                if (filerow == E.current_match_row &&
                    current_render_col >= (size_t)E.current_match_col &&
                    current_render_col < (size_t)E.current_match_col + (E.query ? strlen(E.query) : 0))
                {
                    abAppend(&ab, "\x1b[m", 3); // 恢复正常显示
                }
                current_render_col++;
            }
            // abAppend(&ab, &E.row[filerow].render[E.coloff], len); // 使用 render
        }

        abAppend(&ab, "\x1b[K", 3); // 清除行尾
        abAppend(&ab, "\r\n", 2);
    }

    editorDrawStatusBar(&ab);  // 绘制状态栏
    editorDrawMessageBar(&ab); // 绘制消息栏

    // 将光标定位到cx, cy
    // 注意：光标在屏幕上的位置是根据渲染后的文本计算的
    int rx = 0; // render x
    if (E.cy < E.numrows) { // 确保当前行有效
        for (int i = 0; i < E.cx; i++) {
            if (E.row[E.cy].chars[i] == '\t') {
                rx += (TAB_WIDTH - (rx % TAB_WIDTH));
            } else {
                rx++;
            }
        }
    }

    char buf[32];
    // +1 是因为终端坐标是 1-based
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); // 显示光标

    write(STDOUT_FILENO, ab.b, ab.len); // 一次性写入所有内容
    abFree(&ab); // 释放缓冲区内存
}

// 处理滚动
void editorScroll() {
    // 处理光标移动后，防止cx超出当前行的实际渲染长度
    int rx = 0; // render x
    if (E.cy < E.numrows) {
        for (int i = 0; i < E.cx; i++) {
            if (E.row[E.cy].chars[i] == '\t') {
                rx += (TAB_WIDTH - (rx % TAB_WIDTH));
            } else {
                rx++;
            }
        }
    }

    // 垂直滚动
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }

    // 水平滚动
    // 使用渲染后的 rx 来判断水平滚动
    if (rx < E.coloff) {
        E.coloff = rx;
    }
    if (rx >= E.coloff + E.screencols) {
        E.coloff = rx - E.screencols + 1;
    }
}

// 光标移动
void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) { // 在行首按左箭头，移动到上一行行尾
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            // 移动光标，不超过当前行的原始文本长度
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.numrows - 1) { // 在行尾按右箭头，移动到下一行行首
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy > 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows - 1) { // 只能移动到已存在的文本行的末尾
                E.cy++;
            }
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            // 移动到当前屏幕的顶部或底部
            if (key == PAGE_UP) {
                E.cy = E.rowoff;
            } else { // PAGE_DOWN
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows - 1) E.cy = E.numrows - 1; // 确保不超过文件末尾
            }

            // 再按箭头键移动一屏幕行数
            for (int i = 0; i < E.screenrows; i++) {
                editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (row) {
                E.cx = row->size;
            }
            break;
    }

    // 确保光标的横向位置不超过当前行的实际原始长度
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}