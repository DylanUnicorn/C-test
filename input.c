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

///  region Search Functions
// 用于存储上一次搜索结果的静态变量
static char *last_query = NULL;
// static int last_query_len = 0;

// 清理上次的搜索高亮
void editorFindCleanup() {
    E.current_match_row = -1;
    E.current_match_col = -1;
    editorRefreshScreen(); // 刷新屏幕以移除高亮
}

// 核心的搜索查找函数，独立出来
static void editorFindDoSearch(void) {
    if (!E.query || strlen(E.query) == 0) {
        editorFindCleanup();
        return;
    }

    int found_match = 0;

    ///  添加更加严谨的搜索位置判断逻辑
    // 如果没有上次的搜索结果（或者第一次搜索），或者方向变了，则从光标位置开始搜索
    // 否则从上次匹配的下一个位置/上一个位置开始搜索
    int current_search_row = (E.current_match_row == -1) ? E.cy : E.current_match_row;
    size_t current_search_col = (E.current_match_col == (size_t)-1) ? E.cx : E.current_match_col;


    // // 如果是首次搜索或者改变了搜索方向，需要重新设置起始点
    // if (E.last_match_row == -1 || E.current_match_row == -1) {
    //     start_row = saved_cy; // 从进入搜索时的光标行开始
    //     start_col = saved_cx; // 从进入搜索时的光标列开始
    // } else {
    //     // 从上次匹配的下一个/上一个位置开始
    //     if (E.search_direction == 1) { // 向下
    //         start_col++;
    //         if (start_col >= E.row[start_row].size) { // 到行尾了，跳到下一行开头
    //             start_col = 0;
    //             start_row++;
    //         }
    //     } else { // 向上
    //         if (start_col == 0) { // 到行首了，跳到上一行末尾
    //             start_row--;
    //             if (start_row < 0) start_row = E.numrows - 1; // 循环到文件末尾
    //             if (start_row >= 0) start_col = E.row[start_row].size;
    //             else start_col = 0; // 如果文件为空
    //         } else {
    //             start_col--;
    //         }
    //     }
    // }
    // 调整搜索起始点以找到下一个/上一个匹配
    // 这确保了不会重复找到同一个匹配项
    if (E.current_match_row != -1) { // 只有在已经有匹配项的情况下才调整
        if (E.search_direction == 1) { // 向下搜索
            current_search_col++; // 从当前匹配项的下一个字符开始
            if (current_search_col > E.row[current_search_row].size) {
                current_search_col = 0; // 如果到行尾，跳到下一行开头
                current_search_row++;
            }
        } else { // 向上搜索
            if (current_search_col == 0) { // 如果在行首，跳到上一行末尾
                current_search_row--;
                // 如果是文件开头了，则循环到文件末尾。
                // 此时，需要将 current_search_col 设为新行的末尾。
                if (current_search_row < 0) current_search_row = E.numrows - 1;
                // 注意：如果 E.numrows == 0，则 current_search_row 会是 -1
                if (current_search_row >= 0) {
                    current_search_col = E.row[current_search_row].size;
                } else {
                    current_search_col = 0; // 文件为空的情况
                }
            } else {
                current_search_col--; // 从当前匹配项的前一个字符开始
            }
        }
    }

    // 循环搜索机制
    // int current_row = start_row;
    // int num_rows_to_check = E.numrows; // 最多检查所有行，防止无限循环
    // if (num_rows_to_check == 0) num_rows_to_check = 1; // 至少检查一次，防止空文件

    int initial_row = current_search_row;
    size_t initial_col = current_search_col;
    int loop_count = 0; // 用于防止无限循环

    for (loop_count = 0; loop_count < E.numrows + 1; loop_count++) { // 最多遍历 E.numrows + 1 次
        if (current_search_row < 0) { // 向上搜索到达文件开头，循环到文件末尾
            current_search_row = E.numrows - 1;
            // 对于循环，如果跳到了文件末尾，列应从行末尾开始（向上搜索）或行首开始（向下搜索）
            current_search_col = (E.search_direction == -1 && current_search_row >=0) ? E.row[current_search_row].size : 0;
        } else if (current_search_row >= E.numrows) { // 向下搜索到达文件末尾，循环到文件开头
            current_search_row = 0;
            current_search_col = 0;
        }

        // 如果文件是空的，直接退出
        if (E.numrows == 0) break;
        if (current_search_row < 0 || current_search_row >= E.numrows) break; // 确保行号有效

        erow *row = &E.row[current_search_row];
        char *match = NULL;
        size_t match_col = (size_t)-1;
        size_t query_len = strlen(E.query);

        // 如果行内容比查询短，则无法匹配
        if (row->size < query_len && query_len > 0) {
            // 如果是向上搜索，并且当前行不能匹配，且这是最后一行，则跳到上一行
            // 否则，如果向下搜索，且当前行不能匹配，跳到下一行
            if (E.search_direction == 1) {
                current_search_row++;
                current_search_col = 0; // 重置列到行首
            } else {
                current_search_row--;
                // 如果跳到上一行，列应该设为行末尾以继续向上搜索
                if (current_search_row >= 0) current_search_col = E.row[current_search_row].size;
                else current_search_col = 0; // 文件为空或到顶
            }
            continue; // 继续到下一轮循环
        }

        if (E.search_direction == 1) { // 向下搜索
            // 从当前列开始搜索
            if (current_search_col <= row->size) { // 确保起始列有效
                match = strstr(row->chars + current_search_col, E.query);
                if (match) {
                    match_col = current_search_col + (match - (row->chars + current_search_col));
                }
            } else { // 如果 current_search_col 已经超出当前行，从行首开始
                 match = strstr(row->chars, E.query);
                 if (match) match_col = match - row->chars;
            }
        } else { // 向上搜索 (从后往前找)
            // 从当前列（或行末尾）开始向左遍历
            size_t j_start = current_search_col;
            if (j_start > row->size) j_start = row->size; // 确保起始点在行内

            for (size_t j = j_start; ; j--) {
                if (j + query_len <= row->size) { // 确保子串不会越界
                    if (strncmp(&row->chars[j], E.query, query_len) == 0) {
                        match = &row->chars[j];
                        match_col = j;
                        break; // 找到最靠右的匹配项
                    }
                }
                if (j == 0) break; // 已经检查到行首
            }
        }

    // for (int i = 0; i < num_rows_to_check; i++) {
    //     if (current_row < 0) current_row = E.numrows - 1; // 向上循环到文件末尾
    //     if (current_row >= E.numrows) current_row = 0; // 向下循环到文件开头

    //     erow *row = &E.row[current_row];
    //     char *match = NULL;
    //     size_t match_col = (size_t)-1;

    //     // 搜索起点
    //     char *search_start_ptr = (current_row == start_row) ? (row->chars + start_col) : row->chars;
    //     size_t search_start_idx = (current_row == start_row) ? start_col : 0;

    //     if (E.search_direction == 1) { // 向下搜索
    //         // strstr 从 search_start_ptr 开始查找
    //         match = strstr(search_start_ptr, E.query);
    //         if (match) {
    //             match_col = search_start_idx + (match - search_start_ptr);
    //         }
    //     } else { // 向上搜索 (从后往前找)
    //         size_t query_len = strlen(E.query);
    //         if (query_len == 0) break; // 空查询直接跳过

    //         // 确定向上搜索的起始列
    //         size_t j_start = (current_row == start_row) ? search_start_idx : (size_t)row->size;
    //         if (j_start > (size_t)row->size) j_start = row->size; // 确保不超过行长

    //         for (size_t j = j_start; ; j--) {
    //             if (j + query_len <= (size_t)row->size) { // 确保子串不会越界
    //                 if (strncmp(&row->chars[j], E.query, query_len) == 0) {
    //                     match = &row->chars[j];
    //                     match_col = j;
    //                     break; // 找到最靠右的匹配项
    //                 }
    //             }
    //             if (j == 0) break; // 已经检查到行首
    //         }
    //     }

        if (match) {
            // 找到了匹配项
            E.current_match_row = current_search_row;
            E.current_match_col = match_col;
            E.cy = E.current_match_row;
            E.cx = E.current_match_col;
            editorScroll(); // 滚动到可见区域
            found_match = 1;
            // editorSetStatusMessage("Search: %s", E.query);
            break; // 找到一个匹配项就退出当前行的搜索
        }

        // 如果本行没有找到，移动到下一行/上一行继续搜索 (循环逻辑)
        if (E.search_direction == 1) {
            current_search_row++;
            current_search_col = 0; // 重置列到行首
        } else {
            current_search_col--;
            /// 从最新的末尾向上搜索
            if(current_search_row >= 0) current_search_col = E.row[current_search_row].size;
            else current_search_col = 0; // 文件到顶
        }

        if (!found_match) {
            editorSetStatusMessage("Search: \"%s\" (Not found)", E.query);
            editorFindCleanup(); // 清除任何可能存在的旧高亮
            E.current_match_row = -1; // 重置当前匹配
            E.current_match_col = (size_t)-1; // 重置当前匹配列
        }

        // 重置列的搜索起点为行首/行尾，因为换行了
        // start_col = (E.search_direction == 1) ? 0 : E.row[current_row].size;
    }


}

