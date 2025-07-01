#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h> // For size_t
#include <termios.h> // For struct termios, already in rawmode.h, but good to be explicit
#include <time.h>    // For time_t

/// 定义Tab的宽度
#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_TAB_STOP 8

/// 全局处理宏
extern void die(const char *s);

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf *ab, const char *s, int len);
void abFree(struct abuf *ab);



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
    int dirty;
    // struct termios orig_termios; // This is now managed by rawmode.c internally, no need here.
} editorConfig;

extern editorConfig E; // 声明全局编辑器配置


// core
void initEditor(void);
void editorRefreshScreen(void);
void editorScroll(void);
void editorMoveCursor(int key);

// raw
void editorUpdateRow(erow *row);
void editorInsertRow(int at, char *s, size_t len);
void editorDeleteRow(int at);
void editorInsertCharInRow(erow *row, int at, int c);
void editorDeleteCharInRow(erow *row, int at);
void editorFreeAllRows(void); // 新增：释放所有行内存
// void editorAppendRow(char *s, size_t len);

/// input
void editorProcessKeypress(void);
char *editorPrompt(const char *prompt, ...); 

/// file
char *editorRowsToString(int *buflen);
void editorOpen(char *filename);
void editorSave(void);

// 文本显示和绘制
// void editorDrawRows(struct abuf *ab); // Now struct abuf is defined

/// status
void editorDrawStatusBar(struct abuf *ab);
void editorDrawMessageBar(struct abuf *ab);
void editorSetStatusMessage(const char *fmt, ...);

// rawmode.h 中定义的函数（由 rawmode.c 实现）
extern int editorReadKey(void);
extern void enableRawMode(void);
extern void disableRawMode(void);

#endif