#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include "error.h"
#include "ptftp.h"

int net_init(char *);

int main (int argc, char **argv)
{
    if (argc != 2) {
        error_num(3);
        return EXIT_FAILURE;
    }

    int sockfd = net_init(argv[1]);

    char str[20] = "Yalla";
    
    int i;
    for (i = 0; i < 5; i++) {
    int stat = send(sockfd, str, 20, 0);

    if (stat < 0)
        perror("");
    }

    close(sockfd);

    return EXIT_SUCCESS;
}

int net_init(char *ip)
{
    int sockfd, true = 1;
    struct sockaddr_in server_addr;

    if ((sockfd = socket(PF_INET, SOCK_DCCP, IPPROTO_DCCP)) == -1) {
        error(strerror(errno));
        return ERRNO_SOCK;
    }

    setsockopt(sockfd, SOL_DCCP, SO_REUSEADDR, (const char *) &true, sizeof(true));

    /* Initialize server info */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr.s_addr) < 1)
        return ERRNO_IP;

    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        error(strerror(errno));
        close(sockfd);
        return ERRNO_CON;
    }

    return sockfd;
}
