
// A thread whose sole purpose is to send the
// server the 'power' query periodically.

#include <stdio.h>
#include <stdlib.h>
#include <mutex>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <FL/Fl.H> // Fl::awake

extern bool validConnect;
extern int  sockfd;

void *powerThread(void *)
{
    while (1)
    {
        if (!validConnect)
            return nullptr;

        send(sockfd, "CMD_POWER\n", 10, 0);   
        
        sleep(15); // wait seconds before querying again
    }
    return nullptr;
}
