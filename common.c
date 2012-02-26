#include <sys/socket.h>
#include <errno.h>
// #include <stdio.h>

int dccp_send(int sockfd, char *buf, int len) {
    int stat;

    do {
        stat = send(sockfd, buf, len, 0);
    } while ((stat < 0) && (errno == EAGAIN));

//     perror("dccp_send");
    return stat;
}
