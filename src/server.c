#include "doepoll.h"

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

    for (i = 0; i <= max_conn_id; i++) {
        if (conns[i] && conns[i]->fd == fd) {
            conns[i] = NULL;
            return 0;
        }
    }

    return -1;
}

// usage:
// server               default ip: 127.0.0.1 port: 50427
// server [port]        default ip: 127.0.0.1
// server [ip] [port]
int main(int argc, char **argv) {
    struct sockaddr_in server;
    int retval;
    int listenfd, epollfd;
    struct bipbuffer *bip;
    struct buffer *buffP, *connections[MAX_CONNECTIONS];
    struct epoll_event ev, events[MAX_EVENTS];

    memset(connections, 0, sizeof(struct buff *)*MAX_CONNECTIONS);

    // set server
    server.sin_family = AF_INET;
    switch(argc) {
        case 1:
            retval = inet_pton(AF_INET, SERVER_DEFAULT_ADDR_STR, &server.sin_addr.s_addr);
            if (retval <= 0) {
                printf("receive an invalid ip address\n");
                return 0;
            }
            server.sin_port = htons(SERVER_DEFAULT_PORT);
            break;
        case 2:
            server.sin_addr.s_addr = INADDR_ANY;
            sscanf(argv[1], "%hu", &server.sin_port);
            server.sin_port = htons(server.sin_port);
            break;
        case 3:
        default:
            retval = inet_pton(AF_INET, argv[1], &server.sin_addr.s_addr);
            if (retval <= 0) {
                printf("receive an invalid ip address\n");
                return 0;
            }
            sscanf(argv[2], "%hu", &server.sin_port);
            server.sin_port = htons(server.sin_port);
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("create socket failed");
        exit(EXIT_FAILURE);
    }
    set_none_blocking(listenfd);

    retval = bind(listenfd, (struct sockaddr *)&server, sizeof(server));
    if (retval < 0) {
        perror("bind address to socket failed");
        exit(EXIT_FAILURE);
    }

    retval = listen(listenfd, 100);
    if (retval < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    epollfd = epoll_create(1);
    if (epollfd < 0) {
        perror("create epoll fd failed");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN | EPOLLET;
    buffP = new_buffer();
    buffP->fd = listenfd;
    buffP->tasks = NULL;
    bip = bb_create(BUFFER_SIZE);
    buffP->bip = bip;
    ev.data.ptr = buffP;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);

    connections[get_first_unused(connections)] = buffP;

    while (1) {
        int nfds, i, j;

        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            perror("epoll wait error");
            exit(EXIT_FAILURE);
        }

        for (i = 0; i < nfds; i++) {
            struct buffer *current_buffer = (struct buffer *)events[i].data.ptr;
            int fd = current_buffer->fd;

            if (fd == listenfd && events[i].events & EPOLLIN) {
                // receive a new connection
                int connfd;
                struct sockaddr_in client;
                socklen_t socklen = sizeof(struct sockaddr_in);

                connfd = accept(listenfd, (struct sockaddr *)&client, &socklen);
                if (connfd < 0) {
                    perror("accept failed");
                    exit(EXIT_FAILURE);
                }
                set_none_blocking(connfd);

                buffP = new_buffer();
                buffP->fd = connfd;
                buffP->tasks = NULL;
                buffP->bip = bb_create(BUFFER_SIZE);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = buffP;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);

                connections[get_first_unused(connections)] = buffP;

            } else if (events[i].events & EPOLLIN) {
                // receive new data
                retval = recv_data(fd, current_buffer);
                if (retval < 0) {
                    if (errno == ENOBUFS) continue;

                    // error encountered
                    perror("recv_data failed");
                    free_buffer(current_buffer);
                    delete_connection(connections, fd);
                    close(fd);
                } else if (retval == 0) {
                    // disconnect normally
                    printf("receive EOF from peer fd %d\n", fd);
                    free_buffer(current_buffer);
                    delete_connection(connections, fd);
                    close(fd);
                } else {
                    struct task *taskP = current_buffer->tasks;

                    while (taskP != NULL) {
                        if (!taskP->printed) {
                            print_packet(taskP->packet);
                            taskP->printed = 1;
                        }
                        taskP = taskP->next;
                    }
                }
            }

            // send all data
            for (j = 0; j <= max_conn_id; j++) {
                if (connections[j] != NULL) {
                    int fd = (*connections[j]).fd;
                    retval = send_data(fd, connections[j]);
                    if (retval < 0) {
                        // error encountered
                        perror("send_data failed");
                        free_buffer(connections[j]);
                        delete_connection(connections, fd);
                        close(fd);
                    } else {
                        printf("%d bytes data echo to fd %d\n", retval, fd);
                    }
                }
            }
        }
    }

    return 0;
}
