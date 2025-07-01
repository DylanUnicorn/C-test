// #include "editor.h" // Include editor.h first to ensure its definitions are picked up
// #include "rawmode.h"

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdarg.h> // For va_list, va_start, va_end
// #include <unistd.h> // For write, read
// #include <sys/ioctl.h> // For ioctl, TIOCGWINSZ
// #include <time.h> // For time_t, time
// #include <errno.h> // For EAGAIN etc.
// #include <ctype.h> // For isprint

// // Global editor configuration instance
// editorConfig E;

// // --- Append Buffer (already moved to editor.h for visibility) ---
// // struct abuf {
// //     char *b;
// //     int len;
// // };


// void abAppend(struct abuf *ab, const char *s, int len) { // Moved to editor.h
//     char *new = realloc(ab->b, ab->len + len);

//     if (new == NULL) 
//     {
//         perror("realloc failed in abAppend");
//         exit(1);
//         // return;
//     }
//     memcpy(&new[ab->len], s, len);
//     ab->b = new;
//     ab->len += len;
// }

// void abFree(struct abuf *ab) { // Moved to editor.h
//     free(ab->b);
//     ab->b = NULL;
//     ab->len = 0;
// }

// // --- Raw Operations ---
// // 更新行的渲染内容（处理 Tab）
// void editorUpdateRow(erow *row) {
//     int tabs = 0;
//     for (int i = 0; i < row->size; i++) {
//         if (row->chars[i] == '\t') tabs++;
//     }

//     free(row->render);
//     row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);
//     if (row->render == NULL) die("malloc render"); // 错误处理

//     int idx = 0;
//     for (int i = 0; i < row->size; i++) {
//         if (row->chars[i] == '\t') {
//             do {
//                 row->render[idx++] = ' ';
//             } while (idx % KILO_TAB_STOP != 0); // 确保对齐到 Tab 停止位
//         } else {
//             row->render[idx++] = row->chars[i];
//         }
//     }
//     row->render[idx] = '\0';
//     row->rsize = idx;
// }

// // 向编辑器中追加一行
// void editorAppendRow(char *s, size_t len) {
//     E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
//     if (E.row == NULL) die("realloc row"); // 错误处理

//     E.row[E.numrows].size = len;
//     E.row[E.numrows].chars = malloc(len + 1);
//     if (E.row[E.numrows].chars == NULL) die("malloc chars"); // 错误处理
//     memcpy(E.row[E.numrows].chars, s, len);
//     E.row[E.numrows].chars[len] = '\0';

//     E.row[E.numrows].rsize = 0;
//     E.row[E.numrows].render = NULL;
//     editorUpdateRow(&E.row[E.numrows]); // 更新渲染后的行

//     E.numrows++;
// }

// // --- Editor Initialization ---

// void initEditor() {
//     E.cx = 0;
//     E.cy = 0;
//     E.rowoff = 0;
//     E.coloff = 0;
//     E.numrows = 0;
//     E.row = NULL;
//     E.filename = NULL;
//     E.statusmsg[0] = '\0';
//     E.statusmsg_time = 0;

//     // Get terminal size
//     struct winsize ws;
//     if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {   
//         // 假如ioctl失败，使用默认值
//         E.screenrows = 24;
//         E.screencols = 80;
//     } else {
//         E.screencols = ws.ws_col;
//         E.screenrows = ws.ws_row;
//     }
//     // 减去状态栏和消息栏的两行
//     E.screenrows -= 2;
// }

// // --- Editor Drawing ---

// // 绘制每一行文本到缓冲区
// void editorDrawRows(struct abuf *ab) {
//     for (int y = 0; y < E.screenrows; y++) {
//         int filerow = E.rowoff + y;
//         if (filerow >= E.numrows) {
//             // If it's an empty line, display welcome message or tilde
//             if (E.numrows == 0 && y == E.screenrows / 3) {
//                 char welcome[80];
//                 int welcomelen = snprintf(welcome, sizeof(welcome),
//                     "Hello, World!"); // Replace with your version
//                 if (welcomelen > E.screencols) welcomelen = E.screencols;
//                 int padding = (E.screencols - welcomelen) / 2;
//                 if (padding) {
//                     abAppend(ab, "~", 1);
//                     padding--;
//                 }
//                 while (padding--) abAppend(ab, " ", 1);
//                 abAppend(ab, welcome, welcomelen);
//             } else {
//                 abAppend(ab, "~", 1);
//             }
//         } else {
//             // Draw file content
//             int len = E.row[filerow].size - E.coloff;
//             if (len < 0) len = 0;
//             if (len > E.screencols) len = E.screencols;
//             abAppend(ab, &E.row[filerow].chars[E.coloff], len);
//         }