void editorFind() {
    // 1. 保存当前光标位置，以便搜索结束后可以返回
    int original_cx = E.cx;
    int original_cy = E.cy;
    int original_rowoff = E.rowoff;
    int original_coloff = E.coloff;

    // 2. 记住上一次的搜索词和方向
    if (last_query != NULL) {
        // 如果有上次搜索词，则将其设置为当前的 E.query，以便重新开始搜索时显示
        if (E.query) free(E.query);
        E.query = strdup(last_query); // 复制上次的搜索词
    } else {
        // 确保 E.query 在没有上次搜索时是 NULL
        if (E.query) { free(E.query); E.query = NULL; }
    }

    // 重置上次匹配和当前高亮，确保每次进入搜索模式都是从头开始查找
    // 只有在输入字符或者首次进入搜索时才完全重置
    // 如果是再次进入，则保留 current_match_row/col 作为起始点
    if (E.query == NULL || strlen(E.query) == 0 || E.current_match_row == -1) {
        E.last_match_row = -1;
        E.last_match_col = (size_t)-1;
        E.current_match_row = -1;
        E.current_match_col = (size_t)-1;
    }
    // E.last_match_row = -1; // 重置上次匹配
    // E.last_match_col = (size_t)-1;
    // E.current_match_row = -1;
    // E.current_match_col = (size_t)-1; // -1 for size_t
    E.search_direction = 1; // 默认向下搜索

    // 3. 进入搜索模式循环
    // char *query_input;
    // 确保 editorPrompt 能够显示并处理上次的搜索词作为初始值
    // editorPrompt 已经有 %s 格式化，但需要它在内部将 E.query 设为提示的初始值
    // 这里我们先清空 E.statusmsg，然后直接用 E.query (last_query) 来构建提示
    editorSetStatusMessage("Search: %s (Use Arrows for next/prev, Enter to confirm, Esc to cancel)", E.query ? E.query : "");

    // 搜索循环
    int c;
    // int current_row_search_start = E.cy; // 从当前光标所在行开始搜索
    // int current_col_search_start = E.cx; // 从当前光标所在列开始搜索
    int saved_cx = E.cx; // 保存当前光标位置作为搜索起点
    int saved_cy = E.cy;

    // 如果刚进入搜索模式就有查询词，执行一次初始搜索
    if (E.query && strlen(E.query) > 0) {
        editorFindDoSearch(); // 执行初始搜索
    }

    while (1) {
        editorRefreshScreen(); // 实时刷新屏幕

        c = editorReadKey(); // 读取用户输入

        int perform_search = 0; // 标志是否需要执行一次新的搜索查找

        // 处理搜索导航键
        if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            E.search_direction = 1; // 向下搜索
            perform_search = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            E.search_direction = -1; // 向上搜索
            perform_search = 1;
        } else if (c == '\r') { // Enter 确认搜索，退出
            // 搜索结束，清除状态栏消息，但保留高亮和光标位置
            editorSetStatusMessage("");
            break;
        } else if (c == '\x1b') { // Esc 退出搜索，恢复光标
            editorSetStatusMessage("");
            E.cx = original_cx; // 恢复光标位置
            E.cy = original_cy;
            E.rowoff = original_rowoff;
            E.coloff = original_coloff;
            editorFindCleanup(); // 清除高亮
            // 释放当前搜索词，但不清除 last_query
            if (E.query) { free(E.query); E.query = NULL; }
            return;
        } else if (c == DEL_KEY || c == CTRL_KEY('h') || c == '\x7f') { // Backspace
            if (E.query && strlen(E.query) > 0) {
                E.query[strlen(E.query) - 1] = '\0'; // 删除最后一个字符
                // 重新开始搜索以更新高亮
                // editorFindCleanup(); // 清除上次的高亮
                // E.last_match_row = -1;
                // E.last_match_col = (size_t)-1;
                // E.search_direction = 1; // 恢复默认搜索方向
                // // E.current_match_row = -1; // 重置当前高亮
                // // E.current_match_col = -1;
                // saved_cy = E.cy; // 重新从当前光标开始
                // saved_cx = E.cx;
            } else {
                // 如果搜索词为空且按了退格，清空 query 并跳过后续处理
                if (E.query) { free(E.query); E.query = NULL; }
                editorFindCleanup(); // 清除旧高亮
                editorSetStatusMessage("Search: (empty query)");
                continue;
            }
            editorFindCleanup();
            E.current_match_row = -1; // 重置当前高亮，确保从当前光标开始新的增量搜索
            E.current_match_col = (size_t)-1;
            perform_search = 1; // 字符改变
        } else if (!iscntrl(c) && c < 128) { // 普通字符输入
            // 追加字符到搜索词
            size_t current_query_len = E.query ? strlen(E.query) : 0;
            E.query = realloc(E.query, current_query_len + 2); // +1 for char, +1 for null
            if (E.query == NULL) die("realloc query");
            E.query[current_query_len] = c;
            E.query[current_query_len + 1] = '\0';

            // 每次输入字符后，重置搜索起点和上次匹配，以便从头开始新的增量搜索
            editorFindCleanup();
            E.last_match_row = -1;
            E.last_match_col = (size_t)-1;
            E.search_direction = 1;
            // E.current_match_row = -1; // 重置当前高亮
            // E.current_match_col = -1;
            // saved_cy = E.cy; // 重新从当前光标开始
            // saved_cx = E.cx;
            perform_search = 1; // 字符改变
        } else {
            // 忽略其他按键（例如 Ctrl 键等，因为它们不是搜索输入或导航键）
            continue;
        }


        if(perform_search) {
            // 执行搜索查找逻辑
            editorFindDoSearch();
        } else if (!E.query || strlen(E.query) == 0) {
            // 如果没有搜索词，清除高亮并退出搜索
            editorFindCleanup();
            editorSetStatusMessage("Search: (empty query)");
            continue; // 继续等待输入
        }
    }

    if(E.query) {
        // 退出搜索模式后，清除上次的搜索词
        if(last_query) {
            free(last_query); // 释放上次的搜索词
        }
        last_query = strdup(E.query); // 保存当前搜索词作为上次搜索词
    } else {
        if(last_query) {
            free(last_query); // 如果没有搜索词，释放上次的搜索词
            last_query = NULL;
        }
    }

    editorFindCleanup(); /// 进一步清除可能的高亮

    //     // 4. 执行搜索逻辑
    //     if (E.query && strlen(E.query) > 0) {
    //         int found = 0;
    //         int current_row = current_row_search_start;
    //         int current_col = current_col_search_start;
    //         int start_row_for_loop = E.search_direction == 1 ? current_row : current_row; // 循环起始行
    //         int start_col_for_loop = E.search_direction == 1 ? current_col : current_col; // 循环起始列

    //         // 循环搜索机制
    //         for (int i = 0; i < E.numrows; i++) {
    //             current_row = (start_row_for_loop + E.search_direction * i + E.numrows) % E.numrows;

    //             erow *row = &E.row[current_row];
    //             char *match = NULL;

    //             if (E.search_direction == 1) { // 向下搜索
    //                 // 从指定列开始搜索
    //                 match = strstr(row->chars + (current_row == start_row_for_loop ? start_col_for_loop : 0), E.query);
    //             } else { // 向上搜索 (需要从后往前找)
    //                 size_t query_len = strlen(E.query); // 获取查询字符串长度，类型是 size_t
    //                 if (query_len == 0) continue; // 避免空查询导致问题

    //                 // 如果行太短无法包含查询，直接跳过
    //                 if ((size_t)row->size < query_len) continue;

    //                 // 对于向上搜索，需要遍历所有可能的起始点
    //                 for (size_t j = row->size - query_len; ; j--) {
    //                     // 确保 j + strlen(E.query) 不会越界
    //                     if (j + query_len <= (size_t)row->size) {
    //                         if (strncmp(&row->chars[j], E.query, query_len) == 0) {
    //                             match = &row->chars[j];
    //                             break; // 找到最靠右的匹配项
    //                         }
    //                     }
    //                     // 如果 j 减到 0，且 j + query_len 仍然小于 query_len (即 query_len 大于 row->size)
    //                     // 这种情况不可能匹配，因为查询字符串比行还长
    //                     if (j == 0) break; // 防止 size_t j-- 减到负数变成一个大正数，导致无限循环
    //                 }
    //             }

    //             if (match) {
    //                 E.last_match_row = current_row;
    //                 E.last_match_col = match - row->chars;

    //                 // 设置当前高亮匹配项并移动光标和视图
    //                 E.current_match_row = E.last_match_row;
    //                 E.current_match_col = E.last_match_col;
    //                 E.cy = E.current_match_row;
    //                 E.cx = E.current_match_col;
    //                 E.rowoff = E.numrows; // 强制滚动，确保匹配项可见 (临时设置为一个大值)
    //                 editorScroll();       // 调用 scroll 来更新 rowoff, coloff
    //                 found = 1;
    //                 break; // 找到一个匹配项就停止，以便用户按方向键继续
    //             }
    //         }

    //         if (!found) {
    //             editorSetStatusMessage("Search: \"%s\" (Not found)", E.query);
    //             editorFindCleanup(); // 清除任何可能存在的旧高亮
    //         } else {
    //              editorSetStatusMessage("Search: %s", E.query);
    //         }
    //     } else {
    //          editorFindCleanup(); // 搜索词为空时清除高亮
    //          editorSetStatusMessage("Search: (empty query)");
    //     }
    //     // 对于每次搜索操作，更新起始行和列，以便下次搜索可以从当前光标位置继续
    //     current_row_search_start = E.cy;
    //     current_col_search_start = E.cx;

    //     // 保存当前有效的搜索词到 last_query
    //     if (E.query) {
    //         if (last_query) free(last_query);
    //         last_query = strdup(E.query);
    //         last_query_len = strlen(last_query);
    //     } else {
    //         if (last_query) { free(last_query); last_query = NULL; }
    //         last_query_len = 0;
    //     }
    // }
    // 退出搜索模式后，清除上次的搜索词
    // if (E.query) { free(E.query); E.query = NULL; } // 重新进入会复制 last_query
}
/// endregion Search Functions

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

        case CTRL_KEY('f'): // Ctrl-F：进入搜索模式
            editorFind();
            break;

        default:
            // 只处理可打印字符和制表符作为普通字符插入
            if (!iscntrl(c) || c == '\t') {
                editorInsertChar(c);
            }
            break;
    }
    quit_times = 3; // 每次进行操作后重置退出确认计数
}