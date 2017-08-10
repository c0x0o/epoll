#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <time.h>

#define MAX_PACKET_SIZE 2048
#define PACKET_HEAD ((int)(sizeof(int)+sizeof(time_t)))
#define MAX_PACKET_BODY (MAX_PACKET_SIZE - PACKET_HEAD)

struct packet {
    int length;
    time_t sendTime;
    char *body;
};

#endif
