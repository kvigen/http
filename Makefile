FLAGS = -Wall
CC = cc
all:
	$(CC) $(FLAGS) -o client client.c

debug:
	$(CC) $(FLAGS) -g -o client client.c
