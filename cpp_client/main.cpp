#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define CMD_PORT 5000
#define IP_ADDR  "192.168.1.4"
#define MAXDATASIZE 1024

int main(int argc, char *argv[])
{
    puts("Hello, World!");
    
    struct sockaddr_in serv_addr;    
 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(CMD_PORT);

    int res = inet_pton(AF_INET, IP_ADDR, &serv_addr.sin_addr);
    if (res < 1)
    {
        puts("bad address");
        return -1;
    }
    
    int clientfd = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (clientfd < 0)
    {
        close(sockfd);
        puts("connect fail");
        return -1;
    }

    const char *msg = u8"CMD_POWER\n";
    char buffer[MAXDATASIZE] = {0};
    
    send(sockfd, msg, 10, 0);

    sleep(2);
    
    {
        int valread = read(sockfd, buffer, MAXDATASIZE);
        close(clientfd);
        close(sockfd);
        printf("Retval %d\n", valread);
        printf("Response: %s\n", buffer);
    }
    
    return 0;
}
