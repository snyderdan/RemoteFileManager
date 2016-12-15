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
	int fd = netopen("test1.txt", MODE_RD);
	char buf[20];
	netread(fd, buf, 20);
	printf(buf);
	printf("RIGHT ON");
}
