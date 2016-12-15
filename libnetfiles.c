#include "libnetfiles.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


int sockfd = -1;
struct sockaddr_in serverAddressInfo;						// Super-special secret C struct that holds address info for building our socket
struct hostent *serverIPAddress;									// Super-special secret C struct that holds info about a machine's address

void error(char *msg) {
    perror(msg);
    exit(0);
}

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
int sendMessage(int fd, char cmd, const char *args, char opt) {
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

int netserverinit(char * hostname, int connectMode){
	char *message;
	int status;
	// look up the IP address that matches up with the name given - the name given might
	//    BE an IP address, which is fine, and store it in the 'serverIPAddress' struct
    serverIPAddress = gethostbyname(hostname);
    if (serverIPAddress == NULL)
	{
        fprintf(stderr,"ERROR, no such host\n");
        return -1;
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
	{
        error("ERROR creating socket");
        return -1;
	}
	
	/** We now have the IP address and port to connect to on the server, we have to get    **/
	/**   that information into C's special address struct for connecting sockets                     **/

	// zero out the socket address info struct .. always initialize!
    bzero((char *) &serverAddressInfo, sizeof(serverAddressInfo));

	// set a flag to indicate the type of network address we'll be using 
    serverAddressInfo.sin_family = AF_INET;
	
	// set the remote port .. translate from a 'normal' int to a super-special 'network-port-int'
	serverAddressInfo.sin_port = htons(PORT_NUM);

	// do a raw copy of the bytes that represent the server's IP address in 
	//   the 'serverIPAddress' struct into our serverIPAddressInfo struct
    bcopy((char *)serverIPAddress->h_addr, (char *)&serverAddressInfo.sin_addr.s_addr, serverIPAddress->h_length);



	/** We now have a blank socket and a fully parameterized address info struct .. time to connect **/
	
	// try to connect to the server using our blank socket and the address info struct 
	//   if it doesn't work, complain and exit
    if (connect(sockfd,(struct sockaddr *)&serverAddressInfo,sizeof(serverAddressInfo)) < 0) 
	{
        error("ERROR connecting");
        return -1;
	}		
	/** If we're here, we're connected to the server .. w00t!  **/
	
	status = sendMessageInt(sockfd, connectMode, '\0');	
	if (status == -1) {
		return status;}
	message = getResponse(sockfd);
	if (message == NULL){
		return -1;
	} else if (message[0] == STATUS_SUCCESS){
		free(message);
		return 0;
	} else {
		errno = atoi(message + 2);
		free(message);
		return -1;
	}
	
	
}

//The  argument  flags  must  include  one of the following access  modes:  O_RDONLY, 
//O_WRONLY,  or  O_RDWR. These request   opening  the  file  read-only,  write-only,  or 
//read/write, respectively.
/* Open:
 *  Client->Server
 * 	- 1 byte function 'O'
 *  - 1 byte sep
 *  - n bytes file name
 *  - 1 byte sep
 *  - 1 byte mode */
 
int netopen(const char *pathname, int flags){
	int ret;
	char * message;
	ret = sendMessage(sockfd, FN_OPEN, pathname, flags);
	if (ret == -1) {
		return ret;
	}
	message = getResponse(sockfd);
	if (message == NULL){
		return -1;
	} else if (message[0] == STATUS_SUCCESS){
		ret = atoi(message + 2);
		free(message);
		return ret;
	} else {
		errno = atoi(message + 2);
		free(message);
		return -1;
	}
	
}	


 /* Close:
 *  Client->Server
 * 	- 1 byte function 'C'
 *  - 1 byte sep
 *  - 8 byte file descriptor
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 
  - n byte FD/error condition
  
   int netclose(int fd)
RETURN VALUE
netclose()  returns zero on  success. On  error, -1 is returned, and errno is set appropriately.
*/
int netclose(int fd){
	int ret;
	char * message;
	ret = sendMessageInt(sockfd, FN_CLOSE, fd);
	if (ret == -1) {
		return ret;}
	message = getResponse(sockfd);
	if (message == NULL){
		return -1;
		}
	else if (message[0] == STATUS_SUCCESS){
		free(message);
		return 0;
	} else {
		errno = atoi(message + 2);
		free(message);
		return -1;
	}
}	

 /* Read:
 *  Client->Server
 * 	- 1 byte function 'R'
 *  - 1 byte sep
 *  - 8 byte file descriptor
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 *  - n bytes data or error condition
 
 ssize_t netread(int fildes, void *buf, size_t nbyte)
RETURN VALUE
Upon successful completion, netread() should return  a  non-negative  integer indicating the
number of bytes  actually  read.  Otherwise,  the  function should return -1 and set errno to
indicate the error
 */
ssize_t netread(int fileDesc, void *buf, size_t nbyte){
	int status;
	char * message;
	int len;
	status = sendMessageInt(sockfd, FN_READ, fileDesc);
	if (status == -1){
		return status;}
	message = getResponse(sockfd);
	if (message == NULL){
		return -1;}
	else if (message[0] == STATUS_SUCCESS){
		len = strlen(message + 2);
		if (len >= nbyte){
			strncpy(buf, message + 2, len-2);
			((char *) buf)[nbyte -1] = '\0';
			free(message);
			return nbyte;
		} else {
			strcpy(buf, message + 2);
			((char *) buf)[len] = '\0';
			free(message);
			return len;
		}
	} else {
		errno = atoi(message + 2);
		free(message);
		return -1;
	} 
}



 /* Write:
 *  Client->Server
 * 	- 1 byte function 'W'
 *  - 1 byte sep
 *  - 8 byte file descriptor
 *  - 1 byte sep
 *  - n bytes data
  *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 *  - 8 byte length/error condition */
 
// ssize_t netwrite(int fildes, const void *buf, size_t nbyte)
//RETURN VALUE
//Upon successful completion, netwrite()  should return  the  number of bytes actually written to
//the file associated  with  fildes.  This  number  should never be greater than nbyte. Otherwise, -1
//should be returned and errno set to indicate the error.

ssize_t netwrite(int fileDesc, const void *buf, size_t nbyte){
	int status;
	char * message;
	int bytes;
	status = sendMessageInt(sockfd, FN_WRITE, fileDesc);
	status = sendMessage(sockfd, FN_WRITE, buf, '\0');
	if (status == -1){
		return status;}
	message = getResponse(sockfd);
	if (message == NULL){
		return -1;}
	else if (message[0] == STATUS_SUCCESS){
	bytes = atoi(message + 2);
		if (bytes > nbyte){
			free(message);
			printf("Too many bytes written\n");
			return -1;
		}
		else {
			free(message);
			return bytes;
		}
	}
	else {
		errno = bytes;
		free(message);
		return -1;} 
		
}
