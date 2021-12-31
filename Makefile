CC = gcc -std=gnu99 -pedantic
CFLAGS = -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -Wpedantic -Wshadow
#CFLAGS = -Ofast

rr: rr.c Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f rr
