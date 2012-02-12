#ifndef ERROR_H
#define ERROR_H

#define ERRNO_GETADDR       -1
#define ERRNO_SOCK          -2
#define ERRNO_BIND          -3
#define ERRNO_LISTEN        -4
#define ERRNO_CON           -5
#define ERRNO_IP            -6

void error (const char *);
void error_num (int);

#endif
