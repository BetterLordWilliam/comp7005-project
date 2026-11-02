#ifndef UDP_COMMON
#define UDP_COMMON

#include <stdio.h>

#define LINFO(fmt, ...) \
    (fprintf)(stderr, \
        "%s:%s:%d \e[32mINFO\e[0m\t" fmt "\n", __FILE__, __func__, __LINE__\
            , ##__VA_ARGS__)
#define LWARN(fmt, ...) \
    (fprintf)(stderr, __FILE__ ":" \
        "%s:%s:%d \e[33mWARN\e[0m\t" fmt "\n", __FILE__, __func__, __LINE__\
            , ##__VA_ARGS__)
#define LERROR(fmt, ...) \
    (fprintf)(stderr, __FILE__ ":" \
        "%s:%s:%d \e[31mERROR\e[0m\t" fmt "\n", __FILE__, __func__, __LINE__\
            , ##__VA_ARGS__)

#endif

