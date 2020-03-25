CC=gcc
CFLAGS=-Wall -ggdb -Werror
LDFLAGS=-pthread

all: master master2 bin_adder

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

master: master.c common.o
	$(CC) $(CFLAGS) master.c common.o -o master $(LDFLAGS)

master2: master2.c common.o
	$(CC) $(CFLAGS) master2.c common.o -o master2 $(LDFLAGS) -lm

bin_adder: bin_adder.c common.o
	$(CC) $(CFLAGS) bin_adder.c common.o -o bin_adder $(LDFLAGS)

clean:
	rm -rf master master2 bin_adder common.o
