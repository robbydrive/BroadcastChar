#include "headers.h"
/*
 * EPOLLIN - вычитывать, пока read не вернёт -1 и EWOULDBLOCK
 * Заводить циклобуфер для каждого клиента (или связный список)
 * EPOLLOUT - писать в сокет из циклобуфера, пока send != -1 && errno != EWOULDBLOCK
 * Заводить в контексте флаг writable (i.e.), менять его только при полной отправке
 * Хранить в контексте также id сокета
 */

typedef struct sockaddr_in addr_struct;
int BUFSIZE = 1024;
int MAXEVENTS = 1000;

typedef struct {

} message_node;

typedef struct {
    int writable;
    message_node *message;
    int fd;
} context;

context * create_context(int sockfd)
{
    context *ptr = (context *)malloc(sizeof(context));
    ptr->writable = 0;
    ptr->message = NULL;
    ptr->fd = sockfd;
    return ptr;
}

int setnonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return -1;
    flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int register_fd(int epfd, int fd)
{
    struct epoll_event event;
    event.events = EPOLLET | EPOLLIN | EPOLLOUT;
    event.data.ptr = create_context(fd);
    int errcode = setnonblocking(fd);
    if (errcode == -1)
    {
        printf("Failed to set fd %d nonblocking\n", fd);
        exit(1);
    }
    errcode = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
    if (errcode == -1)
        printf("Failed registering new connection\n");
    else
        printf("Created new connection\n");
    return errcode;
}

int main()
{
    int listen_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    addr_struct addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &addr.sin_addr);
    addr.sin_port = 50801;
    int errcode = bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));
    if (errcode == -1)
    {
        printf("Bind failure");
        exit(1);
    }
    else
    {
        printf("Bind ok\n");
        printf("%s\n", inet_ntoa(addr.sin_addr));
        printf("%d\n", addr.sin_port);
    }

    errcode = listen(listen_sock, 10);
    if (errcode == -1)
    {
        printf("Listen failed");
        exit(1);
    }

    int epfd = epoll_create(1000),
        nevents = 0,
        accepted_fd;

    struct epoll_event event, events[MAXEVENTS];
    event.events = EPOLLET | EPOLLIN;
    event.data.ptr = create_context(listen_sock);
    errcode = epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &event);
    while (1)
    {
        if ((nevents = epoll_wait(epfd, events, MAXEVENTS, 20)) == 0)
            continue;
        else if (nevents == -1)
            exit(1);

        for (int i = 0; i < nevents; ++i)
        {
            context *cptr = (context *)events[i].data.ptr;
            if (cptr->fd == listen_sock)
            {
                struct sockaddr_in peer_addr;
                socklen_t addr_size;
                memset(&peer_addr, 0, sizeof(peer_addr));
                while ((accepted_fd = accept(listen_sock, (struct sockaddr *)&peer_addr, &addr_size)) != -1)
                    register_fd(epfd, accepted_fd);
            }
            else
            {
                switch (events[i].events)
                {
                    case EPOLLIN:

                        break;
                    case EPOLLOUT:

                        break;
                    default:
                        printf("Strange event happened");
                        break;
                }
            }
        }
    }

    /*
    printf("Waiting connection\n");
    errcode = listen(sockfd, 1);
    if (errcode == -1)
        printf("Listen failed\n");

    addr_struct peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, sizeof(peer_addr));
    int peer_fd = accept(sockfd, (struct sockaddr *)&peer_addr, &peer_addr_len);

    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    ssize_t received = recv(peer_fd, buf, BUFSIZE, 0);

    write(1, (void *)buf, received);

    close(sockfd);
    */
    exit(0);
}
