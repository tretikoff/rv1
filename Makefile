CC=clang
CFLAGS=-std=c99 -Wall -pedantic

all: build logsRemove

build: 
	$(CC) $(CFLAGS) *.c

logsRemove:
	rm -r *.log

clean:
	killall -9 a.out
