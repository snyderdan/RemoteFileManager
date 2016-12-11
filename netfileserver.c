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

/**
 * Sends a status character, and integer to a client specified by fd.
 * Returns 0 on success, or -1 on error, with errno set
 */
int sendResponseInt(int fd, char stat, int num) {
	char msg[20];
	int len;
	
	sprintf(msg, "%c%c%d", stat, SEP_CHAR, num);
	len = strlen(msg);
	if (write(fd, &len, 4) == -1) return -1;
	if (write(fd, msg, len) == -1) return -1;
}

/**
 * Sends a status character, and a string message to a client specified by fd.
 * Returns 0 on success, or -1 on error, with errno set
 */
void sendResponse(int fd, char stat, char *resp) {
	char msg[strlen(resp) + 2];
	int len;
	
	sprintf(msg, "%c%c%s", stat, SEP_CHAR, resp);
	len = strlen(msg);
	if (write(fd, &len, 4) == -1) return -1;
	if (write(fd, msg, len) == -1) return -1;
}

/**
 * Receives a message from a client. Returns null on error with errno set, and a 
 * malloc()'ed character string containing all the data sent from the client. 
 * Remember to free the character pointer returned from this function.
 */
char *getMessage(int fd) {
	int len, rdlen;
	
	if (read(fd, &len, 4) < 4) return NULL;
	
	char *msg = malloc(len);
	if (read(fd, msg, len) < len) {
		free(msg);
		return NULL;
	}
	
	return msg;
	
}

void *handleClient(void *ptr) {
	int clientfd = * ((int *) ptr);
	char inmsg[6];
	
	// read opening msg from client
	int rdlen = read(clientfd, inmsg, 6);
	
	if (rdlen != 6) {
		sendResponseInt(clientfd, STATUS_FAILURE, INVALID_FILE_MODE);
		return NULL;
	} else if (inmsg[8] == MODE_UNRESTRCT) {
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else if (inmsg[8] == MODE_EXCLUSIVE) {
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else if (inmsg[8] == MODE_TRANSACTN) {
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else {
		sendResponseInt(clientfd, STATUS_FAILURE, INVALID_FILE_MODE);
		return NULL;
	}
	
	// loop to handle any number of requests from client
	
	return NULL;
}

void addClient(int clientfd, struct sockaddr_in *info) {
	pthread_t threadid;
	char *ipaddr;
	
	static int i = 0;
	static char clientbuf[10];
	// store client file descriptors in an array to prevent the memory being overwritten
	// before the worker thread is started and able to copy the value
	clientbuf[i] = clientfd;
	ipaddr = inet_ntoa(info->sin_addr);
	printf("\nConnected to %s, FD: %d\n",ipaddr, clientfd);
	pthread_create(&threadid, NULL, &handleClient, (void *) (clientbuf + i));
	
	i = (i + 1) % 10;
}

int main(int argc, char *argv[]) {
	struct sockaddr_in *serverInfo, *clientInfo;
	
	int serversock, clientfd;
	uint infolen;
	 
	serversock = socket(AF_INET, SOCK_STREAM, 0);
    if (serversock < 0) error("Cannot open socket");
    
	// allocate serverInfo struct
	serverInfo = calloc(sizeof(struct sockaddr_in), 1);
	clientInfo = calloc(sizeof(struct sockaddr_in), 1);
	// configure server
	serverInfo->sin_port = htons(PORT_NUM);
    serverInfo->sin_family = AF_INET;
    serverInfo->sin_addr.s_addr = INADDR_ANY;
     
    // bind server to socket
    if (bind(serversock, (struct sockaddr *) serverInfo, sizeof(struct sockaddr_in)) < 0) error("Failed to bind to socket");

	// set up the server socket to listen for client connections
    if (listen(serversock, NUM_CLIENTS) < 0) error("Unable to listen on socket");
    
    infolen = sizeof(struct sockaddr_in);
    
    while (1) {
		clientfd = accept(serversock, (struct sockaddr *) clientInfo, &infolen);
		
		if (clientfd < 0) error("Unable to accept client");
		
		addClient(clientfd, clientInfo);
	}
}