//         // Clear to end of line
//         abAppend(ab, "\x1b[K", 3);
//         abAppend(ab, "\r\n", 2);
//     }
// }

// // 状态栏
// void editorDrawStatusBar(struct abuf *ab) {
//     abAppend(ab, "\x1b[7m", 4); // Invert colors
//     char status[80], rstatus[80];
//     int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
//         E.filename ? E.filename : "[No Name]", E.numrows,
//         ""); // Add dirty flag later
//     int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
//         E.cy + 1, E.numrows);

//     if (len > E.screencols) len = E.screencols;
//     abAppend(ab, status, len);
//     while (len < E.screencols) {
//         if (E.screencols - len == rlen) {
//             abAppend(ab, rstatus, rlen);
//             break;
//         } else {
//             abAppend(ab, " ", 1);
//             len++;
//         }
//     }
//     abAppend(ab, "\x1b[m", 3); // Reset colors
//     abAppend(ab, "\r\n", 2);
// }

// // 绘制消息栏（可能会用于搜索）
// void editorDrawMessageBar(struct abuf *ab) {
//     abAppend(ab, "\x1b[K", 3); // Clear current line
//     int msglen = strlen(E.statusmsg);
//     if (msglen > E.screencols) msglen = E.screencols;
//     if (msglen && time(NULL) - E.statusmsg_time < 5) { // Message displays for 5 seconds
//         abAppend(ab, E.statusmsg, msglen);
//     } else {
//         /// 搜索的一些提示信息ctrl + F

//     }
// }

// // 刷新屏幕
// void editorRefreshScreen() {
//     editorScroll(); // Process scrolling before refreshing

//     struct abuf ab = ABUF_INIT;

//     abAppend(&ab, "\x1b[?25l", 6); // 隐藏光标
//     abAppend(&ab, "\x1b[H", 3);    // 光标移动到屏幕左上角

//     editorDrawRows(&ab);
//     editorDrawStatusBar(&ab);
//     editorDrawMessageBar(&ab);

//     // 光标定位到cx， cy位置
//     /// 将 E.cx 和 E.cy 转换为屏幕坐标
//     int rx = 0;
//     if (E.cy < E.numrows) { // 确保当前行有效
//         for (int i = 0; i < E.cx; i++) {
//             if (E.row[E.cy].chars[i] == '\t') {
//                 rx += (KILO_TAB_STOP - (rx % KILO_TAB_STOP));
//             } else {
//                 rx++;
//             }
//         }
//     }

//     char buf[32];
//     snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
//                                             (rx - E.coloff) + 1); /// 使用rx !
//     abAppend(&ab, buf, strlen(buf));

//     abAppend(&ab, "\x1b[?25h", 6); // 显示光标

//     write(STDOUT_FILENO, ab.b, ab.len);
//     abFree(&ab);
// }

// // 设置状态栏消息
// void editorSetStatusMessage(const char *fmt, ...) {
//     va_list ap;
//     va_start(ap, fmt);
//     vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
//     va_end(ap);
//     E.statusmsg_time = time(NULL);
// }

// // --- 滚动 ---
// void editorScroll() {
//     // 如果光标在屏幕外，调整偏移量
//     int rx = 0;
//     if (E.cy < E.numrows) { // 确保当前行有效
//         for (int i = 0; i < E.cx; i++) {
//             if (E.row[E.cy].chars[i] == '\t') {
//                 rx += (KILO_TAB_STOP - (rx % KILO_TAB_STOP));
//             } else {
//                 rx++;
//             }
//         }
//     }

//     // 垂直滚动
//     if (E.cy < E.rowoff) {
//         E.rowoff = E.cy;
//     }
//     if (E.cy >= E.rowoff + E.screenrows) {
//         E.rowoff = E.cy - E.screenrows + 1;
//     }

//     // 水平滚动
//     if (E.cx < E.coloff) {
//         E.coloff = E.cx;
//     }
//     if (E.cx >= E.coloff + E.screencols) {
//         E.coloff = E.cx - E.screencols + 1;
//     }
// }

