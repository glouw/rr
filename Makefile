TITLE = roman2
DEBUG = 1

ifeq (1,$(DEBUG))
CFLAGS = -O0 -g -fsanitize=address -fsanitize=undefined
else
CFLAGS = -O3 -march=native
endif

WFLAGS = -Wall -Wextra -Wpedantic -Wshadow
CC = gcc -std=gnu99 -pedantic

BIN = $(TITLE)
SRC = $(TITLE).c
HDR = $(TITLE).h
LIB = lib$(TITLE).so

install: $(SRC) $(HDR) Makefile
	sudo cp $(HDR) /usr/include
	$(CC) -DRR_LIBROMAN2=\"$(TITLE)\" $(CFLAGS) $(WFLAGS) $< -o $(LIB) -fpic -shared 
	sudo mv $(LIB) /usr/lib
	$(CC) -DRR_MAIN -DRR_LIBROMAN2=\"$(TITLE)\" $(CFLAGS) $(WFLAGS) $< -o $(BIN) -ldl -l$(TITLE)
	sudo mv $(BIN) /usr/bin

uninstall:
	sudo rm /usr/include/$(HDR)
	sudo rm /usr/lib/$(LIB)
	sudo rm /usr/bin/$(BIN)

clean:
	rm -f $(BIN) $(LIB)
