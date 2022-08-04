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

extern bool validVidConn;
extern int sockfdV;

size_t readLength()
{
    // read length
    char buff1[4];
    memset(buff1, 0, 4);
    int n = recv(sockfdV, buff1, 4, 0);
            
    if (n < 1)
        return -1;
    
    if (n != 4)
    {
        printf("Unexpected data: %d\n", n);
        return -1;
    }

    unsigned int v1 = buff1[0] & 0xff;
    unsigned int v2 = buff1[1] & 0xff;
    unsigned int v3 = buff1[2] & 0xff;
    
    long len = v3 * 65536 + v2 * 256 + v1; // TODO ntohl
    return len;
}

void *vidThread(void *p)
{
	if (!validVidConn)
		return nullptr;

    int filenum = 0; // debugging: write image to file

    while (1) // until exception from closed socket(?)
    {
		if (!validVidConn)
			return nullptr;
			
        size_t len = readLength();
        if (len < 1) continue;
        
        unsigned char *buffer = (unsigned char *)malloc(len);
        size_t toRead = len;
        unsigned char *bufPtr = buffer;
        int readSize = 4096;
        size_t totRead = 0;
        while (toRead > 0)
        {
            int bytesRead = recv(sockfdV, bufPtr, readSize, 0);
            if (bytesRead < 0)
                break; // TODO better error handling
            bufPtr += bytesRead;
            totRead += bytesRead;
            toRead -= bytesRead;
            if (toRead < 4096)
                readSize = toRead;
        }
        
        // buffer now contains the image
        char fn[12];
        sprintf(fn, "vid%03d.jpg", filenum);
        FILE *f = fopen(fn, "wb");
        fwrite(buffer, 1, totRead, f);
        fclose(f);
        filenum++;
    }
    
    return nullptr;
}
