all:server client

server:server.c
	gcc -o server -pthread server.c

client:client.c
	gcc -o client -pthread client.c
