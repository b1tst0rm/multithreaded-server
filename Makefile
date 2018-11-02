CC = gcc

all: clean appserver appserver-coarse

appserver:
	gcc -pthread -o appserver appserver.c Bank.c

appserver-coarse:
	gcc -pthread -o appserver-coarse appserver-coarse.c Bank.c

clean:
	$(RM) appserver appserver-coarse
