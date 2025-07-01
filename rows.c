#include "editor.h" // 包含全局头文件
#include <stdlib.h> // For malloc, realloc, free
#include <string.h> // For memcpy, memmove

// 释放单个 erow 的内存
void editorFreeRow(erow *row) {
    free(row->chars);
    free(row->render);
}

// 释放所有行数据及其关联内存
void editorFreeAllRows(void) {
    for (int i = 0; i < E.numrows; i++) {
        editorFreeRow(&E.row[i]);
    }
    free(E.row);
    E.row = NULL;
    E.numrows = 0;
}

// 更新行的渲染内容（处理 Tab）
void editorUpdateRow(erow *row) {
    int tabs = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') tabs++;
    }

    free(row->render); // 释放旧的渲染内存
    row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);
    if (row->render == NULL) die("malloc render");

    int idx = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            do {
                row->render[idx++] = ' ';
            } while (idx % KILO_TAB_STOP != 0); // 确保对齐到 Tab 停止位
        } else {
            row->render[idx++] = row->chars[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

// 向编辑器中插入一个新行
void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return; // 检查插入位置的有效性

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    if (E.row == NULL) die("realloc row");

    // 移动现有行以腾出空间
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL) die("malloc chars");
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL; // 初始化为 NULL，editorUpdateRow会分配
    editorUpdateRow(&E.row[at]); // 更新渲染后的行

    E.numrows++;
    E.dirty++; // 文件已修改
}

// 删除指定位置的行
void editorDeleteRow(int at) {
    if (at < 0 || at >= E.numrows) return;

    editorFreeRow(&E.row[at]); // 释放该行的内存

    // 移动后续行以覆盖被删除的行
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++; // 文件已修改
}

// 在指定行的指定位置插入字符
void editorInsertCharInRow(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size; // 确保插入位置有效

    // 重新分配内存以容纳新字符和 null 终止符
    char *new_chars = realloc(row->chars, row->size + 2); // +1 for new char, +1 for null terminator
    if (new_chars == NULL) die("realloc chars for insert");
    row->chars = new_chars; // 更新指针
    // row->chars = realloc(row->chars, row->size + 2); // +1 for new char, +1 for null terminator
    // if (row->chars == NULL) die("realloc chars for insert");

    // 移动 `at` 位置及其后的字符，为新字符腾出空间
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1); // +1 for null terminator
    row->chars[at] = c; // 插入新字符
    row->size++; // 更新原始长度

    editorUpdateRow(row); // 更新渲染行
    E.dirty++; // 文件已修改
}

// 在指定行的指定位置删除字符
void editorDeleteCharInRow(erow *row, int at) {
    if (at < 0 || at >= row->size) return; // 检查删除位置的有效性

    // 移动 `at` 位置之后的字符，覆盖被删除的字符
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--; // 更新原始长度
    row->chars = realloc(row->chars, row->size + 1); // 重新分配内存，减少空间
    if (row->chars == NULL && row->size > 0) die("realloc chars for delete"); // Only die if it's not an empty row

    editorUpdateRow(row); // 更新渲染行
    E.dirty++; // 文件已修改
}