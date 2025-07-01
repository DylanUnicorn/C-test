#include "editor.h" // This now includes all necessary prototypes
#include "rawmode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strlen, etc. (though not strictly needed in main, good practice if using strings)

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    } else {
        /// 没有命令行参数，显示"Hello, World!"作为默认内容
        editorAppendRow("Hello, World!", strlen("Hello, World!"));
    }

    editorSetStatusMessage("HELP: Ctrl-Q = quit | Ctrl-S = save"); // 初始化状态栏消息

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}