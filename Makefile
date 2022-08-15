CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -ansi
OUTFILE = bf

ifeq ($(release),1)
	CFLAGS += -Os -DNDEBUG
else
	CFLAGS += -g3 -ggdb
endif

ifeq ($(OS),Windows_NT)
	mmap = ./extern/mmap.c
endif

jit: $(wildcard $(mmap))
	$(CC) -o $(OUTFILE) ./jitfuck.c $< $(CFLAGS) -mno-sse -m64

while:
	$(CC) -o $(OUTFILE) ./whilefuck.c $(CFLAGS)
