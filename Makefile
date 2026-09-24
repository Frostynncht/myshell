CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -D_POSIX_C_SOURCE=200809L

SRCS = src/main.c src/lexer.c
OBJS = $(SRCS:.c=.o)

all: myshell

myshell: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o myshell

debug: CFLAGS += -g -fsanitize=address,undefined
debug: clean myshell

clean:
	rm -f myshell $(OBJS)

.PHONY: all debug clean