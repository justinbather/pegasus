
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


/*** data ***/
struct termios orig_termios;


/*** terminal ***/

void die(const char *s) {
  // Clear terminal and move cursor to top left upon exit
  write(STDOUT_FILENO, "\1xb[2J", 4);
  write(STDOUT_FILENO, "\1xb[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    die("tcsetattr");
  }
}


void enableRawMode() {

  // Gets attributes of users termios to make a copy, allowing us to revert the users termios from rawMode to normal mode upon exit
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    die("tcgetattr");
  }
  atexit(disableRawMode);

  // Make copy of termios
  struct termios raw = orig_termios;

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


/*** input ***/
void editorProcessKeypress() {
  char c = editorReadKey();

  switch(c) {
    //Quit key
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\1xb[2J", 4);
      write(STDOUT_FILENO, "\1xb[H", 3);
      exit(0);
      break;
  }
}

/*** output ***/
void editorDrawRows() {
  int y;

  // Print tildes on each line
  for (y = 0; y < 24; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}


void editorRefreshScreen() {
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
  write(STDOUT_FILENO, "\x1b[2J", 4);

  //moves cursor to the top left
  //H command is for cursor positioning and takes 2 args -> row num, column num separated by ;
  //ex \x1b[12;40H
  //defaults to 1;1
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();
  
  write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** init ***/

int main() {
  enableRawMode();

  while(1) {
    // handle key press, exits if ctrl-q is pressed
    editorProcessKeypress();
    editorRefreshScreen();
  }

  return 0;
}
