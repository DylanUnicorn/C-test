#include "editor.h" // 包含全局头文件
#include <stdio.h>  // For fopen, getline, fwrite, fclose
#include <stdlib.h> // For malloc, free
#include <string.h> // For memcpy, strlen, strdup, strerror
#include <errno.h>  // For errno, ENOENT
#include <unistd.h> // For write


// 将所有行的内容拼接成一个字符串
char *editorRowsToString(int *buflen) {
    int totlen = 0;
    for (int i = 0; i < E.numrows; i++) {
        totlen += E.row[i].size + 1; // +1 for newline character
    }
    *buflen = totlen;

    char *buf = malloc(totlen);
    if (buf == NULL) die("malloc rows to string buffer");
    char *p = buf;
    for (int i = 0; i < E.numrows; i++) {
        memcpy(p, E.row[i].chars, E.row[i].size);
        p += E.row[i].size;
        *p = '\n'; // Add newline at the end of each row
        p++;
    }
    return buf;
}

// 打开文件
void editorOpen(char *filename) {
    // 释放旧的文件名（如果有的话）
    if (E.filename) {
        free(E.filename);
        E.filename = NULL;
    }
    E.filename = strdup(filename); // 复制文件名

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        // 如果文件不存在或无法打开，并且编辑器当前没有行数据（表示是新文件）
        if (errno == ENOENT && E.numrows == 0) { // ENOENT: No such file or directory
            editorInsertRow(0, "Hello world!", strlen("Hello world!")); // 添加一个空行
            E.dirty = 0; // 新创建的空文件，不是脏的
            return;
        }
        die("fopen"); // 对于其他文件打开错误，直接退出
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    // 清理现有行（如果已经有打开的文件）
    editorFreeAllRows(); // 确保在读取新文件前清空旧数据

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        // 移除行尾的换行符和回车符
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(E.numrows, line, linelen); // 将读取到的行添加到编辑器数据中
    }
    free(line); // 释放 getline 分配的缓冲区
    fclose(fp);

    // 如果文件是完全空的（即没有任何内容被读入），也添加一个空行
    if (E.numrows == 0) {
        editorInsertRow(0, "", 0); // 添加一个空行
    }
    E.dirty = 0; // 文件加载完成后，重置脏标志
}

// 保存文件
void editorSave() {
    if (E.filename == NULL || strcmp(E.filename, "[No Name]") == 0) { // 如果没有文件名（新建文件）或文件名是默认的 "[No Name]"
        char *new_filename = editorPrompt("Save as: %s (ESC to cancel)");
        if (new_filename == NULL) { // 用户取消保存
            editorSetStatusMessage("Save aborted.");
            return;
        }
        if (E.filename) free(E.filename); // 释放旧的 "[No Name]"
        E.filename = new_filename;
    }

    int len;
    char *buf = editorRowsToString(&len); // 将所有行转换为一个字符串

    // 以只写、创建、截断的方式打开文件。使用 "w" 模式，它会创建文件（如果不存在）并清空（如果存在）。
    FILE *fp = fopen(E.filename, "w");
    if (fp == NULL) {
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
        free(buf);
        return;
    }

    if (fwrite(buf, 1, len, fp) == (size_t)len) { // 写入文件
        fclose(fp);
        free(buf);
        E.dirty = 0; // 保存成功，清除脏标志
        editorSetStatusMessage("%d bytes written to %s", len, E.filename);
    } else {
        editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
        free(buf);
        fclose(fp);
    }
}