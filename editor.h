#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h> // For size_t
#include <termios.h> // For struct termios, already in rawmode.h, but good to be explicit
#include <time.h>    // For time_t

// 对 abuf 进行前向声明，以便 editorDrawRows 能够使用它
// 更好的做法是，如果它是一个通用的实用工具，直接在此处定义它。
// 由于 abuf 是 editorDrawRows 使用的通用缓冲区，
// 因此将其定义在 editor.h 中是有意义的。
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);

/// 定义Tab的宽度
#define KILO_TAB_STOP 8

/// erow结构体，存储每行文本的信息
/// @brief 从上到下依次是原始文本的长度，原始文本内容
/// @brief 渲染后的文本长度（考虑Tab展开），渲染后的文本内容
typedef struct erow {
    int size;
    char *chars;
    int rsize; 
    char *render;
} erow;


typedef struct editorConfig {
    int cx, cy; ///光标在屏幕上的x和y坐标
    int rowoff; /// 垂直滚动偏移量，表示当前显示的第一行的行号
    int coloff; /// 水平滚动偏移量，表示当前显示的第一列的列号
    int screenrows; /// 终端的行数
    int screencols; /// 终端的列数
    int numrows; /// 文件中的总行数
    erow *row; /// 指向 erow 结构体数组的指针，存储所有行内容
    char *filename; /// 当前打开的文件名
    char statusmsg[80]; /// 状态栏消息
    time_t statusmsg_time; /// 你好
    // struct termios orig_termios; // This is now managed by rawmode.c internally, no need here.
} editorConfig;

extern editorConfig E; // 声明全局编辑器配置

// --- Macros ---
#define CTRL_KEY(k) ((k) & 0x1f)

// --- Prototypes ---

// 初始化编辑器
void initEditor(void);

// 文本行操作：新增用于更新渲染行
void editorUpdateRow(erow *row);
void editorAppendRow(char *s, size_t len);


// 文本显示和绘制
void editorDrawRows(struct abuf *ab); // Now struct abuf is defined
void editorRefreshScreen(void);
void editorDrawStatusBar(struct abuf *ab);
void editorDrawMessageBar(struct abuf *ab);

// 光标和屏幕操作
void editorMoveCursor(int key);
void editorScroll(void);

// 输入处理
void editorProcessKeypress(void);

// 状态栏
void editorSetStatusMessage(const char *fmt, ...);

// 文件操作
void editorOpen(char *filename); // Add this prototype

#endif