#include "editor.h" // Include editor.h first to ensure its definitions are picked up
#include "rawmode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h> // For va_list, va_start, va_end
#include <unistd.h> // For write, read
#include <sys/ioctl.h> // For ioctl, TIOCGWINSZ
#include <time.h> // For time_t, time
#include <errno.h> // For EAGAIN etc.

// Global editor configuration instance
editorConfig E;

// --- Append Buffer (already moved to editor.h for visibility) ---
// struct abuf {
//     char *b;
//     int len;
// };


void abAppend(struct abuf *ab, const char *s, int len) { // Moved to editor.h
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) 
    {
        perror("realloc failed in abAppend");
        exit(1);
        // return;
    }
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) { // Moved to editor.h
    free(ab->b);
    ab->b = NULL;
    ab->len = 0;
}

// --- Editor Initialization ---

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    // Get terminal size
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // Fallback: Try to get cursor position using ANSI escape sequence
        // This is a less reliable fallback, but better than nothing.
        // It requires the terminal to respond, and you're in raw mode.
        // It should already be handled by the rawmode.c's editorReadKey for '\x1b'
        // For simplicity, for initial setup, let's assume a default size if ioctl fails.
        // Or, you can add a `getWindowSize` helper function.
        // For now, let's just use a default or try the specific escape sequence.
        
        // This part needs to be careful: if ioctl fails, and we try to read,
        // it might block indefinitely if the terminal doesn't respond.
        // For robustness, consider a timeout on `read` if you go this route.
        // For now, let's just use default if ioctl fails.
        // write(STDOUT_FILENO, "\x1b[999C\x1b[999B\x1b[6n", 18); // Move to bottom right and query cursor pos
        // char buf[32];
        // unsigned int i = 0;
        // // This read could block if no response, consider a non-blocking read or timeout
        // if (read(STDIN_FILENO, buf, sizeof(buf) - 1) != 1) { // Read response
        //     E.screenrows = 24; // Default
        //     E.screencols = 80; // Default
        //     return;
        // }
        // buf[i] = '\0';
        // if (buf[0] == '\x1b' && buf[1] == '[') {
        //     if (sscanf(&buf[2], "%d;%d", &E.screenrows, &E.screencols) != 2) {
        //         E.screenrows = 24;
        //         E.screencols = 80;
        //     }
        // }
        // write(STDOUT_FILENO, "\x1b[H", 3); // Move cursor back to top-left

        // For simplicity, if ioctl fails, set a default size.
        E.screenrows = 24;
        E.screencols = 80;
    } else {
        E.screencols = ws.ws_col;
        E.screenrows = ws.ws_row;
    }
    // Subtract space for status bar and message bar
    E.screenrows -= 2;
}

// --- Editor Drawing ---

// Draw each row of text to the buffer
void editorDrawRows(struct abuf *ab) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = E.rowoff + y;
        if (filerow >= E.numrows) {
            // If it's an empty line, display welcome message or tilde
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Kilo editor -- version %d.%d.%d", 0, 0, 1); // Replace with your version
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            // Draw file content
            int len = E.row[filerow].size - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].chars[E.coloff], len);
        }

        // Clear to end of line
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

// Draw status bar
void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4); // Invert colors
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.numrows,
        ""); // Add dirty flag later
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d",
        E.cy + 1, E.numrows);

    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3); // Reset colors
    abAppend(ab, "\r\n", 2);
}

// Draw message bar
void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3); // Clear current line
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5) { // Message displays for 5 seconds
        abAppend(ab, E.statusmsg, msglen);
    }
}

// Refresh screen
void editorRefreshScreen() {
    editorScroll(); // Process scrolling before refreshing

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
    abAppend(&ab, "\x1b[H", 3);    // Move cursor to top-left (1,1)

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    // Position cursor at cx, cy
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                            (E.cx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6); // Show cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

// Set status bar message
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

// --- Scrolling ---
void editorScroll() {
    // Vertical scrolling
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }

    // Horizontal scrolling
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cx - E.screencols + 1;
    }
}

// --- Cursor Movement ---

void editorMoveCursor(int key) {
    // Ensure cursor doesn't go past end of row, otherwise it could lead to illegal memory access
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) { // If at start of line, move to end of previous line
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) { // Ensure cursor doesn't exceed current line's char length
                E.cx++;
            } else if (row && E.cx == row->size && E.cy < E.numrows - 1) { // If at end of line, move to start of next line
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy > 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) { // Can move to one line past the end of file (for new line input)
                E.cy++;
            }
            break;
        case PAGE_UP: {
            int times = E.screenrows;
            while (times--) {
                editorMoveCursor(ARROW_UP);
            }
            break;
        }
        case PAGE_DOWN: {
            int times = E.screenrows;
            while (times--) {
                editorMoveCursor(ARROW_DOWN);
            }
            break;
        }
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (row) {
                E.cx = row->size;
            }
            break;
    }

    // After cursor movement, prevent cx from exceeding the actual length of the current row
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

// --- Input Processing ---
void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case '\r': // Enter key (for future text editing)
            break;

        case CTRL_KEY('q'): // Ctrl-Q to quit
            write(STDOUT_FILENO, "\x1b[2J", 4); // Clear screen
            write(STDOUT_FILENO, "\x1b[H", 3);  // Move cursor home
            exit(0);
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
        
        case DEL_KEY:
            // Handle delete key
            break;
    }
}

// --- File Operations ---
// This is where you would load/save files.
// For now, we'll just add a dummy line.
void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        die("fopen"); // Use your defined die function
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen; // Use ssize_t for getline return value

    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;

        // For now, we'll just add a simple row
        E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
        if (E.row == NULL) die("realloc");

        E.row[E.numrows].size = linelen;
        E.row[E.numrows].chars = malloc(linelen + 1);
        if (E.row[E.numrows].chars == NULL) die("malloc");
        memcpy(E.row[E.numrows].chars, line, linelen);
        E.row[E.numrows].chars[linelen] = '\0';
        E.numrows++;
    }
    free(line);
    fclose(fp);
}