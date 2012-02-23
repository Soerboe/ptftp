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

int get(int, char *, char *);
int net_init(char *);
int serialize_request(struct pkt_request *, char *);
int serialize_ack(struct pkt_ack *, char *);


int main (int argc, char **argv)
{

//     struct file_info in;
//     in.readonly = TRUE;
//     strcpy(in.filename, argv[3]);
//     strcpy(in.mode, MODE_OCTET);
// 
//     struct file_info out;
//     out.readonly = FALSE;
//     strcpy(out.filename, argv[4]);
//     strcpy(out.mode, MODE_OCTET);
// 
//     file_init(&in);
//     file_init(&out);
// 
//     int len = read_next_block(&in);
//     int len2 = write_block(&out, in.last_block, len);
// 
//     if (len != len2)
//         printf("ERRORORORRO\n");
// 
//     file_close(&in);
//     file_close(&out);
//     return 0;






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
    uint8_t successful = FALSE;

    /* Create local file */
    strcpy(fi.filename, local_file);
    strcpy(fi.mode, MODE_OCTET); //TODO
    fi.readonly = FALSE;

    if (file_init(&fi) == ERRNO_FOPEN) {
        error_num(6);
        return -1;
    }

    struct pkt_request req;
    req.opcode = PKT_RRQ;
    strcpy(req.filename, remote_file);
    strcpy(req.mode, MODE_OCTET); // TODO
    len = serialize_request(&req, request);

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

        uint16_t opcode = ntohs(*(uint16_t *) buf);

        /* Handle incoming packet */
        if (opcode == PKT_DATA) {
            uint32_t block = ntohs(*(uint32_t *) (buf + B2));
            char *data = buf + B2 + B4;

            printf("block: %d\n", block);

            /* Ignore data with wrong block number */
            if (block == fi.cur_block + 1) {
                write_block(&fi, data, bytes - B2 - B4);
            } else {
                continue;
            }

            printf("last_bytes: %d\n", fi.last_numbytes);

            /* Send ACK */
            struct pkt_ack ack;
            ack.opcode = PKT_ACK;
            ack.block = block;
            char buf[16];
            int len = serialize_ack(&ack, buf);

            send(sockfd, buf, len, 0);

            if (fi.last_numbytes < BLOCKSIZE) {
                // transfer finished
                successful = TRUE;
                break; // TODO not break, wait for timeout
            }

        } else if (opcode == PKT_ACK) {

        } else if (opcode == PKT_ERROR) {

        } else {

        }
    }

    file_close(&fi);
    if (!successful)
        remove(fi.filename);
    return 0;
}

/* Assumes buf is large enough to hold the whole request 
 * Returns length of serialized string */
int serialize_request(struct pkt_request *req, char *buf) {
    int len;

    uint16_t *opcode = (uint16_t *) buf;
    *opcode = htons(req->opcode);

    if ((len = sprintf(buf + sizeof(req->opcode), "%s%c%s", req->filename, '\0', req->mode)) < 0) {
        return len;
    }

    return len + sizeof(req->opcode) + 1;
}

/* Assumes buf is large enough to hold the whole request 
 * Returns length of serialized string */
int serialize_ack(struct pkt_ack *ack, char *buf) {
    uint16_t *opcode = (uint16_t *) buf;
    *opcode = htons(ack->opcode);
    uint32_t *block = (uint32_t *) (buf + B2);
    *block = htons(ack->block);

    return B2 + B4;
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
