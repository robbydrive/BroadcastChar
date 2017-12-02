#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

int logger_init(const char *filename);

void log_message_receival(int sockfd, const char *message, size_t n);
void log_message_sending(int sockfd, const char *message, size_t n);
