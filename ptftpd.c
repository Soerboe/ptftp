#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include "error.h"
#include "file.h"
#include "ptftp.h"
#include "common.h"

#define BACKLOG 5 // max number of pending connections

int pubsock;
char terminate = 0;

struct client_info {
    pthread_t thread;
    int sock;
    int id;
    struct sockaddr_in addr;
    struct client_info *next;
};

struct client_info *clients;
int num_clients = 0, clients_id = 0;

pthread_mutex_t clients_mutex;

/* Function definitions */
int serialize_data(struct pkt_data *, char *);
void destroy();
int init();
void handle();
void *handle_client(void *);
int net_init();
int send_next_data_block (struct file_info *, char *, int *, int);
int send_data(int, char *, int);


int main (int argc, char **argv)
{
// 
//     struct file_info fi;
//     strcpy(fi.filename, "ptftp.h");
//     strcpy(fi.mode, MODE_NETASCII);
//     fi.readonly = TRUE;
// 
//     file_init(&fi);
//     read_next_block(&fi);
//     printf("--\n%s\n--\n%d\n", fi.last_block, fi.last_numbytes);
// 
//     read_next_block(&fi);
//     printf("--\n%s\n--\n%d\n", fi.last_block, fi.last_numbytes);
// 
//     file_close(&fi);
// 
// 
//     return 0;



    signal(SIGINT, destroy);
    
    if (init() < 0)
        return EXIT_FAILURE;



    handle();

    return EXIT_SUCCESS;
}

int init() {
    int ret;
    
    if ((ret = net_init()) < 0)
        return ret;

    pthread_mutex_init(&clients_mutex, NULL);
    clients = NULL;

    return 0;
}

