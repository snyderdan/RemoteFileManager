#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h> 
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

/****************************************************************************************************
 * 																									*
 * Generic LinkedList methods and definitions														*
 * 																									*
 * Used to keep track of the global list of open files, the owners of each file, and				*
 * what files each client owns individually. 														*
 * 																									*
 ****************************************************************************************************/
 
typedef struct {
	void *value;
	void *prev;
	void *next;
} LinkedNode;

typedef struct {
	int length;
	int index;
	LinkedNode *cur;
	LinkedNode *head;
	LinkedNode *tail;
} LinkedList;

# define getNext(nd) ((LinkedNode *) nd->next)
# define getPrev(nd) ((LinkedNode *) nd->prev)
# define getHead(lst) ((LinkedNode *) lst->head)
# define getTail(lst) ((LinkedNode *) lst->tail)

/**
 * Added a value to the end of a linked list
 */
void linkedListAdd(LinkedList *list, void *val) {
	LinkedNode *node = calloc(sizeof(LinkedNode), 1);
	node->value = val;
	if (list->head == NULL) {
		list->head = node;
		list->tail = node;
		list->cur = node;
	} else {
		getTail(list)->next = node;
		node->prev = getTail(list);
		list->tail = node;
	}

	list->length++;
}

/**
 * Gets a link in the list at a specific index
 */
LinkedNode *linkedListGet(LinkedList *list, int index) {
	if (index < 0) return NULL;
	if (index >= list->length) return NULL;

	while (list->index < index) {
		list->index++;
		list->cur = getNext(list->cur);
	}

	while (list->index > index) {
		list->index--;
		list->cur = getPrev(list->cur);
	}
	return list->cur;
}

/**
 * Removes an object from the linked list. The comparison is by reference,
 * meaning only the pointer address is checked. Copies will not match.
 * 
 * Returns 0 if successfully removed a matching link, 1 otherwise.
 */
int linkedListRemove(LinkedList *list, void *val) {
	LinkedNode *node;
	int i;
	
	for (i=0; i<list->length; i++) {
		node = linkedListGet(list, i);
		if (node->value == val) {
			if (node->prev == NULL) {
				// removing the head
				list->head = getNext(node);
				if (list->head != NULL) list->head->prev = NULL;
			} else if (node->next == NULL) {
				// removing tail
				list->tail = getPrev(node);
				if (list->tail != NULL) list->tail->next = NULL;
			} else {
				// a middle node
				getPrev(node)->next = getNext(node);
				getNext(node)->prev = getPrev(node);
			}
			
			list->index = 0;
			list->cur = getHead(list);
			list->length--;
			
			if (list->length == 0) {
				list->head = NULL;
				list->tail = NULL;
			}
			free(node);
			return 0;
		}
	}
	
	return 1;
}

/****************************************************************************************************
 * 																									*
 * File permission management																		*	
 * 																									*
 * Code below provides a mechanism for implementing unrestricted mode,								*
 * exclusive mode and transaction mode, allowing for clients to choose								*
 * how they view files.																				*
 * 																									*	
 ****************************************************************************************************/
 
/*
 * 
 * How this file management system works:
 * 
 * There is a global linked list of files that are open by clients. Each node
 * represents 1 MultiFile object which describes particulars on a file. A file
 * is added to the global file when it is opened by a client for the first time,
 * and removed when no more clients have it opened.
 * 
 * Each MultiFile keeps track of the highest access level (most restrictive)
 * and whether or not any clients have write access. This is for easy permission
 * checking during opens. The highest access level and write value are re-evaluated
 * when a client closes the file.
 * 
 * When a client performs an operation on a file, we first check if they have
 * the file open and that their permissions are appropriate before fufilling
 * the request.
 * 
 * When a client performs a read or write, the issue of "what the fuck happens
 * to the file index" comes up. In transaction mode, the files index remains
 * wherever the client left it last since it will never be modified by anyone
 * else. In any other mode, the file index is set to 0 (start of file). This 
 * may seem like a lousy implementation, and that's because it is. But we 
 * don't have a netseek function, which is even lousier. So this is the only 
 * solution where we don't have to come up with some stupid special case about 
 * "what the fuck happens to the file index once someone rewrites the file"
 * 
 * Now you always get from the start of the file, and always write to the start
 * of the file. Unless you completely own the file.
 * 
 * Deal with it.
 */
 
