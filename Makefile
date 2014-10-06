FLAGS = -Wall
CC = cc
all:
	$(CC) $(FLAGS) -o client client.c
	$(CC) $(FLAGS) -o server server.c

debug:
	$(CC) $(FLAGS) -g -o client client.c
	$(CC) $(FLAGS) -g -o server server.c

clean:
	rm client server
