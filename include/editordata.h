#ifndef EDITORDATA_H
#define EDITORDATA_H

#include <stdbool.h>
#include <termios.h>
#include <time.h>

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  // render stores characters to render into row eg. spaces instead of t (\t)
  char *render;
} erow;

struct editorConfig {
  // cursor x and y position indexes into rows and columns
  int cx, cy;
  // rx indexes into render
  int rx;
  int rowoffset;
  int coloffset;
  int screenrows;
  int screencols;
  int numrows;
  char statusmsg[80];
  time_t statusmsg_time;
  erow *row;
  char *filename;
  bool normalMode;
  int dirty;
  struct termios orig_termios;
};

struct editorConfig EConfig;

enum editorKey {
  ARROW_LEFT = 'h',
  ARROW_RIGHT = 'l',
  ARROW_UP = 'k',
  ARROW_DOWN = 'j',
  BACKSPACE = 127
};
#endif
