#include "doepoll.h"

void free_packet(struct packet *packet) {
    free(packet);
}

void free_queue(struct task *task) {
    struct task *temp;

    while (task != NULL) {
        temp = task;
        task = task->next;
        free_packet(temp->packet);
        free(temp);
    }
}

void free_buffer(struct buffer *buff) {
    free_queue(buff->tasks);
    bb_destroy(buff->bip);
    free(buff);
}

void print_packet(struct packet *packet) {
    printf("new packet incoming:\n");
    printf("length: %d\n", packet->length);
    printf("send at: %lu\nbody:", packet->sendTime);

    fflush(stdout);

    write(1, packet->body, packet->length);
    printf("\n");

    fflush(stdout);
}

int recv_data(int fd, struct buffer *buffP) {
    int recved = 0, nBytes;
    char *base = NULL;
    struct bipbuffer *bip = buffP->bip;

    // receive data
    while (1) {
        base = bb_alloc(bip, BUFFER_BLK_SIZE);
        if (base == NULL) {
            errno = ENOBUFS;
            return -1;
        }

        nBytes = recv(fd, base, BUFFER_BLK_SIZE, 0);
        if (nBytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // all data has been read
                break;
            }

            return -1;
        } else if (nBytes == 0) {
            // an eof received
            return 0;
        } else {
            recved += nBytes;

            nBytes = bb_commit(bip, nBytes);
            if (nBytes < 0) {
                errno = ENOBUFS;
                return -1;
            }
        }
    }

    // generate packet from buffer
    while (1) {
        struct task *taskP = new_task(), *tmp = buffP->tasks;
        struct packet *packet = new_packet();
        char *body = NULL;

        // check whether there are enough data
        nBytes = bb_look(bip, packet, PACKET_HEAD);
        if (nBytes < 0) {
            // no enough data
            break;
        }

        body = (char *)malloc(packet->length);
        nBytes = bb_look(bip, body, packet->length);
        if (nBytes < 0) {
            errno = ENOBUFS;
            return -1;
        }

        nBytes = bb_read(bip, packet, PACKET_HEAD);
        nBytes = bb_read(bip, body, packet->length);

        packet->body = body;

        taskP->packet = packet;
        taskP->printed = 0;
        taskP->next = NULL;

        if (buffP->tasks == NULL) {
            buffP->tasks = taskP;
        } else {
            while (tmp->next != NULL) {
                tmp = tmp->next;
            }
            tmp->next = taskP;
        }
    }

    return recved;
}

int send_data(int fd, struct buffer *buff) {
    struct task *taskP = buff->tasks, *prev;
    int sent = 0, nBytes;
    char send_buffer[MAX_PACKET_SIZE];

    while (taskP != NULL) {
        struct packet *packet = taskP->packet;

        // send this packet
        memcpy(send_buffer, packet, PACKET_HEAD);
        memcpy(send_buffer+PACKET_HEAD, packet->body, packet->length);

        nBytes = send(fd, send_buffer, packet->length+PACKET_HEAD, 0);
        if (nBytes < 0) {
            return -1;
        } else {
            prev = taskP;
            taskP = taskP->next;
            buff->tasks = buff->tasks->next;

            sent += nBytes;

            free_packet(packet);
            free(prev);
        }
    }

    return sent;
}

int send_packet(int fd, struct packet *packetP) {
    int sent = 0, nBytes;
    char send_buffer[MAX_PACKET_SIZE];

    // send this packet
    memcpy(send_buffer, packetP, PACKET_HEAD);
    memcpy(send_buffer+PACKET_HEAD, packetP->body, packetP->length);

    nBytes = send(fd, send_buffer, packetP->length+PACKET_HEAD, 0);
    if (nBytes < 0) {
        return -1;
    } else {
        sent += nBytes;

        free_packet(packetP);
    }

    return sent;
}

int set_none_blocking(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return -1;
    }

    flags = fcntl(fd, F_SETFL, O_NONBLOCK | flags);
    if (flags < 0) {
        return -1;
    }

    return 0;
}
