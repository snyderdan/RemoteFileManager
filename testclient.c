#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "libnetfiles.h"

/**
 * Receives a message from a client. Returns null on error with errno set, and a 
 * malloc()'ed character string containing all the data sent from the client. 
 * Remember to free the character pointer returned from this function.
 * 
 * If this method returns NULL, then the connection was lost, and ERRNO was set
 * appropriately. It will deal with other types of errors internally.
 */
char *getResponse(int fd) {
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
	return msg;
}

/**
 * Sends a status character, and a string message to a client specified by fd.
 * Returns 0 on success, or -1 on error, with errno set
 * 
 * If this method returns -1, then the connection was lost, and ERRNO was set
 * appropriately. It will deal with other types of errors internally.
 */
int sendMessage(int fd, char cmd, char *args, char opt) {
	char msg[strlen(args) + 4];
	int val, len;
	// create full message to send, and get length
	sprintf(msg, "%c%c%s%c%c", cmd, SEP_CHAR, args, SEP_CHAR, opt);
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
int sendMessageInt(int fd, char cmd, int num) {
	char msg[10];
	
	sprintf(msg, "%d", num);
	return sendMessage(fd, cmd, msg, 0);
}


void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
	// Declare initial vars
    int sockfd = -1;																// file descriptor for our socket
	int portno = -1;																// server port to connect to
	char buffer[256];															// char array to store data going to and coming from the server
    struct sockaddr_in serverAddressInfo;						// Super-special secret C struct that holds address info for building our socket
    struct hostent *serverIPAddress;									// Super-special secret C struct that holds info about a machine's address
    
	/** If the user gave enough arguments, try to use them to get a port number and address **/

	// convert the text representation of the port number given by the user to an int
	portno = 20000;
	
	// look up the IP address that matches up with the name given - the name given might
	//    BE an IP address, which is fine, and store it in the 'serverIPAddress' struct
    serverIPAddress = gethostbyname("localhost");
    if (serverIPAddress == NULL)
	{
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
				
	// try to build a socket .. if it doesn't work, complain and exit
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
	{
        error("ERROR creating socket");
	}


	
	/** We now have the IP address and port to connect to on the server, we have to get    **/
	/**   that information into C's special address struct for connecting sockets                     **/

	// zero out the socket address info struct .. always initialize!
    bzero((char *) &serverAddressInfo, sizeof(serverAddressInfo));

	// set a flag to indicate the type of network address we'll be using 
    serverAddressInfo.sin_family = AF_INET;
	
	// set the remote port .. translate from a 'normal' int to a super-special 'network-port-int'
	serverAddressInfo.sin_port = htons(portno);

	// do a raw copy of the bytes that represent the server's IP address in 
	//   the 'serverIPAddress' struct into our serverIPAddressInfo struct
    bcopy((char *)serverIPAddress->h_addr, (char *)&serverAddressInfo.sin_addr.s_addr, serverIPAddress->h_length);



	/** We now have a blank socket and a fully parameterized address info struct .. time to connect **/
	
	// try to connect to the server using our blank socket and the address info struct 
	//   if it doesn't work, complain and exit
    if (connect(sockfd,(struct sockaddr *)&serverAddressInfo,sizeof(serverAddressInfo)) < 0) 
	{
        error("ERROR connecting");
	}	
	
	
	
	/** If we're here, we're connected to the server .. w00t!  **/
	
	sendMessage(sockfd, MODE_UNRESTRCT, "", 0);
	printf("%s\n", getResponse(sockfd));
    sendMessage(sockfd, FN_OPEN, "helpme.txt", MODE_WR);
    printf("%s\n", getResponse(sockfd));
    close(sockfd);
    return 0;
}
