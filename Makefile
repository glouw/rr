TITLE = roman2
DEBUG = 1

ifeq (1,$(DEBUG))
CFLAGS = -g -fsanitize=address -fsanitize=undefined
else
CFLAGS = -O3 -march=native
endif

WARNINGS = -Wall -Wextra -Wpedantic -Wshadow
CC = gcc -std=gnu99 -pedantic

BIN = $(TITLE)
LIB = lib$(TITLE).so

$(BIN): roman2.c roman2.h Makefile $(LIB)
	$(CC) -DROMAN2SO=\"$(TITLE)\" $(CFLAGS) $(WARNINGS) $< -o $@ -ldl -l$(TITLE)
	sudo mv $(BIN) /usr/bin
	sudo cp roman2.h /usr/include

$(LIB):libroman2.c roman2.h Makefile
	$(CC) -DROMAN2SO=\"$(TITLE)\" $(CFLAGS) $(WARNINGS) $< -o $@ -fpic -shared 
	sudo mv $(LIB) /usr/lib

clean:
	rm -f $(BIN) $(LIB)
