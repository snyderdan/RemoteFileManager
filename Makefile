default: all

all: netfileserver testclient

netfileserver: netfileserver.c libnetfiles.h
	gcc -o netfileserver netfileserver.c -lpthread
	
testclient: testclient.c libnetfiles.o
	gcc -o testclient testclient.c libnetfiles.o
	
libnetfiles.o: libnetfiles.c libnetfiles.h
	gcc -o libnetfiles.o -c libnetfiles.c
