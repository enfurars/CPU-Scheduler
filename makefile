CC = gcc
CFLAGS = -Wall

scheduler: scheduler.c
	$(CC) $(CFLAGS) scheduler.c -o scheduler -lm

.PHONY: clean

clean:
	rm -f scheduler