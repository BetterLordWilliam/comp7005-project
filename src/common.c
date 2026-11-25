#include "common.h"

// change
int doNothing(void) {
    return 10;
}

int poll_s(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int r;
    do {
        r = poll(fds, nfds, timeout);
        if (r == -1 && errno == EINTR) continue;
        if (r == -1) return -1;
        break;
    } while (1);
    return r;
}

ssize_t recvfrom_s(int sockfd, void* buf, size_t size,
        int flags, struct sockaddr* src_addr, socklen_t* addrlen) {
    ssize_t r;
    do {
        r = recvfrom(sockfd, buf, size, flags, src_addr, addrlen);
        if (r == -1 && errno == EINTR) continue;
        if (r == -1) return -1;
        break;
    } while (1);
    return r;
}


ssize_t sendto_s(int sockfd, const void *buf, size_t size, int flags,
        const struct sockaddr *dest_addr, socklen_t addrlen) {
    ssize_t r;
    do {
        r = sendto(sockfd, buf, size, flags, dest_addr, addrlen);
        if (r == -1 && errno == EINTR) continue;
        if (r == -1) return -1;
        break;
    } while (1);
    return r;
}

