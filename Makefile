CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -D_POSIX_C_SOURCE=200809L

all: myshell

myshell: src/main.c
	$(CC) $(CFLAGS) src/main.c -o myshell

debug:
	$(CC) $(CFLAGS) -g -fsanitize=address,undefined src/main.c -o myshell

clean:
	rm -f myshell *.o src/*.o