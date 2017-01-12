CFLAGS ?=
CFLAGS += -Os -g3 -Wall -Wextra
THREAD_COUNT ?= 8

all: client brute

client: client.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

brute: brute.c
	$(CC) $(CFLAGS) -DTHREAD_COUNT=$(THREAD_COUNT) $< -o $@ -lpthread

.PHONY: all
