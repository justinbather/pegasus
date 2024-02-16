CC = gcc
CFLAGS = -Wall -Wextra -I./include
DEPS = include/terminal.h include/editordata.h
OBJ = src/pegasus.o src/terminal.o


%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

pegasus: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