typedef struct s_ClientHandle {
	int fd;
	int permission;
	char access;
} ClientHandle;

# define getClient(nd) ((ClientHandle *) nd->value)

typedef struct s_MultiFile {
	int fd;
	int refcount;
	int write;
	char access;
	char *fname;
	LinkedList *owners;
} MultiFile;

# define getFile(nd) ((MultiFile *) nd->value)

pthread_mutex_t fileLock;
LinkedList fileList = {0};

/**
 * Checks all open files to see if this file is open by another client.
 * If it is, we return a reference to that MultiFile. If not, then this
 * method creates a new MultiFile for the requested file.
 * 
 * On success, returns a MultiFile representing the specified file
 * On failure, returns NULL with errno set appropriately
 */
MultiFile *getFileByName(const char *fname) {
	LinkedNode *node;
	MultiFile *file;
	int fd, i;
	// loop through each object, checking for fname
	for (i=0; i<fileList.length; i++) { // go to end of list to get null value when file not found
		node = linkedListGet(&fileList, i);
		if (strcmp(getFile(node)->fname, fname) == 0) return getFile(node);
	}
	// file not yet opened by another client, so open it with r/w permission
	fd = open(fname, O_RDWR);
	if (fd == -1) return NULL;
	// allocate MultiFile, and initialize values
	file = calloc(sizeof(MultiFile), 1);
	file->fd = fd;
	file->fname = calloc(strlen(fname), 1);
	strcpy(file->fname, fname);
	file->owners = calloc(sizeof(LinkedList), 1);
	// add file to linked list
	linkedListAdd(&fileList, file);
	return file;
}

/**
 * Searches all opened files to get a file by a specific descriptor. Unlike
 * the getFileByName() method, this method will not allocate the file if it
 * does not get a matching file descriptor, as it does not know the name
 * of the file to open.
 * 
 * On success, returns a MultiFile representing the specified file
 * On failure, returns NULL with errno set appropriately
 */
MultiFile *getFileByFD(int fd) {
	MultiFile *file;
	int i;
	// loop through each object, checking for fname
	for (i=0; i<fileList.length; i++) { // go to end of list to get null value when file not found
		file = getFile(linkedListGet(&fileList, i));
		if (file->fd == fd) return file;
	}
	errno = EBADF;
	return NULL;
}

/**
 * Returns 0 if client does not have file open in any way, 1 if the 
 * client has access in the given permission, and -1 if the client has
 * access that differs from the specified permission
 */
int hasAccess(MultiFile *file, int clientfd, int permission) {
	LinkedList *list = file->owners;
	LinkedNode *node;
	int i;

	for (i=0; i<list->length; i++) {
		node = linkedListGet(list, i);
		if (getClient(node)->fd == clientfd) {
			if (getClient(node)->permission == permission) return 1;
			return -1;
		}
	}
	
	return 0;
}

/**
 * Updates the maximum access level and write permission on the file.
 * Only occurs when a new client is added or removed
 */
void updateAccess(MultiFile *file) {
	LinkedList *list = file->owners;
	LinkedNode *node;
	ClientHandle *handle;
	int write = 0, i;
	char maxAccess = 0;

	for (i=0; i<list->length; i++) {
		node = linkedListGet(list, i);
		handle = getClient(node);
		if (handle->permission == O_WRONLY || handle->permission == O_RDWR) write = 1;
		if (handle->access > maxAccess) maxAccess = handle->access;
	}
	
	file->access = maxAccess;
	file->write = write;
}

