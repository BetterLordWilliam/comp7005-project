#ifndef UDP_COMMON
#define UDP_COMMON

#include <stdio.h>

#define ER      "\033[31m"
#define EG      "\033[32m"
#define EY      "\033[33m"
#define ENONE   "\033[0m"

#define LINFO(fmt, ...) \
    (fprintf)(stderr, \
        "%s:%s:%d " EG "INFO" ENONE "\t" fmt "\n", \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define LWARN(fmt, ...) \
    (fprintf)(stderr, \
        "%s:%s:%d " EY "WARN" ENONE "\t" fmt "\n", \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define LERROR(fmt, ...) \
    (fprintf)(stderr, __FILE__ ":" \
        "%s:%s:%d " ER "ERROR" ENONE "\t" fmt "\n", \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__)

int doNothing(void);

#endif

