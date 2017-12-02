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
int MESSAGESIZE = 1024;
int MAXEVENTS = 1000;

typedef struct msg message_node;

struct msg {
    char *buffer;
    message_node *next;
};

typedef struct {
    int writable;
    message_node *message;
    int fd;
} context;

typedef struct context_node {
    context *cptr;
    struct context_node *next;
} context_node;

static context_node *root_context = NULL;

message_node* create_message()
{
    message_node *message = (message_node *)malloc(sizeof(message));
    message->buffer = (char *)malloc(MESSAGESIZE * sizeof(char));
    message->next = NULL;
    return message;
}

context* create_context(int sockfd, int is_listener)
{
    context *ptr = (context *)malloc(sizeof(context));
    ptr->writable = 0;
    ptr->message = NULL;
    ptr->fd = sockfd;
    if (is_listener)
        return ptr;
    context_node *current = root_context;
    if (current == NULL)
    {
        root_context = (context_node *)malloc(sizeof(context_node));
        root_context->cptr = ptr;
        root_context->next = NULL;
    }
    else
    {
        while (current->next != NULL)
            current = current->next;
        current->next = (context_node *)malloc(sizeof(context_node));
        current->next->cptr = ptr;
        current->next->next = NULL;
    }
    return ptr;
}

int delete_context(int sockfd)
{
    context_node *current_cnt = root_context,
                 *parent_cnt = NULL;
    context *ptr = NULL;
    while (current_cnt != NULL)
    {
        ptr = current_cnt->cptr;
        if (ptr->fd == sockfd)
        {
            if (ptr->message != NULL)
            {
                message_node *current_msg = ptr->message,
                             *parent_msg = NULL;
                while (current_msg != NULL)
                {
                    parent_msg = current_msg;
                    current_msg = current_msg->next;
                    free(parent_msg->buffer);
                    free(parent_msg);
                }
                ptr->message = NULL;
            }
            if (parent_cnt == NULL)
                root_context = current_cnt->next;
            else
                parent_cnt->next = current_cnt->next;
            free(current_cnt->cptr);
            free(current_cnt);
            return 0;
        }
    }
    return -1;
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
    event.events = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLRDHUP;
    event.data.ptr = create_context(fd, 0);
    ((context *)event.data.ptr)->writable = 1;
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

int unregister_fd(int epfd, int delete_fd)
{
    printf("Unregistering %d socket...\n", delete_fd);
    delete_context(delete_fd);
    int errcode = 0;
    if ((errcode = epoll_ctl(epfd, EPOLL_CTL_DEL, delete_fd, NULL)) != 0)
    {
        printf("Failed to remove socket %d from epoll instance %d\n", delete_fd, epfd);
        return errcode;
    }
    if ((errcode = close(delete_fd)) != 0)
        printf("Failed to close socket %d\n", delete_fd);
    return errcode;
}

void flush_context(context *cptr)
{
    printf("Flushing...\n");
    if (cptr->writable && cptr->message != NULL)
    {
        message_node *current = cptr->message, *parent;
        while (current != NULL)
        {
            printf("Sending data\n");
            ssize_t sent = 0,
                    total_sent = 0;
            while ((sent = send(cptr->fd, cptr->message->buffer, MESSAGESIZE, 0)) > 0)
            {
                if ((total_sent += sent) == 0 || total_sent == MESSAGESIZE)
                    break;
                printf("Message sent\n");
            }
            parent = current;
            current = current->next;
            printf("Freeing\n");
            free(parent->buffer);
            free(parent);
            printf("Freed\n");
        }
        cptr->message = NULL;
        cptr->writable = 0;
    }
}

void copy_message(message_node *root_message, int except_fd)
{
    printf("Copy message call\n");
    context_node *current_context = root_context;
    while (current_context != NULL)
    {
        printf("Copying...\n");
        if (current_context->cptr->fd == except_fd)
        {
            printf("Excepting fd\n");
            current_context = current_context->next;
            continue;
        }
        message_node *current_message = root_message, 
                     **insert_to = &(current_context->cptr->message);
        // Find place to insert new message_node
        while (*insert_to != NULL)
            if ((*insert_to)->next == NULL)
                break;
            else
                insert_to = &(*insert_to)->next;

        // Recreate message_node forward list with newly created references
        message_node *new_message = NULL;
        while (current_message != NULL)
        {
            printf("Copying message: %s\n", current_message->buffer);
            new_message = (message_node *)malloc(sizeof(message_node));
            if ((*insert_to) == NULL)
                *insert_to = new_message;
            else
            {
                (*insert_to)->next = new_message;
                insert_to = &new_message;
            }
            new_message->buffer = (char *)malloc(MESSAGESIZE * sizeof(char));
            memcpy(new_message->buffer, current_message->buffer, MESSAGESIZE);
            new_message->next = NULL;
            current_message = current_message->next;
        }

        // Send data immediately if socket is ready for writing
        if (current_context->cptr->writable)
            flush_context(current_context->cptr);

        printf("Switching contexts...\n");
        current_context = current_context->next;
    }
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
    event.data.ptr = create_context(listen_sock, 1);
    errcode = epoll_ctl(epfd, EPOLL_CTL_ADD, listen_sock, &event);
    while (1)
    {
        if ((nevents = epoll_wait(epfd, events, MAXEVENTS, 20)) == 0)
            continue;
        else if (nevents == -1)
            exit(1);

        printf("%d events happened\n", nevents);
        for (int i = 0; i < nevents; ++i)
        {
            context *cptr = (context *)events[i].data.ptr;
            printf("\nNEW EVENT\nSocket fd: %d\n", cptr->fd);
            if (cptr->fd == listen_sock)
            {
                printf("Listener branch\n");
                struct sockaddr_in peer_addr;
                socklen_t addr_size;
                memset(&peer_addr, 0, sizeof(peer_addr));
                while ((accepted_fd = accept(listen_sock, (struct sockaddr *)&peer_addr, &addr_size)) != -1)
                    register_fd(epfd, accepted_fd);
            }
            else
            {
                printf("Else branch\n");
                if (events[i].events & EPOLLRDHUP)
                {
                    flush_context(cptr);
                    unregister_fd(epfd, cptr->fd);
                    continue;
                }

                if (events[i].events & (EPOLLERR | EPOLLHUP))
                {
                    unregister_fd(epfd, cptr->fd);
                    continue;
                }

                if (events[i].events & EPOLLIN)
                {
                    message_node *message = create_message();
                    message_node *current = message,
                                 *parent = NULL;
                    char buf[BUFSIZE];
                    ssize_t read_bytes = 0;
                    while ((read_bytes = read(cptr->fd, buf, BUFSIZE)) != -1)
                    {
                        if (read_bytes == 0)
                            break;
                        memcpy(current->buffer, buf, read_bytes);
                        parent = current;
                        current->next = create_message();
                        current = current->next;
                    }
                    if (parent != NULL)
                    {
                        parent->next = NULL;
                        free(current->buffer);
                        free(current);
                    }
                    else
                    {
                        printf("Error while reading from socket %d\nEvents received: %d\n", cptr->fd, (int)events[i].events);
                        free(message->buffer);
                        free(message);
                        message = NULL;
                    }
                    if (message != NULL)
                    {
                        printf("Data received: %s\n", message->buffer);
                        copy_message(message, cptr->fd);
                        current = message;
                        while (current != NULL)
                        {
                            parent = current;
                            current = current->next;
                            free(parent->buffer);
                            free(parent);
                        }
                    }
                }

                if (events[i].events & EPOLLOUT)
                {
                    printf("Socket %d is ready to write\n", cptr->fd);
                    cptr->writable = 1;
                    flush_context(cptr);
                }
            }
        }
    }
    exit(0);
}
