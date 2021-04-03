CC=gcc
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -ggdb
LDLIBS=-lcrypto
VPATH=src

all: server client 

server: server.c hash.o
client: client.c
hash.o: hash.c

clean:
	rm server client hash.o