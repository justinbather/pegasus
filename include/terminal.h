#ifndef TERMINAL_H
#define TERMINAL_H

void die(const char *s);

void disableRawMode();

void enableRawMode();

char editorReadKey();

int getCursorPosition(int *rows, int *cols);

int getWindowSize(int *rows, int *cols);
#endif
