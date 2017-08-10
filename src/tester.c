#include "doepoll.h"
#include <getopt.h>
#include <netdb.h>
#include <sys/stat.h>

#define MAX_EVENTS 200
#define MAX_CONNECTIONS 10000

int max_conn_id = 0;

int get_first_unused(struct buffer **conns) {
    int i;

    for (i = 0; i < MAX_CONNECTIONS; i++) {
        if (conns[i] == NULL) {
            if (i > max_conn_id) {
                max_conn_id = i;
            }

            return i;
        }
    }

    return -1;
}

int delete_connection(struct buffer **conns, int fd) {
    int i;

    for (i = 0; i < MAX_CONNECTIONS; i++) {
        if (conns[i]->fd == fd) {
            conns[i] = NULL;
            return 0;
        }
    }

    return -1;
}

int connect_to(const char *ip, const char *port) {
    int retval;
    int sock;
    struct addrinfo hint;
    struct addrinfo *res, *chain;

    memset(&hint, 0, sizeof(struct sockaddr));
    hint.ai_flags = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;
    hint.ai_flags = AI_NUMERICSERV | AI_NUMERICHOST;

    retval = getaddrinfo(ip, port, &hint, &res);
    if (retval < 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(retval));
        return -1;
    }
    chain = res;

    while (res != NULL) {
        sock = socket(res->ai_family, res->ai_socktype, 0);
        if (sock < 0) {
            continue;
        }

        retval = connect(sock, res->ai_addr, res->ai_addrlen);
        if (retval == 0) {
            break;
        }

        close(sock);

        res = res->ai_next;
    }

    freeaddrinfo(chain);

    if (res == 0) {
        return -1;
    }

    return sock;
}

struct packet *generate_packet(const char *path) {
    struct packet *packet = new_packet();
    char *body;
    int len;

    packet->sendTime = time(NULL);

    if (path == NULL) {
        // manual mode
        char input[BUFFER_SIZE];

        printf("> ");
        scanf("%s", input);
        len = strlen(input);

        body = (char *)malloc(len);
        memcpy(body, input, len);

        packet->length = len;
        packet->body = body;
    } else {
        // auto mode
        long int nBytes;
        int fd;
        struct stat stat;

        fd = open(path, O_RDONLY);
        if (fd < 0) {
            free_packet(packet);
            return NULL;
        }

        fstat(fd, &stat);
        body = (char *)malloc(stat.st_size);

        nBytes = read(fd, body, stat.st_size);
        if (nBytes < 0) {
            free_packet(packet);
            free(body);
            return NULL;
        }
        close(fd);

        packet->length = stat.st_size;
        packet->body = body;
    }

    return packet;
}

void print_usage() {
    printf("Usage: tester [OPTIONS] [IP PORT]\n");
    printf("OPTIONS:\n");
    printf("   -t num       max number of request, default: 1\n");
    printf("   -f filename  read data from 'filename', default to read from stdin\n");
    printf("HINTS:\n");
    printf("1. To test whether the server works fine(rather than do a pressure test),\n");
    printf("   use tester without any OPTIONS\n");
}

int main(int argc, char **argv) {
    char ch, *file_path = NULL;
    char *server_ip = SERVER_DEFAULT_ADDR_STR, *server_port = SERVER_DEFAULT_PORT_STR;
    int conn_nums = 1, sock;
    int i, nBytes, j, retval, running = 1;

    struct buffer *buffP, *connections[MAX_CONNECTIONS];
    struct epoll_event ev, events[MAX_EVENTS];
    struct packet *packetP = NULL;
    int epollfd;

    memset(connections, 0, sizeof(struct buffer *)*MAX_CONNECTIONS);

    // handle arguments
    while ((ch = getopt(argc, argv, "t:f:h")) != -1) {
        switch (ch) {
            case 't':
                sscanf(optarg, "%d", &conn_nums);
                break;
            case 'f':
                file_path = optarg;
                break;
            case 'h':
            default:
                print_usage();
                return 0;
        }
    }

    if (argc - optind >= 2) {
        server_ip = argv[optind++];
        server_port = argv[optind];
    }

    // create epoll handler
    epollfd = epoll_create(1);
    if (epollfd < 0) {
        perror("create epoll fd failed");
        exit(EXIT_FAILURE);
    }

    // create connections
    for (i = 0; i < conn_nums; i++) {
        sock = connect_to(server_ip, server_port);
        if (sock < 0) {
            continue;
        }
        set_none_blocking(sock);

        buffP = new_buffer();
        buffP->bip = bb_create(BUFFER_SIZE);
        buffP->fd = sock;
        buffP->tasks = NULL;

        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = buffP;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev);

        connections[get_first_unused(connections)] = buffP;
    }

    // main loop
    while (running) {
        int nfds;

        running = 0;

        // send all data
        for (j = 0; j <= max_conn_id; j++) {
            if (connections[j] != NULL) {
                int fd = (*connections[j]).fd;

                packetP = generate_packet(file_path);

                retval = send_packet(fd, packetP);
                if (retval < 0) {
                    if (errno == EAGAIN) continue;

                    // error encountered
                    perror("send_packet failed");
                    free_buffer(connections[j]);
                    delete_connection(connections, fd);
                    close(fd);
                } else if (retval == 0) {
                } else {
                    printf("%d bytes data sent to fd %d\n", retval, fd);
                }

                running = 1;
            }
        }

        if (!running) {
            break;
        }

        // receive data
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < nfds; i++) {
            struct buffer *current_buffer = (struct buffer *)events[i].data.ptr;
            int fd = current_buffer->fd;

            if (events[i].events & EPOLLIN) {
                nBytes = recv_data(fd, current_buffer);
                if (nBytes < 0) {
                    if (errno == ENOBUFS) continue;
                    perror("recv_data failed");

                    free_buffer(current_buffer);
                    delete_connection(connections, fd);
                    close(fd);
                    continue;
                } else if (nBytes == 0) {
                    printf("disconnect fd %d\n", fd);

                    free_buffer(current_buffer);
                    delete_connection(connections, fd);
                    close(fd);
                    continue;
                } else {
                    struct task *taskP = current_buffer->tasks;
                    struct task *prevP = NULL;

                    while (taskP != NULL) {
                        print_packet(taskP->packet);
                        prevP = taskP;
                        taskP = taskP->next;

                        free_packet(prevP->packet);
                        free(prevP);
                    }

                    current_buffer->tasks = NULL;
                }
            }
        }
    }

    printf("all connections break down, test procedure terminated\n");

    return 0;
}
