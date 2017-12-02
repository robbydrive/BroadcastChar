#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

int logger_init(const char *filename);
int logger_set_debug(const char *filename, int fd);

void log_message_receival(int sockfd, const char *message, size_t n);
void log_message_sending(int sockfd, const char *message, size_t n);
void log_debug(const char *format, ...);
