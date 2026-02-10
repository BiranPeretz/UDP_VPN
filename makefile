CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
OBJS    = tun.o io.o
HDRS    = tun.h io.h

all: client server

client: client.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

server: server.o $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o client server
