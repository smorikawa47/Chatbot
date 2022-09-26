all: lets-talk.c list.c server.c client.c
	gcc -g -Wall -o lets-talk lets-talk.c list.c server.c client.c -lpthread

Valgrind:
	valgrind --leak-check=full ./lets-talk 3000 localhost 3001

clean:
	$(RM) lets-talk