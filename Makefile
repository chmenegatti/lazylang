CC = gcc
CFLAGS = -Wall -Wextra -std=c11
SRCS = $(wildcard src/*.c) \
	$(wildcard src/ast/*.c) \
	$(wildcard src/parser/*.c) \
	$(wildcard src/sema/*.c) \
	$(wildcard src/codegen/*.c)

all: lazylangc

lazylangc: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f lazylangc
