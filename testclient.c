#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "libnetfiles.h"


int main(int argc, char *argv[])
{
	netserverinit("localhost", MODE_UNRESTRCT);
	int fd = netopen("test1.txt", MODE_RW);
	char buf[20];
	netread(fd, buf, 20);
	printf(buf);
	buf[0] = 'H';
	netwrite(fd, buf, 20);
	netclose(fd);
	printf("\nRIGHT ON\n");
}
