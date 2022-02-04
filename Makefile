DEBUG = 0

ifeq (1,$(DEBUG))
CFLAGS = -O0 -g -fsanitize=address -fsanitize=undefined
else
CFLAGS = -O3 -march=native
endif

WFLAGS = -Wall -Wextra -Wpedantic -Wshadow
CC = gcc -std=gnu99 -pedantic
LIBS = -lm

TITLE = roman2
BIN = $(TITLE)
SRC = $(TITLE).c

install: $(SRC) Makefile
	$(CC) $(CFLAGS) $(WFLAGS) $< -o $(BIN) $(LIBS)
	sudo mv $(BIN) /usr/bin

uninstall:
	sudo rm /usr/bin/$(BIN)
