CC=clang
CFLAGS=-std=c99 -Wall -pedantic

all: build

build: 
	$(CC) $(CFLAGS) *.c

clean:
	killall -9 a.out

