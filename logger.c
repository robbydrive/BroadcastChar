#include "logger.h"

static int logger_fd;
extern int BUFSIZE;

typedef enum { RECEIVAL, SENDING } message_processing;

int logger_init(const char *filename)
{
    char pathname[] = "logs/";
    mode_t required_mode = S_IRWXU | S_IROTH | S_IRGRP | S_IWGRP;
    if (opendir(pathname) == NULL)
    {
        if (mkdir(pathname, required_mode) == -1)
            return -1;
    }
    size_t name_size = 80;
    char fullname[name_size];
    if (snprintf(fullname, name_size, "%s%s", pathname, filename) < 0)
        return -1;
    logger_fd = open(fullname, O_CREAT | O_RDWR | O_EXCL | O_TRUNC, required_mode & ~S_IXUSR);
    if (logger_fd == -1)
        return -1;
    return 0;
}

void _log_message(int sockfd, const char *message, size_t n, message_processing type)
{
    if (logger_fd == -1)
        return;

    size_t prefix_length = (type == RECEIVAL ? 25 : 20);
    char buffer[n + BUFSIZE + prefix_length];
    int printed = snprintf(buffer, n + BUFSIZE + prefix_length,
            (type == RECEIVAL ? "RECEIVED from socket %d: %s" : "SENT to socket %d: %s"), sockfd, message);
    ssize_t total_written = 0,
            written = 0;
    while ((written = write(logger_fd, buffer, printed + 1)) > 0)
        if ((total_written += written) >= printed)
            break;
}

void log_message_receival(int sockfd, const char *message, size_t n)
{
    _log_message(sockfd, message, n, RECEIVAL);
}

void log_message_sending(int sockfd, const char *message, size_t n)
{
    _log_message(sockfd, message, n, SENDING);
}
