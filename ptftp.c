#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include "error.h"
#include "file.h"
#include "ptftp.h"

int net_init(char *);
int get(int, char *, char *);

int main (int argc, char **argv)
{
    int sockfd = -1;

    if (argc != 5) {
        error_num(3);
        return EXIT_FAILURE;
    }

    /* Parse input */
    if (strcmp(argv[1], "get") == 0) {
        if ((sockfd = net_init(argv[2])) < 0) {
            //TODO handle error
        }

        get(sockfd, argv[3], argv[4]);

    } else {
        error_num(5);
        exit(EXIT_FAILURE);
    }
    




// 
//     char filename[512] = "/bin/foo";
//     char buf[1024];
// 
//     struct pkt_request *req = (struct pkt_request *) buf;
//     req->opcode = PKT_RRQ;
//     strcpy(buf+2, filename);
//     char *mode = buf+2 + strlen(filename) + 1;
//     strcpy(mode, MODE_NETASCII);
//     int len = sizeof(req->opcode) + strlen(filename) + 1 + strlen(MODE_NETASCII) + 1;
// 
//     send(sockfd, buf, len, 0);


//     char str[20] = "Yalla";
// 
//     int i;
//     for (i = 0; i < 5; i++) {
//         int stat;
//         do {
//             send(sockfd, str, 20, 0);
//         } while (stat < 0 && errno == EAGAIN);
// 
//         if (stat < 0)
//             perror("");
//     }

    if (sockfd >= 0)
        close(sockfd);

    return EXIT_SUCCESS;
}

int get(int sockfd, char *remote_file, char *local_file) {
    int len;
    char request[512];
    struct file_info fi;

    /* Create local file */
    strcpy(fi.filename, local_file);
    strcpy(fi.mode, MODE_OCTET); //TODO
    fi.readonly = FALSE;

    if (file_init(&fi) == ERRNO_FOPEN) {
        error_num(6);
        return -1;
    }

    struct pkt_request *req = (struct pkt_request *) request;
    req->opcode = PKT_RRQ;
    strcpy(request+2, remote_file);
    char *mode = request+2 + strlen(remote_file) + 1;
    strcpy(mode, MODE_OCTET);
    len = strlen(remote_file) + strlen(MODE_OCTET) + 4;

    send(sockfd, request, len, 0);

    while (TRUE) {
        int bytes;
        char buf[BUF_SIZE];
        static int de = 1;

        if ((bytes = recv(sockfd, buf, BUF_SIZE, 0)) <= 0) {
            /* Client shut down */
            if (bytes == 0) {
                //TODO client shut down
                error_num(4);
                break;
            } else  {
                //TODO error
            }
        }

        printf("PKT %d\n", de++);

        uint16_t opcode = (uint16_t) *buf;

        /* Handle incoming packet */
        if (opcode == PKT_DATA) {
            uint16_t block = (uint16_t) *(buf+2);
            char *data = buf+4;

            printf("block: %d\n", block);

            /* Ignore data with wrong block number */
            if (block == fi.cur_block + 1) {
                write_block(&fi, data, bytes - 4);
            } else {
                continue;
            }

            printf("last_bytes: %d\n", fi.last_numbytes);

            /* Send ACK */
            struct pkt_ack ack;
            ack.opcode = PKT_ACK;
            ack.block = block;
            send(sockfd, (char *) &ack, sizeof(ack), 0);

            if (fi.last_numbytes < BLOCKSIZE) {
                // transfer finished
                break;
            }

        } else if (opcode == PKT_ACK) {

        } else if (opcode == PKT_ERROR) {

        } else {

        }
    }

    file_close(&fi);
    return 0;
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
