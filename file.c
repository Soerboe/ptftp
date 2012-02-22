#include <string.h>
#include <errno.h>
#include "file.h"
#include "ptftp.h"
#include "error.h"

int file_init (struct file_info *fi)
{
    char fmode[4];

    if (fi == NULL) {
        return ERRNO_NULL;
    }

    if (fi->readonly == TRUE) {
        if (strcmp(fi->mode, MODE_NETASCII) == 0)
            strcpy(fmode, "r");
        else if (strcmp(fi->mode, MODE_OCTET) == 0)
            strcpy(fmode, "rb");
        else {
            // TODO ERROR condition
        }
    } else { // readonly == FALSE
        if (strcmp(fi->mode, MODE_NETASCII) == 0)
            strcpy(fmode, "w");
        else if (strcmp(fi->mode, MODE_OCTET) == 0)
            strcpy(fmode, "wb");
        else {
            //TODO error
        }
    }

    if ((fi->fd = fopen(fi->filename, fmode)) == NULL) {
        error(strerror(errno));
        return ERRNO_FOPEN;
    }

    rewind(fi->fd);
    fi->cur_block = 0;
    return 0;
}

int file_close (struct file_info *fi)
{
    return fclose(fi->fd);
}

/* 
 * Returns number of bytes read, or error condition (value < 0)
 */
int read_next_block (struct file_info *fi)
{
    fi->cur_block++;
    fi->last_numbytes = fread(fi->last_block, 1, BLOCKSIZE, fi->fd);

    // TODO check for errors
    return fi->last_numbytes;
}

int write_block (struct file_info *fi, char *buf, int length)
{
    fi->cur_block++;
    strcpy(fi->last_block, buf);

    fi->last_numbytes = fwrite(fi->last_block, 1, length, fi->fd);
    return fi->last_numbytes;
}

