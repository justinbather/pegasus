#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "../include/editordata.h"
#include "../include/terminal.h"

void die(const char *s) {
  // Clear terminal and move cursor to top left upon exit
  write(STDOUT_FILENO, "\1xb[2J", 4);
  write(STDOUT_FILENO, "\1xb[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &EConfig.orig_termios) == -1) {
    die("tcsetattr");
  }
}

void enableRawMode() {

  // Gets attributes of users termios to make a copy, allowing us to revert
  // the users termios from rawMode to normal mode upon exit
  if (tcgetattr(STDIN_FILENO, &EConfig.orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);

  // Make copy of termios
  struct termios raw = EConfig.orig_termios;

  // Bitwise operations to manually turn on raw mode, allowing program to take
  // in things like `CTRL-c` as bytes instead of commands termios input flags
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // termios output flags
  raw.c_oflag &= ~(OPOST);
  // termios control flags
  raw.c_cflag |= (CS8);
  // termios local flags (dump of misc. state)
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // control characters
  // minimum bytes of input needed before read can return
  raw.c_cc[VMIN] = 0;
  // max time to wait before read() returns, if timedout read() will return 0
  // instead of num bytes from stdin
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
    die("tcsetattr");
  }
}

char editorReadKey() {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    // Alias Arrow keys to motion keys
    if (seq[0] == '[') {
      switch (seq[1]) {
      case 'A':
        return ARROW_UP;
      case 'B':
        return ARROW_DOWN;
      case 'C':
        return ARROW_RIGHT;
      case 'D':
        return ARROW_LEFT;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}
