
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)


struct termios orig_termios;

void die(const char *s) {
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

int main() {
  enableRawMode();
  //read 1 byte from stdin until no more bytes or q is pressed
  while(1) {

  char c = '\0';

  if (read(STDIN_FILENO, &c, 1) == -1 && errno !=EAGAIN) die("read"); 

  if (iscntrl(c)) {
    printf("%d\n\r", c);
  } else {
    printf("%d ('%c')\n\r", c, c);
  }
    if (c == CTRL_KEY('q')) break;
  }
  return 0;
}
