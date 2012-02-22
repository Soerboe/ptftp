#include <stdio.h>
#include "error.h"

char *errors[] = {                                      // error_num
    "No DCCP support",                                  // 0
    "No server given",                                  // 1
    "Could not bind to socket",                         // 2
    "Wrong argument count",                             // 3
    "Connection closed by peer",                        // 4
    "Unsupported function",                             // 5
    "Could not create local file",                      // 6
};


void error (const char *msg) {
    fprintf(stderr, "%s: %s\n", "error", msg);
}

void error_num (int num) {
    if (num < 0 || num > sizeof(errors) / sizeof(char *))
        return;

    fprintf(stderr, "%s: %s\n", "error", errors[num]);
}
