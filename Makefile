CFLAGS ?=
CFLAGS += -Os -g3 -Wall -Wextra

all: client brute

client: client.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

brute: brute.c
	$(CC) $(CFLAGS) $< -o $@ -lpthread

.PHONY: all
