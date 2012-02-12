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
#include "error.h"
#include "file.h"
#include "ptftp.h"

#define BACKLOG 5 // max number of pending connections

int sockfd;
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
int init();
int net_init();
void destroy();
void handle();
void *handle_client(void *);


int main (int argc, char **argv)
{

    struct file_info fi;
    strcpy(fi.filename, "ptftp.h");
    strcpy(fi.mode, MODE_NETASCII);
    fi.readonly = TRUE;

    file_init(&fi);
    read_next_block(&fi);
    printf("--\n%s\n--\n%d\n", fi.last_block, fi.last_numbytes);

    read_next_block(&fi);
    printf("--\n%s\n--\n%d\n", fi.last_block, fi.last_numbytes);

    file_close(&fi);


    return 0;



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

void destroy()
{
    pthread_mutex_destroy(&clients_mutex);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void handle()
{
    int infd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while (TRUE) {
        if ((infd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len)) == -1) {
            error(strerror(errno));
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        if (num_clients >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }

        int id = clients_id;
        clients_id = (clients_id+1) % MAX_CLIENTS;
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
    int BUF_SIZE = 1024;// extend if larger data field added

    int id = (int) _id, i;
    struct client_info *ci = NULL;
    struct file_info fi;
    char buf[BUF_SIZE];
    char requesting = FALSE;

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
        static int de = 1;

        if ((bytes = recv(ci->sock, buf, BUF_SIZE, 0)) <= 0) {
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
        if (opcode == PKT_RRQ) {
            char done_filename = FALSE;
            char done_mode = FALSE;
            int fileidx = 0, modeidx = 0;

            if (requesting == TRUE) {
                //TODO handle this error
            }

            fi.readonly = TRUE;

            /* Extract filename and mode */
            for (i = sizeof(uint16_t); i < bytes; i++) {
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

            requesting = TRUE;

            printf("  %s\n  %s\n", fi.filename, fi.mode);



        } else if (opcode == PKT_WRQ) {
            // Not implemented
        } else if (opcode == PKT_DATA) {
            // Not implemented
        } else if (opcode == PKT_ACK) {
            /* Discard ACK's when not in request mode */
            if (requesting == FALSE)
                continue;



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