/**
 * Attempts to add client as an owner of a given MultiFile.
 * 
 * Returns -1 if we are unable to attach to the file due to permission 
 * conflicts. 0 if it was successful.
 */
int addOwner(MultiFile *file, int flags, int clientfd, char access) {
	
	ClientHandle *handle;
	
	if (hasAccess(file, clientfd, flags)) {
		// don't let clients open a file twice
		goto BADPERM;
	}
	
	if (file->access == 0) {
		// new file, add client no matter what
		goto GOODPERM;
	}
	
	if (file->access == MODE_TRANSACTN || access == MODE_TRANSACTN) {
		// if the file is not new, and someone has/wants transaction mode, that can't happen
		goto BADPERM;
	}
	
	if (flags == O_RDONLY) {
		// if the file is in exclusive or unrestricted and we want to read, we're good to go
		goto GOODPERM;
	}
	
	// past here, we know this client wants some form of write access
	if ((file->access == MODE_EXCLUSIVE || access == MODE_EXCLUSIVE) && file->write) {
		// fail if the file is/would be in exclusive mode and someone else has write access
		goto BADPERM;
	}
	
	// now either no one else has write access, or we're all in unrestricted mode and don't care!
	GOODPERM:
	// initialize client handle
	handle = calloc(sizeof(ClientHandle), 1);
	handle->access = access;
	handle->fd = clientfd;
	handle->permission = flags;
	file->refcount++;
	linkedListAdd(file->owners, handle);
	updateAccess(file);	// update access level
	return 0;
	
	// there was a conflict with existing permissions
	BADPERM:
	errno = EPERM;
	return -1;
}

/**
 * Removes an owner from a file. Returns 0 on success. If the client did not
 * have access to the file, errno is set and -1 is returned.
 */
int removeOwner(MultiFile *file, int clientfd) {
	LinkedList *owners;
	ClientHandle *handle;
	int i;
	// if they don't have it open, set errno and return
	if (!hasAccess(file, clientfd, O_RDONLY)) {
		// if they have a valid reference to the file, then they tried closing multiple times
		// so this is the appropriate errno that close() would throw
		errno = EBADF;
		return -1;
	}
	// first off, remove the client from the list
	owners = file->owners;
	
	for (i=0; i<owners->length; i++) {
		handle = getClient(linkedListGet(owners, i));
		if (handle->fd == clientfd) {
			linkedListRemove(owners, handle);
			break;
		}
	}
	// free client
	free(handle);
	// update refcount
	file->refcount--;
	if (file->refcount == 0) {
		// if no one is holding the file, close and remove file from linked list
		linkedListRemove(&fileList, file);
		close(file->fd);	// close file
		free(file->owners); // only owned by us, so we don't have to cycle through the tree
		free(file->fname); 	// free string name
		free(file); 		// finally, free the file descriptor
	}
	
	return 0;
}

void printFileTree() {
	MultiFile *file;
	ClientHandle *client;
	int i, j;
	
	printf("\n\nTREE: \n");
	for (i=0; i<fileList.length; i++) {
		file = getFile(linkedListGet(&fileList, i));
		printf("\tFNAME: %s\n\tFD:    %d\n\tMAXAC: %c\n\tWRITE: %d\n\tREFCT: %d\n\tOWNED:\n", file->fname, file->fd, file->access, file->write, file->refcount);
		
		for (j=0; j<file->owners->length; j++) {
			client = getClient(linkedListGet(file->owners, j));
			printf("\t\tFD: %d\n\t\tAC: %c\n\t\tRW: %d\n\n", client->fd, client->access, client->permission);
		}
	}
}

/****************************************************************************************************
 * 																									*
 * Function implementations																			*
 * 																									*
 * Code below implements each function that the server will provide being,							*
 * netopen, netclose, netread, netwrite.															*
 * 																									*
 ****************************************************************************************************/

