#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "libnetfiles.h"

#define NUM_CLIENTS 10

void error(char *msg) {
    perror(msg);
    exit(0);
}

void *handleclient(void *ptr) {
	int clientfd = * ((int *) ptr);
	return NULL;
}

void clientManager(int clientfd, struct sockaddr_in *info) {
	
	static int clients[NUM_CLIENTS] = {0};
	static char inUse[NUM_CLIENTS] = {0};
	
	pthread_t threadid;
	char *ipaddr;
	int i;
	
	for (i=0; i<NUM_CLIENTS; i++) {
		if (inUse[i] == 0) break;
	}
	
	if (i < NUM_CLIENTS) {
		inUse[i] = 1;
		clients[i] = clientfd;
	} else {
		char *msg = atoi
	}
	
	ipaddr = inet_ntoa(info->sin_addr);
	printf("\nConnected to %s, FD: %d\n",ipaddr, clientfd);
	pthread_create(&threadid, NULL, &handleclient, (void *) &clientfd);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in *serverInfo, *clientInfo;
	
	int serversock, clientfd;
	uint infolen;
	 
	serversock = socket(AF_INET, SOCK_STREAM, 0);
    if (serversock < 0) {
       error("Cannot open socket");
	}
	// allocate serverInfo struct
	serverInfo = calloc(sizeof(struct sockaddr_in), 1);
	clientInfo = calloc(sizeof(struct sockaddr_in), 1);
	// configure server
	serverInfo->sin_port = htons(PORT_NUM);
    serverInfo->sin_family = AF_INET;
    serverInfo->sin_addr.s_addr = INADDR_ANY;
     
    // bind server to socket
    if (bind(serversock, (struct sockaddr *) serverInfo, sizeof(struct sockaddr_in)) < 0) {
		error("Failed to bind to socket");
	}
			  
	// set up the server socket to listen for client connections
    listen(serversock, NUM_CLIENTS);
    
    infolen = sizeof(struct sockaddr_in);
    
    while (1) {
		clientfd = accept(serversock, (struct sockaddr *) clientInfo, &infolen);
		clientManager(clientfd, clientInfo);
	}
}