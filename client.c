#include "headers.h"

typedef struct sockaddr_in addr_struct;
int BUFSIZE = 1024;

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    addr_struct addr, peer_addr;
    memset(&addr, 0, sizeof(addr));
    memset(&peer_addr, 0, sizeof(peer_addr));
    addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &addr.sin_addr);
    addr.sin_port = 51001;
    int errcode = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (errcode == -1)
    {
        printf("Bind failed\n");
    }
    else
    {
        printf("Bind successful\n");
        printf("%s\n", inet_ntoa(addr.sin_addr));
        printf("%d\n", addr.sin_port);
    }
    peer_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &peer_addr.sin_addr);
    peer_addr.sin_port = 50801;
    errcode = connect(sockfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
    if (errcode == -1)
        printf("Connection failed\n");
    else
        printf("Sending data\n");
    char buf[] = "Hello!\n";
    ssize_t written = send(sockfd, buf, strlen(buf), 0);
    if (written > 0)
        printf("Successful write\n");
    else
        printf("Failed\n");
    char buffer[BUFSIZE];
    ssize_t read_bytes = read(sockfd, buf, BUFSIZE);
    if (read_bytes > 0)
        printf("Received data: %s\n", buffer);
    else
        printf("Error while reading\n");
    close(sockfd);
    return 0;
}
