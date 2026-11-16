#ifndef UDP_COMMON
#define UDP_COMMON

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>


#define ER      "\033[31m"
#define EG      "\033[32m"
#define EY      "\033[33m"
#define ENONE   "\033[0m"

// [WO] segment size not to be configurable
#define BUFFER_SIZE (4096)

#define LINFO(fmt, ...) \
    (fprintf)(stderr, \
        EG "INFO" ENONE " %s:%s:%d\t" fmt "\n", \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define LWARN(fmt, ...) \
    (fprintf)(stderr, \
        EY "WARN" ENONE " %s:%s:%d\t" fmt "\n", \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#define LERROR(fmt, ...) \
    (fprintf)(stderr, \
        ER "ERROR" ENONE " %s:%s:%d\t" fmt "\n", \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__)

int doNothing(void);


int poll_s(struct pollfd *fds, nfds_t nfds, int timeout);

ssize_t recvfrom_s(int sockfd, void* buf, size_t size,
                        int flags,
                        struct sockaddr* src_addr,
                        socklen_t* addrlen);

ssize_t sendto_s(int sockfd, const void *buf, size_t size, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen);

#endif