void error(char *msg) {
    perror(msg);
    exit(0);
}

/**
 * Opens file for a given client. If successful, it will return a valid
 * file descriptor to return to the client. On failure, this method will
 * return -1, and errno will be set appropriately.
 */
int openFile(const char *fname, int flags, int clientfd, char access) {
	MultiFile *file;
	int retfd = -1;
	// acquire lock 
	pthread_mutex_lock(&fileLock);
	file = getFileByName(fname);
	
	// file cannot be opened for some reason, so return with errno
	if (file == NULL) goto OPENEND;
	if (addOwner(file, flags, clientfd, access) == -1) goto OPENEND;
	retfd = file->fd;
	
	OPENEND:
	//printFileTree();
	// return lock, and return file descriptor (or -1 if it was an error)
	pthread_mutex_unlock(&fileLock);
	return retfd;
}

/**
 * Close file for a given client. If successful, it will return 0. 
 * On failure, this method will return -1, and errno will be set appropriately.
 */
int closeFile(int fd, int clientfd) {
	MultiFile *file;
	int retval = -1;
	// acquire lock 
	pthread_mutex_lock(&fileLock);
	file = getFileByFD(fd);

	// file cannot be opened for some reason, so return with errno
	if (file == NULL) goto CLOSEND;
	if (removeOwner(file, clientfd) == -1) goto CLOSEND;
	retval = 0;
	
	CLOSEND:
	//printFileTree();
	// return lock, and return status
	pthread_mutex_unlock(&fileLock);
	return retval;
}

/**
 * Reads data from file 
 * 
 * Returns malloc()'ed string with file data on success
 * Return NULL on failure with errno set accordingly
 */
char *readFile(int fd, int clientfd) {
	MultiFile *file;
	char *data = NULL;
	
	int bytesread = -1;
	
	pthread_mutex_lock(&fileLock);
	file = getFileByFD(fd);
	
	if (file == NULL) goto READEND;
	
	if (hasAccess(file, clientfd, O_RDONLY) == 1 || hasAccess(file, clientfd, O_RDWR) == 1) {
		// go to end of file to get length
		int len = lseek(file->fd, 0, SEEK_END);
		// go to start of file
		lseek(file->fd, 0, SEEK_SET);
		data = malloc(len);
		read(file->fd, data, len);
	} else {
		errno = EACCES;
	}
	READEND:
	// free lock and return
	pthread_mutex_unlock(&fileLock);
	return data;
}

/**
 * writes data to a file
 * 
 * Returns number of bytes written on success
 * Return -1 on failure, with errno set appropriately
 */
int writeFile(int fd, int clientfd, char *buf) {
	MultiFile *file;
	
	int bytesread = -1;
	
	pthread_mutex_lock(&fileLock);
	file = getFileByFD(fd);
	
	if (file == NULL) goto WRITEND;
	
	if (hasAccess(file, clientfd, O_WRONLY) == 1 || hasAccess(file, clientfd, O_RDWR) == 1) {
		// go to start of file
		lseek(file->fd, 0, SEEK_SET);
		bytesread = write(file->fd, buf, strlen(buf));
	} else {
		errno = EACCES;
	}
	WRITEND:
	// free lock and return
	pthread_mutex_unlock(&fileLock);
	return bytesread;
}

/****************************************************************************************************
 * 																									*
 * Client communication helper functions															*	
 * 																									*
 * Functions below implement the protocol for communication between client							*
 * and server. These include getMessage(), sendReponse() and sendResponseInt()						*
 * 																									*
 ****************************************************************************************************/

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
	
	printf("%d -> '%s'\n", fd, msg);
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
	printf("%d <- '%s'\n", fd, msg);
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

