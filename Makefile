CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRCS = $(wildcard src/*.c) $(wildcard src/ast/*.c) $(wildcard src/parser/*.c) $(wildcard src/sema/*.c)

all: lazylangc

lazylangc: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f lazylangc
