
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../include/editordata.h"
#include "../include/terminal.h"

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define INSERT_KEY 'i'
#define ESC_KEY 27
#define PEGASUS_VERSION "0.0.1"
#define PEGASUS_TAB_SPACES 8
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

/*** row operations ***/

int editorRowCxtoRx(erow *row, int cx) {
  int rx = 0;

  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t') {
      rx += (PEGASUS_TAB_SPACES - 1) - (rx % PEGASUS_TAB_SPACES);
    }
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;

  // count num tabs in line/row
  for (j = 0; j < row->size; j++) {

    if (row->chars[j] == '\t')
      tabs++;
  }

  free(row->render);
  // account for spaces tabs will take up
  row->render = malloc(row->size + tabs * (PEGASUS_TAB_SPACES - 1) + 1);

  int idx = 0;

  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      // add 8 spaces in place of tab character
      while (idx % PEGASUS_TAB_SPACES != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
  EConfig.row = realloc(EConfig.row, sizeof(erow) * (EConfig.numrows + 1));

  // current row being rendered
  int at = EConfig.numrows;

  EConfig.row[at].size = len;
  EConfig.row[at].chars = malloc(len + 1);
  memcpy(EConfig.row[at].chars, s, len);
  EConfig.row[at].chars[len] = '\0';

  EConfig.row[at].rsize = 0;
  EConfig.row[at].render = NULL;

  editorUpdateRow(&EConfig.row[at]);

  EConfig.numrows++;
  EConfig.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
}

void editorDelRow(int at) {
  if (at < 0 || at >= EConfig.numrows)
    return;

  editorFreeRow(&EConfig.row[at]);
  memmove(&EConfig.row[at], &EConfig.row[at + 1],
          sizeof(erow) * (EConfig.numrows - at - 1));
  EConfig.numrows--;
  EConfig.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size)
    at = row->size;

  // add 2 bytes to size of chars to make room for null byte
  row->chars = realloc(row->chars, row->size + 2);

  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);

  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  EConfig.dirty++;
}

void editorRowAppendText(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  EConfig.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size)
    return;

  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  EConfig.dirty++;
}

/*** editor operations ***/

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(EConfig.statusmsg, sizeof(EConfig.statusmsg), fmt, ap);
  va_end(ap);
  EConfig.statusmsg_time = time(NULL);
}

void editorInsertChar(int c) {
  if (EConfig.cy == EConfig.numrows) {
    editorAppendRow("", 0);
  }

  editorRowInsertChar(&EConfig.row[EConfig.cy], EConfig.cx, c);
  EConfig.cx++;
}

void editorDelChar() {
  if (EConfig.cy == EConfig.numrows)
    return;
  if (EConfig.cx == 0 && EConfig.cy == 0)
    return;

  erow *row = &EConfig.row[EConfig.cy];
  if (EConfig.cx > 0) {
    editorRowDelChar(row, EConfig.cx - 1);
    EConfig.cx--;
  } else {
    EConfig.cx = EConfig.row[EConfig.cy - 1].size;
    editorRowAppendText(&EConfig.row[EConfig.cy - 1], row->chars, row->size);
    editorDelRow(EConfig.cy);
    EConfig.cy--;
  }
}

void editorNewRow() {}

/*** file i/o ***/

char *editorRowsToString(int *buflen) {
  int totlen = 0;

  for (int j = 0; j < EConfig.numrows; j++) {
    totlen += EConfig.row[j].size + 1;
  }

  *buflen = totlen;

  char *buf = malloc(totlen);
  char *p = buf;

  for (int k = 0; k < EConfig.numrows; k++) {
    memcpy(p, EConfig.row[k].chars, EConfig.row[k].size);
    p += EConfig.row[k].size;
    *p = '\n';
    p++;
  }

  return buf;
}

void editorOpen(char *filename) {
  free(EConfig.filename);
  EConfig.filename = strdup(filename);
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
  EConfig.dirty = 0;
}

