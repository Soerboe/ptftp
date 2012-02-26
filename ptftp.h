#ifndef PTFTP_H
#define PTFTP_H

#include <stdint.h>
#include "file.h"

#define MODE_NETASCII "NETASCII"
#define MODE_OCTET "OCTET"
#define MODE_MAIL "MAIL" // not used

/* DCCP stuff */
#define SOL_DCCP 269
#define PORT_NUMBER 1069 // use 1069 insted of 69 when testing

/* Connection timeout (given in ms) */
#define TIMEOUT 2000

enum {
    PKT_RRQ = 1,
    PKT_WRQ, // not used (yet)
    PKT_DATA,
    PKT_ACK,
    PKT_ERROR,

    TRUE = 1,
    FALSE = 0,

    MAX_CLIENTS = 100,
    BUF_SIZE = 1024,

    B1 = 1,
    B2 = 2,
    B4 = 4,
};

struct pkt_request {
    uint16_t opcode;
    char filename[512];
    char mode[16];
};

struct pkt_data {
    uint16_t opcode;
    uint32_t block;
    char data[BLOCKSIZE];
};

struct pkt_ack {
    uint16_t opcode;
    uint32_t block;
};

struct pkt_error {
    uint16_t opcode;
    uint16_t error_code;
    char *error_msg;
};

#endif
