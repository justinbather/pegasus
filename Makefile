CC = gcc
CFLAGS = -Wall -Wextra -I.
DEPS = include/terminal.h include/editordata.h
OBJ = src/pegasus.o src/terminal.o src/editordata.o


%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

pegasus: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
