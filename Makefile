client: client.c
	$(CC) -O0 -g3 -Wall -Wextra $< -o $@ -lpthread
