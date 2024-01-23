
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <sys/ioctl.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig EConfig;


/*** terminal ***/

void die(const char *s) {
  // Clear terminal and move cursor to top left upon exit
  write(STDOUT_FILENO, "\1xb[2J", 4);
  write(STDOUT_FILENO, "\1xb[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &EConfig.orig_termios) == -1) {
    die("tcsetattr");
  }
}


void enableRawMode() {

  // Gets attributes of users termios to make a copy, allowing us to revert the users termios from rawMode to normal mode upon exit
  if (tcgetattr(STDIN_FILENO, &EConfig.orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);

  // Make copy of termios
  struct termios raw = EConfig.orig_termios;

  // Bitwise operations to manually turn on raw mode, allowing program to take in things like `CTRL-c` as bytes instead of commands
  // termios input flags
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // termios output flags
  raw.c_oflag &= ~(OPOST);
  //termios control flags
  raw.c_cflag |= (CS8);
  //termios local flags (dump of misc. state)
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  //control characters
  //minimum bytes of input needed before read can return
  raw.c_cc[VMIN] = 0;
  //max time to wait before read() returns, if timedout read() will return 0 instead of num bytes from stdin
  raw.c_cc[VTIME] = 1;
  
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
    die("tcsetattr");
  }
}

char editorReadKey() {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer **/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  // request a larger block of memory to add a string of size n to our exisiting struct in memory so we can copy the given string to the end of our current buffer data
  // if realloc cant add to our exisiting block of memory it will free our existing block and find a new one of size n + ab->len
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;

  //copys new string to end of our buffer
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;

}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** input ***/
void editorProcessKeypress() {
  char c = editorReadKey();

  switch(c) {
    //Quit key
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}


/*** output ***/
void editorDrawRows(struct abuf *ab) {
  int y;

  // Print tildes on each line
  for (y = 0; y < EConfig.screenrows; y++) {
    abAppend(ab, "~", 1);

    if (y < EConfig.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}


//uses appendBuffer to build the stdout and writing to terminal once instead of calling write() many times, causing flickering
void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  //From unistd.h
  //Writes 4 bytes to the terminal
  //byte 1: \x1b (escape character / 27 in decimal)
  //escape chars always followed by `[`
  //bytes 2-4: [2J
  //J command (Erase in display) takes args before itself
  //in this case we use `2` which tells it to clear entire screen
  //<esc>[1J clears screen up to cursor
  //<esc>[0J clears screen after cursor (default argument to J command)
  //using VT100 escape sequences
  //NOTE: Could use ncurses library to support max amount of terminals. It uses teminfo db to choose what escape sequences to use for a users teminal

  abAppend(&ab, "\x1b[2J", 4);

  //moves cursor to the top left
  //H command is for cursor positioning and takes 2 args -> row num, column num separated by ;
  //ex \x1b[12;40H
  //defaults to 1;1

  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  abAppend(&ab, "\x1b[H", 3);
  
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*** init ***/

void initEditor() {
  if (getWindowSize(&EConfig.screenrows, &EConfig.screencols) == -1) {
    die("getWindowSize");
  }
}

int main() {
  enableRawMode();
  initEditor();

  while(1) {
    // handle key press, exits if ctrl-q is pressed
    editorProcessKeypress();
    editorRefreshScreen();
  }

  return 0;
}
