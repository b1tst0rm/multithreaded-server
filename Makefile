CC = gcc

all: appserver

appserver:
	gcc -pthread -o appserver appserver.c Bank.c

clean:
	$(RM) appserver
