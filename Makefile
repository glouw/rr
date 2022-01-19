CC = gcc -std=gnu99 -pedantic
CFLAGS = -ldl -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -Wpedantic -Wshadow
#CFLAGS = -O3 -march=native
LIB = libroman2.so
BIN = roman2
$(BIN): rr.c rr.h Makefile
	$(CC) -DROMAN2SO=\"$(LIB)\" -L/usr/bin -fpic -shared $< -o $(LIB)
	$(CC) -DROMAN2SO=\"$(LIB)\" $(CFLAGS) $(LDFLAGS) $< -o $@
	mv $(LIB) /usr/lib
	mv $(BIN) /usr/bin

clean:
	rm -f $(BIN) $(LIB)
