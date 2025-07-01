#ifndef RAWMODE_H
#define RAWMODE_H

#include <termios.h>
#include <unistd.h>

// 错误处理函数
void die(const char *s);

// 终端设置函数
void disableRawMode(void);
void enableRawMode(void);

// 特殊键枚举
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DEL_KEY
};

// 按键读取函数
int editorReadKey(void);

#endif