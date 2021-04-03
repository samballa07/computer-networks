CC=gcc
CFLAGS=-Wall -Iincludes -Wextra -std=c99 -std=gnu99 -pthread -ggdb
LDLIBS=-lcrypto -lm

all: server client 

server: server.c 
client: client.c

clean:
	rm client server 