void editorSave() {
  if (EConfig.filename == NULL)
    return;

  int len;
  char *buf = editorRowsToString(&len);

  int fd = open(EConfig.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        editorSetStatusMessage("%d bytes written to disk", len);
        EConfig.dirty = 0;
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Error Writing to Disk! error: %s", strerror(errno));
}

/*** append buffer **/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len) {
  // request a larger block of memory to add a string of size n to our
  // exisiting struct in memory so we can copy the given string to the end of
  // our current buffer data if realloc cant add to our exisiting block of
  // memory it will free our existing block and find a new one of size n +
  // ab->len
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;

  // copys new string to end of our buffer
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/*** input ***/

void editorToggleNormalMode() { EConfig.normalMode = !EConfig.normalMode; }

void editorMoveCursor(char key) {

  erow *row = (EConfig.cy >= EConfig.numrows) ? NULL : &EConfig.row[EConfig.cy];
  switch (key) {
    // NOTEConfig. Maybe change the naming of the enum constants later
  case ARROW_UP:
    if (EConfig.cy != 0) {
      EConfig.cy--;
    }
    break;
  case ARROW_DOWN:
    if (EConfig.cy < EConfig.numrows) {
      EConfig.cy++;
    }
    break;
  case ARROW_LEFT:
    if (EConfig.cx != 0) {
      EConfig.cx--;
    } else if (EConfig.cy > 0) {
      EConfig.cy--;
      EConfig.cx = EConfig.row[EConfig.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && EConfig.cx < row->size) {
      EConfig.cx++;
    } else if (row && EConfig.cx == row->size) {
      EConfig.cy++;
      EConfig.cx = 0;
    }
    break;
  }

  row = (EConfig.cy >= EConfig.numrows) ? NULL : &EConfig.row[EConfig.cy];
  int rowlen = row ? row->size : 0;
  if (EConfig.cx > rowlen) {
    EConfig.cx = rowlen;
  }
}

void editorProcessKeypress() {
  char c = editorReadKey();

  if (EConfig.normalMode == true) {
    switch (c) {
    case '\r':
      break;
    case BACKSPACE:
    case CTRL_KEY('h'):
      break;

    case CTRL_KEY('s'):
      editorSave();
      break;

    // Quit key
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
      // TODO: Add macro def for entering insert mode with 'i' to start
    case INSERT_KEY:
      editorToggleNormalMode();
      break;
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_UP:
    case ARROW_DOWN:
      editorMoveCursor(c);
      break;
    }
  } else if (EConfig.normalMode == false) {
    // INSERT MODE
    switch (c) {

    case ESC_KEY:
      editorToggleNormalMode();
    case BACKSPACE:
    case CTRL_KEY('h'):
      editorDelChar();
      break;
    case '\r':
      editorNewRow();
    default:
      editorInsertChar(c);
    }
  }
}

/*** motions ***/

/*** output ***/

void editorScroll() {
  EConfig.rx = 0;
  if (EConfig.cy < EConfig.numrows) {
    EConfig.rx = editorRowCxtoRx(&EConfig.row[EConfig.cy], EConfig.cx);
  }

  if (EConfig.cy < EConfig.rowoffset) {
    EConfig.rowoffset = EConfig.cy;
  }

  if (EConfig.cy >= EConfig.rowoffset + EConfig.screenrows) {
    EConfig.rowoffset = EConfig.cy - EConfig.screenrows + 1;
  }

  if (EConfig.rx < EConfig.coloffset) {
    EConfig.coloffset = EConfig.rx;
  }
  if (EConfig.rx >= EConfig.coloffset + EConfig.screencols) {
    EConfig.coloffset = EConfig.rx - EConfig.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {

  int y;
  for (y = 0; y < EConfig.screenrows; y++) {
    int filerow = y + EConfig.rowoffset;
    if (filerow >= EConfig.numrows) {
      if (EConfig.numrows == 0 && y == EConfig.screenrows / 3) {
        char welcome[80];
        int welcomelen =
            snprintf(welcome, sizeof(welcome), "Pegasus editor -- version %s",
                     PEGASUS_VERSION);
        if (welcomelen > EConfig.screencols)
          welcomelen = EConfig.screencols;
        int padding = (EConfig.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = EConfig.row[filerow].rsize - EConfig.coloffset;
      if (len < 0)
        len = 0;
      if (len > EConfig.screencols)
        len = EConfig.screencols;
      abAppend(ab, &EConfig.row[filerow].render[EConfig.coloffset], len);
    }
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     EConfig.filename ? EConfig.filename : "[No Name]",
                     EConfig.numrows, EConfig.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", EConfig.cy + 1,
                      EConfig.numrows);
  if (len > EConfig.screencols)
    len = EConfig.screencols;
  abAppend(ab, status, len);
  while (len < EConfig.screencols) {
    if (EConfig.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(EConfig.statusmsg);
  if (msglen > EConfig.screencols)
    msglen = EConfig.screencols;
  if (msglen && time(NULL) - EConfig.statusmsg_time < 5) {
    abAppend(ab, EConfig.statusmsg, msglen);
  }
}

// uses appendBuffer to build the stdout and writing to terminal once instead
// of calling write() many times, causing flickering
void editorRefreshScreen() {

  editorScroll();
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);
  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
           (EConfig.cy - EConfig.rowoffset) + 1, EConfig.rx + 1);
  abAppend(&ab, buf, strlen(buf));
  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init ***/

void initEditor() {
  EConfig.cx = 0;
  EConfig.cy = 0;
  EConfig.rx = 0;
  EConfig.numrows = 0;
  EConfig.row = NULL;
  EConfig.rowoffset = 0;
  EConfig.coloffset = 0;
  EConfig.filename = NULL;
  EConfig.statusmsg[0] = '\0';
  EConfig.statusmsg_time = 0;
  EConfig.dirty = 0;

  EConfig.normalMode = true;

  if (getWindowSize(&EConfig.screenrows, &EConfig.screencols) == -1) {
    die("getWindowSize");
  }

  // Make room for status bar
  EConfig.screenrows -= 2;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("CTRL-q to quit");

  while (1) {
    // handle key press, exits if ctrl-q is pressed
    editorProcessKeypress();
    editorRefreshScreen();
  }

  return 0;
}
