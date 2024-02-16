#ifndef EDITORDATA_H
#define EDITORDATA_H

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  // render stores characters to render into row eg. spaces instead of t (\t)
  char *render;
} erow;

#endif
