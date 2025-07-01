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
    }

    editorSetStatusMessage("HELP: Ctrl-Q = quit | Ctrl-S = save"); // Initial status message

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}