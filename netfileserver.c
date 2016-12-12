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
#include <signal.h>

#include "libnetfiles.h"

/*******************************************************************************
 * 
 * File permission management
 * 
 * Code below provides a mechanism for implementing unrestricted mode,
 * exclusive mode and transaction mode, allowing for clients to choose
 * how they view files.
 * 
 *******************************************************************************/
 
/*
 * 
 * How this file management system works:
 * 
 * There is a global linked list of files that are open by clients. Each node
 * contains 1 MultiFile object which describes particulars on a file. A file
 * is added to the global file when it is opened by any client, and removed
 * when no more clients have it opened.
 * 
 * Each client has a linked list of files they have open. Each node contains
 * 1 MultiFile object, which is the same one contained in the global list,
 * so everyone sees the same thing. A file is added to a clients list when
 * the client successfully opens it, and removed when the client either 
 * closes the file or disconnects.
 * 
 * refcount keeps track of how many clients have a file open. access records
 * the highest (most restrictive) access level someone has the file open in.
 * write is either 0 or 1, and tells if there is any client with the file open
 * in write mode.
 * 
 * The files access level is checked before opening. If it is opened in transaction
 * mode, the open fails immediately. 
 * 
 * If the file is open in exclusive mode:
 *   - if attempting transaction mode, fail
 *   - if attempting 
 * 
 */
 
typedef struct {
	char access;
	char write;
	void *prev;
	void *next;
} ClientHandle;

typedef struct {
	int fd;
	int refcount;
	char access;
	char write;
} MultiFile;

typedef struct {
	MultiFile *node;
	void *prev;
	void *next;
} LinkedNode;

/*******************************************************************************
 * 
 * Function implementations
 * 
 * Code below implements each function that the server will provide being,
 * netopen, netclose, netread, netwrite.
 * 
 *******************************************************************************/

void error(char *msg) {
    perror(msg);
    exit(0);
}

int netopen(const char *pathname, int flags) {
	
}

/*******************************************************************************
 * 
 * Client communication helper functions
 * 
 * Functions below implement the protocol for communication between client
 * and server. These include getMessage(), sendReponse() and sendResponseInt()
 * 
 *******************************************************************************/

/**
 * Receives a message from a client. Returns null on error with errno set, and a 
 * malloc()'ed character string containing all the data sent from the client. 
 * Remember to free the character pointer returned from this function.
 * 
 * If this method returns NULL, then the connection was lost, and ERRNO was set
 * appropriately. It will deal with other types of errors internally.
 */
char *getMessage(int fd) {
	int val, len;
	// read length of message
	val = read(fd, &len, 4);
	// if val == 0 we got a clean close, if val == -1, an error occurred
	if (val == 0 || val == -1) {
		// either way, we should try to close the socket and return, while maintaining errno
		val = errno;
		close(fd);
		errno = val;
		return NULL;
	}
	
	char *msg = malloc(len+1);
	// read actual message
	val = read(fd, msg, len);
	// if val == 0 we got a clean close, if val == -1, an error occurred
	if (val == 0 || val == -1) {
		// either way, we should try to close the socket and return, while maintaining errno
		val = errno;
		close(fd);
		free(msg);
		errno = val;
		return NULL;
	}
	
	msg[len] = 0;
	
	printf("GOT: %s\n", msg);
	return msg;
}

/**
 * Sends a status character, and a string message to a client specified by fd.
 * Returns 0 on success, or -1 on error, with errno set
 * 
 * If this method returns -1, then the connection was lost, and ERRNO was set
 * appropriately. It will deal with other types of errors internally.
 */
