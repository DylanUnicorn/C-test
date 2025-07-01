#include "editor.h"
#include "rawmode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For iscntrl, isprint
#include <stdarg.h> // For va_list, va_start, va_end

// --- Editor Operations (Core Editing Logic) ---

// 插入字符到当前光标位置
void editorInsertChar(int c) {
    if (E.cy == E.numrows) { // 如果光标在文件末尾（即没有行），则添加一个新行
        editorInsertRow(E.numrows, "", 0);
    }
    editorInsertCharInRow(&E.row[E.cy], E.cx, c);
    E.cx++; // 移动光标
}

// 删除当前光标位置的字符
void editorDeleteChar() {
    if (E.cy == E.numrows) return; // 如果没有行，无法删除
    if (E.cx == 0 && E.cy == 0 && E.numrows == 1 && E.row[0].size == 0) return; // 如果是唯一空行，无法删除

    erow *row = &E.row[E.cy];
    if (E.cx > 0) { // 如果光标不在行首，删除当前字符
        editorDeleteCharInRow(row, E.cx - 1);
        E.cx--;
    } else { // 如果光标在行首，执行跨行删除（将当前行与上一行合并）
        if (E.cy == 0) return; // 如果在第一行行首，无法向上删除

        E.cx = E.row[E.cy - 1].size; // 将光标移动到上一行末尾
        // 将当前行的内容追加到上一行
        erow *prev_row = &E.row[E.cy - 1];
        prev_row->chars = realloc(prev_row->chars, prev_row->size + row->size + 1);
        if (prev_row->chars == NULL) die("realloc for merge");
        memcpy(&prev_row->chars[prev_row->size], row->chars, row->size);
        prev_row->chars[prev_row->size + row->size] = '\0';
        prev_row->size += row->size;
        editorUpdateRow(prev_row);

        editorDeleteRow(E.cy); // 删除当前行
        E.cy--; // 移动光标到合并后的行
    }
}

// 在当前光标位置插入新行 (回车)
void editorInsertNewline() {
    // if (E.cx == 0) { // 如果在行首，直接插入一个空行
    //     editorInsertRow(E.cy, "", 0);
    // } else { // 如果在行中间或行尾，分割当前行
    //     erow *row = &E.row[E.cy];
    //     // 将光标后的部分移动到新行
    //     editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    //     // 截断当前行
    //     row->size = E.cx;
    //     char *new_chars = realloc(row->chars, row->size + 1);
    //     if (new_chars == NULL) {
    //         die("realloc failed when truncating row in editorInsertNewline");
    //     }   
    //     // row->chars = realloc(row->chars, row->size + 1);
    //     // if (row->chars == NULL && row->size > 0) die("realloc for split");
    //     row->chars = new_chars; // 更新指针
    //     row->chars[row->size] = '\0';
    //     editorUpdateRow(row);
    // }
    // E.cy++; // 移动光标到新行
    // E.cx = 0; // 移动光标到新行开头
        // 获取当前行的引用，确保有效性
    if (E.cy >= E.numrows && E.cx == 0) { // 如果在文件末尾并且光标在虚拟行首，插入一个新空行
        editorInsertRow(E.numrows, "", 0);
        E.cy++;
        E.cx = 0;
        return; // 直接返回，因为处理完毕
    }

    // 确保当前行是有效的，否则无法操作
    if (E.cy >= E.numrows) return; // 避免访问无效行

    erow *row = &E.row[E.cy];

    if (E.cx == 0) { // 如果光标在行首，直接插入一个空行
        editorInsertRow(E.cy, "", 0);
    } else if (E.cx == row->size) { // 光标在行尾：只在下一行插入一个空行，当前行不变
        editorInsertRow(E.cy + 1, "", 0); // 在下一行插入一个空行
    } else { // 光标在行中间，分割当前行
        // 将光标后的部分移动到新行
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        // 截断当前行
        row->size = E.cx;
        char *new_chars = realloc(row->chars, row->size + 1); // 截断时才需要 realloc
        if (new_chars == NULL) {
            die("realloc failed when truncating row in editorInsertNewline");
        }
        row->chars = new_chars;
        row->chars[row->size] = '\0'; // 确保截断后的字符串以 null 终止
        editorUpdateRow(row);
    }

    E.cy++; // 移动光标到新行
    E.cx = 0; // 移动光标到新行开头
    E.dirty++; // 发生了修改
}

// 提示用户输入，并返回输入字符串
char *editorPrompt(const char *prompt, ...) {
    char *buf = malloc(128); // 临时缓冲区
    if (buf == NULL) die("malloc prompt buffer");
    int buflen = 0;
    buf[0] = '\0';

    while (1) {
        // 在状态栏显示提示和已输入内容
        // 使用变参以便 prompt 可以格式化
        va_list ap;
        va_start(ap, prompt);
        vsnprintf(E.statusmsg, sizeof(E.statusmsg), prompt, ap);
        va_end(ap);
        editorRefreshScreen(); // 刷新屏幕以显示提示

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == '\x7f') { // Backspace or Delete
            if (buflen > 0) {
                buf[--buflen] = '\0';
            }
        } else if (c == '\x1b') { // Escape
            editorSetStatusMessage(""); // 清除消息
            free(buf);
            return NULL; // 用户取消
        } else if (c == '\r') { // Enter
            if (buflen > 0) {
                editorSetStatusMessage(""); // 清除消息
                return buf; // 返回用户输入的字符串
            }
        } else if (!iscntrl(c) && c < 128) { // 只允许可见ASCII字符输入
            if (buflen < 127) { // 防止缓冲区溢出
                buf[buflen++] = c;
                buf[buflen] = '\0';
            }
        }
    }
}

// --- Input Processing ---

void editorProcessKeypress() {
    static int quit_times = 3; // 退出确认计数
    int c = editorReadKey();

    switch (c) {
        case '\r': // 回车键：插入新行
            editorInsertNewline();
            break;

        case CTRL_KEY('q'): // Ctrl-Q：退出
            if (E.dirty && quit_times > 0) {
                editorSetStatusMessage("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return; // 不退出，等待再次确认
            }
            // 退出前清理
            write(STDOUT_FILENO, "\x1b[2J", 4); // 清除屏幕
            write(STDOUT_FILENO, "\x1b[H", 3);  // 移动光标到主页
            editorFreeAllRows(); // 释放所有行数据内存
            if (E.filename) {
                free(E.filename);
                E.filename = NULL;
            }
            exit(0);
            break;

        case CTRL_KEY('s'): // Ctrl-S：保存文件
            editorSave();
            break;

        case HOME_KEY:
        case END_KEY:
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case PAGE_UP:
        case PAGE_DOWN:
            editorMoveCursor(c);
            break;

        case DEL_KEY: // Delete 键：删除字符
        case CTRL_KEY('h'): // Ctrl-H (Backspace)
        case '\x7f': // Backspace
            editorDeleteChar();
            break;

        // 屏蔽其他特殊字符和控制字符的直接插入
        case '\x1b': // Esc
            break; // 什么都不做

        default:
            // 只处理可打印字符和制表符作为普通字符插入
            if (!iscntrl(c) || c == '\t') {
                editorInsertChar(c);
            }
            break;
    }
    quit_times = 3; // 每次进行操作后重置退出确认计数
}