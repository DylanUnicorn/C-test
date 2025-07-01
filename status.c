#include "editor.h"
#include <stdio.h> // For snprintf
#include <string.h> // For strlen
#include <stdarg.h> // For va_list, va_start, va_end
#include <time.h>   // For time

// 绘制状态栏
void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); // 反色显示
    char status[80], rstatus[80];

    char *display_filename = E.filename ? E.filename : "[No Name]";
    int len_filename_part;
    // 文件名最多显示 20 字符，否则显示 "..."
    if (E.filename && strlen(E.filename) > 20) {
        len_filename_part = snprintf(status, sizeof(status), "%.20s...", E.filename);
    } else {
        len_filename_part = snprintf(status, sizeof(status), "%s", display_filename);
    }

    // 添加脏标志
    int dirty_len = 0;
    if (E.dirty) {
        dirty_len = snprintf(status + len_filename_part, sizeof(status) - len_filename_part, " (modified)");
    }

    // 添加行数信息
    snprintf(status + len_filename_part + dirty_len, sizeof(status) - (len_filename_part + dirty_len), " - %d lines", E.numrows);

    // 右侧显示当前行号/总行数
    // 如果没有行，显示1/1，防止0/0
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows == 0 ? 1 : E.numrows);

    int len = strlen(status);
    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);

    // 填充空白并将右侧状态信息放在最右边
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3); // 恢复正常显示
    abAppend(ab, "\r\n", 2);
}

// 绘制消息栏（用于显示临时消息或用户输入提示）
void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3); // 清除当前行
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;

    // 消息只显示 5 秒，或者如果没有消息就清除
    if (msglen && time(NULL) - E.statusmsg_time < 5) {
        abAppend(ab, E.statusmsg, msglen);
    }
    // 如果没有临时消息，这里可以留空，或者未来用于显示搜索提示
}

// 设置状态栏消息
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}