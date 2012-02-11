#include <stdio.h>
#include "error.h"

char *errors[] = {                                      // errno
    "No DCCP support",                                  // 0
    "No server given",                                  // 1
    "Could not bind to socket",                          // 2
    "Wrong argument count"
};


void error (const char *msg) {
    fprintf(stderr, "%s: %s\n", "error: ", msg);
}

void error_l (const char *msg) {
    fprintf(stderr, "%s%s\n", "error: ", msg);
}

void error_num (int num) {
    if (num < 0 || num > sizeof(errors) / sizeof(char *))
        return;

    fprintf(stderr, "%s: %s\n", "error: ", errors[num]);
}
