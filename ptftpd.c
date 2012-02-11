#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "error.h"
#include "ptftp.h"

#define BACKLOG 5 // max number of pending connections

int net_init();
void net_destroy();
int handle(int);

int main (int argc, char **argv)
{
    signal(SIGINT, net_destroy);

    int sockfd = net_init();
    handle(sockfd);

    return EXIT_SUCCESS;
}

int net_init()
{
    int sockfd, true = 1;
    struct sockaddr_in server_addr;

    /* NOTE: the new way of doing it didn't work with DCCP */
//     struct addrinfo hints, *serverinfo, *ptr;
// 
//     /* Set up socket info */
//     memset(&hints, 0, sizeof hints);
//     hints.ai_flags = AI_PASSIVE;
//     hints.ai_family = AF_UNSPEC;
//     hints.ai_socktype = SOCK_DCCP;
//     hints.ai_protocol = IPPROTO_DCCP;
// 
//     if ((res = getaddrinfo("localhost", "tftp", &hints, &serverinfo)) != 0) {
//         error_l(gai_strerror(res));
//         return ERRNO_GETADDR;
//     }
// 
//     /* Select a socket */
//     for (ptr = serverinfo; ptr != NULL; ptr = ptr->ai_next) {
//         if ((sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1) {
//             perror("socket() : remove this error output");
//             continue;
//         }
// 
//         setsockopt(sockfd, SOL_DCCP, SO_REUSEADDR, &true, sizeof(int));
// 
//         if (bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
//             close(sockfd);
//             perror("bind(): remove error");
//             continue;
//         }
// 
//         break;
//     }
// 
//     if (ptr == NULL) {
//         error_num(2);
//         return ERRNO_BIND;
//     }
// 
//     freeaddrinfo(serverinfo);

    /* Setup address info struct */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ((sockfd = socket(PF_INET, SOCK_DCCP, IPPROTO_DCCP)) == -1) {
        error(strerror(errno));
        return ERRNO_SOCK;
    }

    setsockopt(sockfd, SOL_DCCP, SO_REUSEADDR, (const char *) &true, sizeof(true));

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        error(strerror(errno));
        close(sockfd);
        return ERRNO_BIND;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        error(strerror(errno));
        close(sockfd);
        return ERRNO_LISTEN;
    }

    return sockfd;
}

void net_destroy()
{

    exit(EXIT_SUCCESS);
}

int handle(int sockfd)
{
    int infd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while (TRUE) {
        char buf[1024];

        if ((infd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len)) == -1) {
            error(strerror(errno));
            continue;
        }

        int bytes = recv(infd, buf, 1024, 0);

        printf("%s %d\n", buf, bytes);

        printf("%d\n", ntohs(client_addr.sin_port));
    }

    return 9999;
}