/* Initialize socket */
int net_init()
{
    int true = 1;
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
//         error(gai_strerror(res));
//         return ERRNO_GETADDR;
//     }
// 
//     /* Select a socket */
//     for (ptr = serverinfo; ptr != NULL; ptr = ptr->ai_next) {
//         if ((pubsock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1) {
//             perror("socket() : remove this error output");
//             continue;
//         }
// 
//         setsockopt(oub_sockfd, SOL_DCCP, SO_REUSEADDR, &true, sizeof(int));
// 
//         if (bind(pubsock, ptr->ai_addr, ptr->ai_addrlen) == -1) {
//             close(pubsock);
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

    if ((pubsock = socket(PF_INET, SOCK_DCCP, IPPROTO_DCCP)) == -1) {
        error(strerror(errno));
        return ERRNO_SOCK;
    }

    setsockopt(pubsock, SOL_DCCP, SO_REUSEADDR, (const char *) &true, sizeof(true));

    if (bind(pubsock, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        error(strerror(errno));
        close(pubsock);
        return ERRNO_BIND;
    }

    if (listen(pubsock, BACKLOG) == -1) {
        error(strerror(errno));
        close(pubsock);
        return ERRNO_LISTEN;
    }

    return pubsock;
}

void destroy()
{
    // TODO wait for all clients

    pthread_mutex_destroy(&clients_mutex);
    close(pubsock);
    exit(EXIT_SUCCESS);
}

void handle()
{
    int infd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while (TRUE) {
        if ((infd = accept(pubsock, (struct sockaddr *) &client_addr, &client_addr_len)) == -1) {
            error(strerror(errno));
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        if (num_clients >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            continue; // discard new connection when too many connected clients
        }

        int id = clients_id++;
        pthread_mutex_unlock(&clients_mutex);

        struct client_info *ci = calloc(1, sizeof(struct client_info));
        ci->id = id;
        ci->sock = infd;
        memcpy(&(ci->addr), &client_addr, sizeof(client_addr));

        if (pthread_create(&(ci->thread), NULL, handle_client, (void *) id)) {
            error(strerror(errno));
            close(infd);
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        num_clients++;
        ci->next = clients;
        clients = ci;
        pthread_mutex_unlock(&clients_mutex);

        pthread_detach(ci->thread);
    }
}

void *handle_client(void *_id)
{
    int id = (int) _id,
        i,
        pkt_num = 1,
        sel_stat,
        send_buf_len;
    struct client_info *ci = NULL;
    struct file_info fi;
    char buf[BUF_SIZE],
         send_buf[BUF_SIZE],
         requesting = FALSE;
    fd_set fileset;
    struct timeval timeout;

    /* Get client info */
    pthread_mutex_lock(&clients_mutex);

    for (ci = clients; ci != NULL; ci = ci->next) {
        if (id == ci->id) break;
    }

    pthread_mutex_unlock(&clients_mutex);
    assert(ci != NULL);

    printf("Thread %d\n", id);

    while (TRUE) {
        int bytes; 

        /* Set up fileset for select() */
        FD_ZERO(&fileset);
        FD_SET(ci->sock, &fileset);
        timeout.tv_sec = TIMEOUT / 1000;
        timeout.tv_usec = (TIMEOUT % 1000) * 1000;

        /* Wait for connection */
        if ((sel_stat = select(ci->sock + 1, &fileset, NULL, NULL, &timeout)) <= 0) {
            if (sel_stat == 0) { // timeout
                if (requesting == TRUE) {
                    if (send_data(ci->sock, send_buf, send_buf_len) == ERRNO_CON) {
                        error_num(4);
                        break;
                    }

                    continue;
                } else
                    break;

            } else { // error
                perror("unexpected error");
                break;
            }
        }

        if ((bytes = recv(ci->sock, buf, BUF_SIZE, 0)) <= 0) {
            /* Client shut down */
            if (bytes == 0) {
                error_num(4);
                break;
            } else  {
                //TODO error
            }
        }

        printf("PKT %d\n", pkt_num++);

        uint16_t opcode = ntohs(*((uint16_t *) buf));

        printf("opcode: %d\n", opcode);

        /* Handle incoming packet */
        if (opcode == PKT_RRQ) {
            char done_filename = FALSE;
            char done_mode = FALSE;
            int fileidx = 0, modeidx = 0;

            if (requesting == TRUE) {
                continue; //discard RRQ when already in transfer mode
            }

            fi.readonly = TRUE;

            /* Extract filename and mode */
            for (i = B2; i < bytes; i++) {
                if (!done_filename) {
                    fi.filename[fileidx++] = buf[i];
                    if (buf[i] == 0)
                        done_filename = TRUE;
                } else {
                    fi.mode[modeidx++] = buf[i];
                    if (buf[i] == 0) {
                        done_mode = TRUE;
                        if (i != bytes-1) {
                            error("hmm\n"); //////////REMOVE
                        }
                    }
                }
            }

            if (!done_filename || !done_mode) {
                // TODO handle error
                error("feilfeil\n");
                break;
            }

            printf("REQUEST:\n  %s\n  %s\n", fi.filename, fi.mode);

            requesting = TRUE;

            if (file_init(&fi) != 0) {
                error("handle this error\n"); //TODO
            }

            send_next_data_block(&fi, send_buf, &send_buf_len, ci->sock);

        } else if (opcode == PKT_WRQ) {
            // Not implemented
        } else if (opcode == PKT_DATA) {
            // Not implemented
        } else if (opcode == PKT_ACK) {
            /* Discard ACK's when not in request mode */
            if (requesting == FALSE)
                continue;

            uint32_t block = ntohs(*((uint32_t *) (buf + B2)));

            /* Ignore wrong ACK's  */
            if (block == fi.cur_block) {
                if (fi.last_numbytes < BLOCKSIZE)
                    break; // end connection when whole file is read

                send_next_data_block(&fi, send_buf, &send_buf_len, ci->sock);
            }

        } else if (opcode == PKT_ERROR) {

        } else {
            //TODO handle this case
        }
    }

    /* Remove client from queue */
    pthread_mutex_lock(&clients_mutex);
    struct client_info *c = clients;

    if (clients->id == ci->id) {
        clients = clients->next;
    } else {
        for (; c->next != NULL; c = c->next) {
            if (c->next->id == ci->id)
                c->next = ci->next;
        }
    }

    num_clients--;
    pthread_mutex_unlock(&clients_mutex);

    close(ci->sock);
    file_close(&fi);
    free(ci);

    printf("Thread %d exited\n", id);
    pthread_exit(NULL);
}


/* Assumes buf is large enough to hold the whole data packet
 * Returns length of serialized string */
int send_next_data_block (struct file_info *fi, char *buf, int *buf_len, int sockfd) {
    int i, bytes;

    read_next_block(fi);

    /* Serialize data packet */
    uint16_t *opcode = (uint16_t *) buf;
    *opcode = htons(PKT_DATA);
    uint32_t *block = (uint32_t *) (buf + B2);
    *block = htons(fi->cur_block);
    char *data = buf + B2 + B4;

    for (i = 0; i < fi->last_numbytes; i++) {
        *(data++) = fi->last_block[i];
    }

    *buf_len = fi->last_numbytes + B2 + B4;

    if ((bytes = dccp_send(sockfd, buf, *buf_len)) == -1) {
        error("handle it 1\n"); //TODO
    }

    return *buf_len;
}

int send_data(int sockfd, char *buf, int len) {
    int bytes;

    if ((bytes = dccp_send(sockfd, buf, len)) == -1) {
        error("handle it 2\n"); //TODO
        return ERRNO_CON;
    }

    return 0;
}

