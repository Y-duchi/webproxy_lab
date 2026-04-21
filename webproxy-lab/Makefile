# Makefile for Proxy Lab 
#
# You may modify this file any way you like (except for the handin
# rule). You instructor will type "make" on your specific Makefile to
# build your proxy from sources.

CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

all: proxy echo client

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

echo.o: echo.c csapp.h
	$(CC) $(CFLAGS) -c echo.c

client.o: client.c csapp.h
	$(CC) $(CFLAGS) -c client.c

proxy: proxy.o csapp.o
	$(CC) $(CFLAGS) proxy.o csapp.o -o proxy $(LDFLAGS)

echo: echo.o csapp.o
	$(CC) $(CFLAGS) echo.o csapp.o -o echo $(LDFLAGS)

client: client.o csapp.o
	$(CC) $(CFLAGS) client.o csapp.o -o client $(LDFLAGS)

# Creates a tarball in ../proxylab-handin.tar that you can then
# hand in. DO NOT MODIFY THIS!
handin:
	(make clean; cd ..; tar cvf $(USER)-proxylab-handin.tar proxylab-handout --exclude tiny --exclude nop-server.py --exclude proxy --exclude driver.sh --exclude port-for-user.pl --exclude free-port.sh --exclude ".*")

clean:
	rm -f *~ *.o proxy echo client server core *.tar *.zip *.gzip *.bzip *.gz
