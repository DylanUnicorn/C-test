#include "editor.h" 
#include <stdlib.h> // For realloc, free
#include <string.h> // For memcpy

// 将字符串添加到动态缓冲区
void abAppend(struct abuf *ab, const char *s, int len) {
    char *new_buf = realloc(ab->b, ab->len + len);

    if (new_buf == NULL) {
        die("realloc failed in abAppend"); // 使用全局的 die 函数
    }
    ab->b = new_buf;
    memcpy(&new_buf[ab->len], s, len);
    ab->len += len;
}

// 释放动态缓冲区内存
void abFree(struct abuf *ab) {
    free(ab->b);
    ab->b = NULL;
    ab->len = 0;
}