/****************************************************************************************************
 * 																									*
 * Client handling functions																		*
 * 																									*		
 * Functions below include the main method which accepts incoming connections,						*
 * spawns off a new worker thread, which then handles the client in the 							*
 * handleClient function.																			*
 * 																									*
 ****************************************************************************************************/
 
int convertToStandard(char md) {
	if (md == MODE_RD) return O_RDONLY;
	if (md == MODE_WR) return O_WRONLY;
	if (md == MODE_RW) return O_RDWR;
	return -1;
}

void *handleClient(void *ptr) {
	LinkedList *files;
	int clientfd = * ((int *) ptr);
	int msglen, running = 1;
	char *inmsg, access;

	// read opening msg from client
	inmsg = getMessage(clientfd);
	
	if (inmsg == NULL) return NULL;
	
	// handles initial connection to client
	if (inmsg[0] == MODE_UNRESTRCT) {
		access = MODE_UNRESTRCT;
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else if (inmsg[0] == MODE_EXCLUSIVE) {
		access = MODE_EXCLUSIVE;
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else if (inmsg[0] == MODE_TRANSACTN) {
		access = MODE_TRANSACTN;
		sendResponse(clientfd, STATUS_SUCCESS, "");
	} else {
		sendResponseInt(clientfd, STATUS_FAILURE, INVALID_FILE_MODE);
		close(clientfd);
		running = 0;
	}
	
	free(inmsg);
	files = calloc(sizeof(LinkedList), 1);
	// loop to handle any number of requests from client
	while (running) {
		//printFileTree();
		inmsg = getMessage(clientfd);
		
		if (inmsg == NULL) break;
		
		if (inmsg[0] == FN_OPEN) {
			// open a file
			msglen = strlen(inmsg);
			inmsg[msglen - 2] = '\0';
			int val = openFile(inmsg + 2, convertToStandard(inmsg[msglen-1]), clientfd, access);
			if (val == -1) {
				sendResponseInt(clientfd, STATUS_FAILURE, errno);
			} else {
				sendResponseInt(clientfd, STATUS_SUCCESS, -val);
				linkedListAdd(files, getFileByName(inmsg+2));
			}
		} else if (inmsg[0] == FN_CLOSE) {
			// close a specific file
			int fd = -atoi(inmsg + 2);
			int val = closeFile(fd, clientfd);
			if (val == -1) {
				sendResponseInt(clientfd, STATUS_FAILURE, errno);
			} else {
				sendResponseInt(clientfd, STATUS_SUCCESS, -fd);
				linkedListRemove(files, getFileByFD(fd));
			}
		} else if (inmsg[0] == FN_READ) {
			// read data and send to client
			int fd = -atoi(inmsg + 2);
			char *data = readFile(fd, clientfd);
			if (data == NULL) {
				sendResponseInt(clientfd, STATUS_FAILURE, errno);
			} else {
				sendResponse(clientfd, STATUS_SUCCESS, data);
			}
		} else if (inmsg[0] == FN_WRITE) {
			// write data to file. data is sent in two messages
			int fd = -atoi(inmsg + 2);
			free(inmsg);
			inmsg = getMessage(clientfd);
			int len = strlen(inmsg);
			inmsg[len-1] = '\0';
			int bytes = writeFile(fd, clientfd, inmsg+2);
			if (bytes == -1) {
				sendResponseInt(clientfd, STATUS_FAILURE, errno);
			} else {
				sendResponseInt(clientfd, STATUS_SUCCESS, bytes);
			}
		}
		
		free(inmsg);
	}
	
	printf("Closed connection FD: %d\n", clientfd);
	
	MultiFile *file;
	int i;
	for (i=0; i<files->length; i++) {
		file = getFile(linkedListGet(files, i));
		closeFile(clientfd, file->fd);
	}
	
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
	
	// initialize mutex
	if (pthread_mutex_init(&fileLock, NULL) != 0) error("\nMutex init failed\n");
    
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