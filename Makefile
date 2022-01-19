CC = gcc -std=gnu99 -pedantic
CFLAGS = -ldl -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -Wpedantic -Wshadow
#CFLAGS = -O3 -march=native
LIB = libroman2.so
BIN = roman2
$(BIN): roman2.c roman2.h Makefile
	$(CC) -DROMAN2SO=\"$(BIN)\" $(CFLAGS) -fpic -shared libroman2.c -o $(LIB)
	sudo mv $(LIB) /usr/lib
	$(CC) -DROMAN2SO=\"$(BIN)\" $(CFLAGS) $< -o $@ -lroman2
	sudo mv $(BIN) /usr/bin

clean:
	rm -f $(BIN) $(LIB)
