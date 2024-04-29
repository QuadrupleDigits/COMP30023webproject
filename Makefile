CC = gcc
CFLAGS = -std=c99 -O3 -Wall -Wpedantic

all: tagger-v1

%: %.c
	$(CC) $(CFLAGS) -o $@ $<
