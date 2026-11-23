CC = gcc
CFLAGS = -Wall -Wextra -pthread

vm: main.o
	$(CC) $(CFLAGS) -o vm main.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f vm main.o

.PHONY: clean
