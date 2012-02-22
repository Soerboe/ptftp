#ifndef FILE_H
#define FILE_H

#include <stdio.h>

#define BLOCKSIZE 512

struct file_info {
    char filename[512];
    char mode[16];
    FILE *fd;
    char readonly;
    size_t cur_block; // first block is 1
    char last_block[BLOCKSIZE];
    size_t last_numbytes; // last number of read/written blocks
};

int file_init (struct file_info *);
int file_close (struct file_info *);
int read_next_block (struct file_info *);
int write_block (struct file_info *, char *, int);

#endif
