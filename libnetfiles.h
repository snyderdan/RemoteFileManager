
#include <errno.h>

/**
 * 
 * The first 8 bytes of any message is the binary length of the following message
 * 
 * If the server responds with a status of 'F' then the data portion of the response
 * is an error code, corresponding to errno.h. If it responds with 'S', then the data 
 * is whatever specified below.
 * 
 * On connect:
 *  Client->Server
 *  - 1 byte mode 'U'/'E'/'T'
 *  - 1 byte separator
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte sep
 *  - n bytes error if any
 * 
 * Open:
 *  Client->Server
 * 	- 1 byte function 'O'
 *  - 1 byte sep
 *  - n bytes file name
 *  - 1 byte sep
 *  - 1 byte mode
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 *  - n byte FD/error condition
 * 
 * Close:
 *  Client->Server
 * 	- 1 byte function 'C'
 *  - 1 byte sep
 *  - 8 byte file descriptor
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 *  - n byte FD/error condition
 * 
 * Read:
 *  Client->Server
 * 	- 1 byte function 'R'
 *  - 1 byte sep
 *  - 8 byte file descriptor
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 *  - n bytes data or error condition
 * 
 * Write:
 *  Client->Server
 * 	- 1 byte function 'W'
 *  - 1 byte sep
 *  - 8 byte file descriptor
 *  - 1 byte sep
 *  - n bytes data
 *  Server->Client
 *  - 1 byte status
 *  - 1 byte separator
 *  - 8 byte length/error condition
 */

#ifndef __LIBNETFILES_H
#  define __LIBNETFILES_H

#  define MODE_UNRESTRCT '0'
#  define MODE_EXCLUSIVE '1'
#  define MODE_TRANSACTN '2'

#  define MODE_RD  'R'
#  define MODE_WR  'W'
#  define MODE_RW  'B'

#  define FN_OPEN  'O'
#  define FN_CLOSE 'C'
#  define FN_WRITE 'W'
#  define FN_READ  'R'
#  define SEP_CHAR ','

#  define STATUS_SUCCESS 'S'
#  define STATUS_FAILURE 'F'

#  define PORT_NUM 20000

#  define INVALID_FILE_MODE -55

int netopen(const char *pathname, int flags);
ssize_t netread(int fd, void *buf, size_t size);
ssize_t netwrite(int fd, const void *buf, size_t size);
int netclose(int fd);

int netserverinit(char * hostname, int filemode);

#endif