CC = gcc

all: clean appserver

appserver:
	gcc -pthread -o appserver appserver.c Bank.c

clean:
	$(RM) appserver
