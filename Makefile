all: server client

logger:
	gcc logger.c -c -ggdb

server: logger
	gcc server.c logger.o -o server.out -ggdb

client: logger
	gcc client.c logger.o -o client.out

clean:
	rm server.out client.out logger.o
