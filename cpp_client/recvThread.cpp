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

extern void sendFLTKMsg(long);

extern bool validConnect;
extern int  sockfd;

#define UPD_POWER 1002 // TODO header
#define UPD_DIST  1003 // TODO header

int percent; // battery power percentage
char distStr[10]; // distance
extern std::mutex distMutex;

void *recvThread(void *p)
{
    char buffer[1024];
    memset(buffer, 0, 1024);
    char workbuff[1024];
    memset(workbuff, 0, 1024);
    
    if (!validConnect)
        return nullptr;
    
    while (1) // until exception from closed socket(?)
    {
        if (!validConnect)
            return nullptr;
        
        int n = recv(sockfd, buffer, 1024, 0);
        if (n < 0)
            continue;
//        printf("%d\n", n);
        
        strcat(workbuff, buffer);
        
        char *pch = strtok(workbuff, "\n");
        while (pch != NULL)
        {
            // Split on #
            char *message = strchr(pch, '#');
            
            if (pch[4] == 'S')
            {
            // 1. CMD_SONIC
            // self.Ultrasonic.setText('Obstruction:%s cm'%Massage[1])
                {
                std::lock_guard<std::mutex> guard(distMutex);
                strcpy(distStr, message+1);
                }
                sendFLTKMsg(UPD_DIST);
            }
            if (pch[4] == 'P')
            {
            // 2. CMD_POWER
            // percent_power=int((float(Massage[1])-7)/1.40*100)
                message[4] = 0; // kill extra e.g. 7.8000000001
                float val = strtof(message+1, nullptr);
                percent = (int)(((val - 7.0) / 1.40) * 100.0);
//                printf("%s - %f - %d \n", message, val, percent);
                sendFLTKMsg(UPD_POWER);
            }
            // 3. CMD_LIGHT
            // 4. incomplete message
            
//            printf("%s\n", pch);
            pch=strtok(NULL, "\n");
        }
        
        memset(buffer, 0, 1024);
        workbuff[0] = 0;
        
    }
}