int sendResponse(int fd, char stat, char *resp) {
	char msg[strlen(resp) + 2];
	int val, len;
	// create full message to send, and get length
	sprintf(msg, "%c%c%s", stat, SEP_CHAR, resp);
	len = strlen(msg);
	// first step is to write the message length to the client (first 4 bytes)
	val = write(fd, &len, 4);
	// if val == 0 we got a clean close, if val == -1, an error occurred
	if (val == 0 || val == -1) {
		// either way, we should try to close the socket and return, while maintaining errno
		val = errno;
		close(fd);
		errno = val;
		return -1;
	}
	
	// now we write the actual message
	val = write(fd, msg, len);
	// if val == 0 we got a clean close, if val == -1, an error occurred
	if (val == 0 || val == -1) {
		// either way, we should try to close the socket and return, while maintaining errno
		val = errno;
		close(fd);
		errno = val;
		return -1;
	}
	
	return 0;
}

/**
 * Sends a status character, and integer to a client specified by fd.
 * Returns 0 on success, or -1 on error, with errno set
 * 
 * If this method returns -1, then the connection was lost, and ERRNO was set
 * appropriately. It will deal with other types of errors internally.
 */
int sendResponseInt(int fd, char stat, int num) {
	char msg[10];
	
	sprintf(msg, "%d", num);
	return sendResponse(fd, stat, msg);
}

/*******************************************************************************
 * 
 * Client handling functions
 * 
 * Functions below include the main method which accepts incoming connections,
 * spawns off a new worker thread, which then handles the client in the 
 * handleClient function.
 * 
 *******************************************************************************/

void *handleClient(void *ptr) {
	int clientfd = * ((int *) ptr);
	int msglen, tmperr, running = 1;
	char *inmsg;

	// read opening msg from client
	inmsg = getMessage(clientfd);
	
	if (inmsg == NULL) return NULL;
	
	// handles initial connection to client
	if (inmsg[0] == MODE_UNRESTRCT) {
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else if (inmsg[0] == MODE_EXCLUSIVE) {
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else if (inmsg[0] == MODE_TRANSACTN) {
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else {
		sendResponseInt(clientfd, STATUS_FAILURE, INVALID_FILE_MODE);
		close(clientfd);
		running = 0;
	}
	
	free(inmsg);
	
	// loop to handle any number of requests from client
	while (running) {
		inmsg = getMessage(clientfd);
		
		if (inmsg == NULL) break;
		
		if (inmsg[0] == FN_OPEN) {
			// open a file
			msglen = strlen(inmsg);
			char outmsg[msglen + 30];
			sprintf(outmsg, "Got: OPEN '%s' $$$\0", inmsg);
			sendResponse(clientfd, STATUS_SUCCESS, outmsg);
		} else if (inmsg[0] == FN_CLOSE) {
			// close a specific file
			msglen = strlen(inmsg);
			char outmsg[msglen + 30];
			sprintf(outmsg, "Got: CLOSE '%s' $$$\0", inmsg);
			sendResponse(clientfd, STATUS_SUCCESS, outmsg);
		} else if (inmsg[0] == FN_READ) {
			// read data and send to client
			msglen = strlen(inmsg);
			char outmsg[msglen + 230];
			sprintf(outmsg, "Got: READ '%s' $$$\0", inmsg);
			sendResponse(clientfd, STATUS_SUCCESS, outmsg);
		} else if (inmsg[0] == FN_WRITE) {
			// write client data to file
			msglen = strlen(inmsg);
			char outmsg[msglen + 30];
			sprintf(outmsg, "Got: WRITE '%s' $$$\0", inmsg);
			sendResponse(clientfd, STATUS_SUCCESS, outmsg);
		}
		
		free(inmsg);
	}
	
	printf("Closed connection FD: %d\n", clientfd);
	
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
	// ignore SIGPIPE if clients disconnect
	signal(SIGPIPE, SIG_IGN);
	 
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
    if (listen(serversock, 10) < 0) error("Unable to listen on socket");
    
    infolen = sizeof(struct sockaddr_in);
    
    while (1) {
		// accept client connection
		clientfd = accept(serversock, (struct sockaddr *) clientInfo, &infolen);
		if (clientfd < 0) error("Unable to accept client");
		// add client to be managed
		addClient(clientfd, clientInfo);
	}
}