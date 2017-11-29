all: server client

server:
	gcc server.c -o server.out

client:
	gcc client.c -o client.out

clean:
	rm server.out client.out