// // --- 光标移动 ---

// void editorMoveCursor(int key) {
//     /// 时刻注意光标不要超过行尾，否则访问非法内存
//     erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy]; /// 使用原始文本的行

//     switch (key) {
//         case ARROW_LEFT:
//             if (E.cx > 0) {
//                 E.cx--;
//             } else if (E.cy > 0) { // 如果在行首按左箭头，移动到上一行行尾
//                 E.cy--;
//                 E.cx = E.row[E.cy].size;
//             }
//             break;
//         case ARROW_RIGHT:
//             if (row && E.cx < row->size) { // 移动光标，不超过当前行的原始文本长度
//                 E.cx++;
//             } else if (row && E.cx == row->size && E.cy < E.numrows - 1) { // 如果在行尾按右箭头，移动到下一行行首
//                 E.cy++;
//                 E.cx = 0;
//             }
//             break;
//         case ARROW_UP:
//             if (E.cy > 0) {
//                 E.cy--;
//             }
//             break;
//         case ARROW_DOWN:
//             if (E.cy < E.numrows - 1) { // 只能移动到已存在的文本行的末尾 这个-1有待斟酌
//                 E.cy++;
//             }
//             break;
//         case PAGE_UP: {
//             // int times = E.screenrows;
//             // while (times--) {
//             //     editorMoveCursor(ARROW_UP);
//             // }
//             // break;
//         }
//         case PAGE_DOWN: {
//             if(key == PAGE_UP) {
//                 E.cy = E.rowoff; // 移动到当前屏幕可见的第一行
//             } else {
//                 E.cy = E.rowoff + E.screenrows - 1; // 移动到当前屏幕可见的最后一行
//                 if (E.cy > E.numrows - 1) E.cy = E.numrows - 1; // 确保不超过文件末尾
//             }

//             for (int i = 0; i < E.screenrows; i++) {
//                 editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
//             }
//             break;
//         }
//         case HOME_KEY:
//             E.cx = 0;
//             break;
//         case END_KEY:
//             if (row) {
//                 E.cx = row->size;
//             }
//             break;
//     }

//     // 调整光标的横向位置，防止光标移动到空行的末尾或者比下一行的内容长
//     // 每次光标垂直移动后，确保E.cx不超过当前行的实际长度
//     row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
//     int rowlen = row ? row->size : 0;
//     if (E.cx > rowlen) {
//         E.cx = rowlen;
//     }
// }

// // --- Input Processing ---
// void editorProcessKeypress() {
//     int c = editorReadKey();

//     switch (c) {
//         case '\r': // Enter key (for future text editing, but now we just handle it)
//             break;

//         case CTRL_KEY('q'): // Ctrl-Q to quit
//             write(STDOUT_FILENO, "\x1b[2J", 4); // Clear screen
//             write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor home
//             exit(0);
//             break;

//         case HOME_KEY:
//         case END_KEY:
//         case ARROW_UP:
//         case ARROW_DOWN:
//         case ARROW_LEFT:
//         case ARROW_RIGHT:
//         case PAGE_UP:
//         case PAGE_DOWN:
//             editorMoveCursor(c);
//             break;
        
//         case DEL_KEY:
//             // Handle delete key
//             break;
//     }
// }

// // --- File Operations ---
// /// 将会支持保存文件和打开文件
// void editorOpen(char *filename) {
//     if(E.filename){
//         free(E.filename); 
//         E.filename = NULL; 
//     }
//     E.filename = strdup(filename);

//     FILE *fp = fopen(filename, "r");
//     if (!fp) {
//         die("fopen"); // Use your defined die function
//     }

//     char *line = NULL;
//     size_t linecap = 0;
//     ssize_t linelen; // Use ssize_t for getline return value

//     while ((linelen = getline(&line, &linecap, fp)) != -1) {
//         while (linelen > 0 && (line[linelen - 1] == '\n' ||
//                                line[linelen - 1] == '\r'))
//             linelen--;

//         editorAppendRow(line, linelen);
//     }
//     free(line);
//     fclose(fp);

//     // 如果文件是完全空的（即没有任何内容被读入），也添加一个空行
//     if (E.numrows == 0) {
//         editorAppendRow("", 0); // 添加一个空行
//     }
// }