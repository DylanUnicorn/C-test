#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

// 保存原始终端设置
struct termios orig_termios;

// 错误处理函数
void die(const char *s) {
    perror(s);
    exit(1);
}

// 禁用原始模式，恢复终端原始设置
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

// 启用原始模式
void enableRawMode() {
    // 获取当前终端设置
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) 
        die("tcgetattr");
    
    // 注册退出时恢复原始模式的函数
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    
    // 修改输入模式标志
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);
    
    // 修改输出模式标志
    raw.c_oflag &= ~(OPOST);
    
    // 修改控制模式标志
    raw.c_cflag |= (CS8);
    
    // 修改本地模式标志
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // 设置输入超时和最小字符数
    raw.c_cc[VMIN] = 0;  // 最小读取字符数
    raw.c_cc[VTIME] = 10; // 超时时间(十分之一秒)
    
    // 应用新的终端设置
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

int main() {
    // 启用原始模式
    enableRawMode();
    
    printf("原始模式已启用。按'q'退出。\n");
    printf("按下任意键查看ASCII码...\n");
    
    while (1) {
        char c = '\0';
        
        // 读取一个字符
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
            die("read");
        
        // 检查是否按下了'q'
        if (c == 'q') break;
        
        // 显示按键信息
        if (iscntrl(c)) {
            // 控制字符
            printf("%d\r\n", c);
        } else {
            // 可打印字符
            printf("%d ('%c')\r\n", c, c);
        }
    }
    
    return 0;
}