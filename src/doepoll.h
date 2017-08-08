#ifndef _DOEPOLL_H_
#define _DOEPOLL_H_

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "bipbuffer.h"

#define MAX_PACKET_SIZE 2048
#define PACKET_HEAD ((int)(sizeof(int)+sizeof(time_t)))
#define MAX_PACKET_BODY (MAX_PACKET_SIZE - PACKET_HEAD)
#define BUFFER_SIZE (2048*10)
#define BUFFER_BLK_SIZE 64

#define SERVER_DEFAULT_ADDR_STR "127.0.0.1"
#define SERVER_DEFAULT_PORT_STR "50427"
#define SERVER_DEFAULT_PORT 50427

struct packet {
    int length;
    time_t sendTime;
    char *body;
};

struct task {
    struct packet *packet;
    struct task *next;
    int printed;
};

struct buffer {
    struct task *tasks;
    struct bipbuffer *bip;
    int fd;
};

#define new_type(type) ((type *)malloc(sizeof(type)))

#define new_packet() new_type(struct packet)
#define new_task() new_type(struct task)
#define new_buffer() new_type(struct buffer)

void free_packet(struct packet *packet);
void free_queue(struct task *task);
void free_buffer(struct buffer *buff);

void print_packet(struct packet *packet);

int recv_data(int connfd, struct buffer *buffP);
int send_data(int connfd, struct buffer *buffP);
int send_packet(int connfd, struct packet *packetP);

int set_none_blocking(int fd);

#endif
