#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "libnetfiles.h"

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in *serverInfo, *clientInfo;
	
	int serversock, clientfd;
	uint infolen;
	 
	serversock = socket(AF_INET, SOCK_STREAM, 0);
    if (serversock < 0) {
       error("Cannot open socket\n");
	}
	// allocate serverInfo struct
	serverInfo = calloc(sizeof(struct sockaddr_in), 1);
	// configure server
	serverInfo->sin_port = htons(PORT_NUM);
    serverInfo->sin_family = AF_INET;
    serverInfo->sin_addr.s_addr = INADDR_ANY;
     
    // bind server to socket
    if (bind(serversock, (struct sockaddr *) &serverInfo, sizeof(struct sockaddr_in)) < 0) {
		error("Failed to bind to socket\n");
	}
			  
	// set up the server socket to listen for client connections
    listen(serversock, 5);
    
    infolen = sizeof(struct sockaddr_in);
    
    while (1) {
		clientInfo = calloc(sizeof(struct sockaddr_in), 1);
		clientfd = accept(serversock, (struct sockaddr *) clientInfo, &infolen);
	}
}