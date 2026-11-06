#include "udp_saw.h"
#include <string.h>


void saw_initHeader(saw_header_t* header)
{
#ifdef UDP_DEBUG
    assert(header != NULL);
#endif

    header->packetId    = 0;
    header->packetType  = (uint8_t)DATA;
    header->checksum    = 0;
    header->length      = 0;
}

void saw_writePacket(const saw_header_t* header,
    char* netBuf, size_t netBufSize, const char* dataBuf, size_t dataBufSize
) {
#ifdef UDP_DEBUG
    assert(netBuf != NULL);
    assert(netBufSize - HEADER_LEN >= dataBufSize);
    assert(header->length == dataBufSize);
#endif

    int offset = 0;
    uint16_t tChecksum, tLength;

    memcpy((netBuf + offset), &header->packetId, PACKETID_L);
    offset += PACKETID_L;       // 1 byte, no `hton*`

    memcpy((netBuf + offset), &header->packetType, PACKETTYPE_L);
    offset += PACKETTYPE_L;     // 1 byte, no `hton*`
   
    tChecksum = htons(header->checksum);
    memcpy((netBuf + offset), &tChecksum, CHECKSUM_L);
    offset += CHECKSUM_L;

    tLength = htons(header->length);
    memcpy((netBuf + offset), &tLength, LENGTH_L);
    offset += LENGTH_L;

    if (dataBuf != NULL && dataBufSize > 0) {
        memcpy((netBuf + offset), dataBuf, dataBufSize);
        offset += dataBufSize;      // same byte ordering this is client data
    }
}

void saw_readPacket(saw_header_t* header,
    const char* netBuf, size_t netBufSize
) {
#ifdef UDP_DEBUG
    assert(netBuf != NULL);
    assert(netBufSize >= HEADER_LEN);
#endif

    int offset = 0;

    memcpy(&header->packetId, netBuf + offset, PACKETID_L);
    offset += PACKETID_L; // 1 byte
    
    memcpy(&header->packetType, netBuf + offset, PACKETTYPE_L);
    offset += PACKETTYPE_L; // 1 byte

    memcpy(&header->checksum, netBuf + offset, CHECKSUM_L);
    header->checksum = ntohs(header->checksum);
    offset += CHECKSUM_L;

    memcpy(&header->length, netBuf + offset, LENGTH_L);
    header->length = ntohs(header->length);
    offset += LENGTH_L;

#ifdef UDP_DEBUG
    assert(netBufSize - HEADER_LEN >= header->length);
#endif
}


void saw_initState(saw_state_t* state)
{
#ifdef UDP_DEBUG
    assert(state != NULL);
#endif   
 
    state->cSeqNo = (uint8_t)0;
    state->nSeqNo = (uint8_t)0;
    state->rtrans = 0;
    
    state->timeout = 0;
    state->timeoutDur = I_TIMEOUTDUR_MS;

    state->cStatus = INIT;

    state->paddr    = (struct sockaddr_in){ 0 };
    state->paddrLen = sizeof(state->paddr);
}

int saw_calcTimeoutDur(saw_state_t* state)
{
    state->timeoutDur = I_TIMEOUTDUR_MS; // previous RTTs are not tracked (yet)
    return 0;
}

int saw_calcTimeout(saw_state_t* state)
{
    struct timespec ts = { 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        LERROR("error getting monotonic time.");
        return -1;
    }
    // [WO] normalized to milliseconds
    time_t ntime    = NTIME(ts);
    state->timeout  = ntime + state->timeoutDur;
    return 0;
}

int saw_listen(int fd, saw_state_t* state)
{
    // Simplified implementation for now with 1 client
    // assumption: the `state` parameter has already been initialized
    // blocks sets `state` to contain `paddr` & `paddrLen` of the connected
    // peer (breaks for same machine, multiple connections, ppid + paddr?)


    int rcode = 0, r = 0;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in paddr;
    socklen_t paddrLen = sizeof(paddr);
    struct pollfd pfd = { 0 };
    pfd.fd      = fd;    
    pfd.events  = POLLIN;

    // await `I` message from the client
    do {
        r = poll(&pfd, 1, -1);
        if (r == -1 && errno == EINTR)
            continue;
        else if (r == -1 || r == 0) {
            if (r == 0) LERROR("poll timeout is impossible");
            LERROR("bonnafide poll error while waiting for client message");
            rcode = -1;
            break;
        }

        if (pfd.revents & (POLLERR)) {
            LERROR("poll error");
            rcode = -1;
            break;
        }
        if (pfd.revents & ( POLLHUP | POLLIN )) {
            saw_header_t i; saw_initHeader(&i);
            do {
                r = recvfrom(fd, buffer, BUFFER_SIZE, 0,
                    (struct sockaddr*)&paddr, &paddrLen);
                if (r == -1 && errno == EINTR)
                    continue;
                if (r == -1) {
                    LERROR("recvfrom error during handshake aborting");
                    rcode = -1; 
                    return rcode;
                }
                if (r < HEADER_LEN) {
                    LERROR("invalid saw packet too small");
                    rcode = -1;
                    return rcode;
                }
                break;
            } while (1);
           
            saw_readPacket(&i, buffer, BUFFER_SIZE);
            if (i.packetType == I) {
                state->paddr    = paddr;
                state->paddrLen = paddrLen;
                state->cStatus  = INIT;
                break;
            } else {
                LINFO("not an `I` packet dropping");
                continue;
            }
        }
    } while (1);
    LINFO("`I` received");
     
    // submit the `IACK` message to the client
    LINFO("submitting `IACK` reply to the client.");
    memset(buffer, 0, BUFFER_SIZE);
    saw_header_t iack; saw_initHeader(&iack);
    iack.packetType = IACK;
    iack.length = 0;
    saw_writePacket(&iack, buffer, BUFFER_SIZE, NULL, 0);
    do {
        r = sendto(fd, buffer, BUFFER_SIZE, 0,
            (struct sockaddr*)&state->paddr, state->paddrLen);
        if (r == -1 && errno == EINTR)
            continue;
        if (r == -1) {
            LERROR("sendto error during handshake, aborting");
            rcode = -1;
            return rcode;
        }
        if (r < HEADER_LEN) {
            LERROR("sendto transmission dropped part of the message, aborting");
            rcode = -1;
            return rcode;
        }
        break;
    } while (1);
    LINFO("`IACK` submitted to the client");

    // wait for the `IACK` from the client
    memset(buffer, 0, BUFFER_SIZE);
    LINFO("waiting for client `IACK` to confirm connection.");
    do {
        r = poll(&pfd, 1, -1);

        if (r == -1 && errno == EINTR)
            continue;
        else if (r == -1 || r == 0) {
            if (r == 0) LERROR("poll timeout is impossible");
            LERROR("bonnafide poll error while waiting for client message");
            rcode = -1;
            break;
        }

        if (pfd.revents & (POLLERR)) {
            LERROR("poll error");
            rcode = -1;
            break;
        }
        if (pfd.revents & ( POLLHUP | POLLIN )) {
            saw_header_t i; saw_initHeader(&i);
            do {
                r = recvfrom(fd, buffer, BUFFER_SIZE, 0,
                    (struct sockaddr*)&paddr, &paddrLen);
                if (r == -1 && errno == EINTR)
                    continue;
                if (r == -1) {
                    LERROR("recvfrom error during handshake aborting");
                    rcode = -1; 
                    return rcode;
                }
                if (r < HEADER_LEN) {
                    LERROR("invalid saw packet too small");
                    rcode = -1;
                    return rcode;
                }
                // reply saddr info does not match the peers saddr info
                if (paddr.sin_addr.s_addr != state->paddr.sin_addr.s_addr 
                    || paddr.sin_port != state->paddr.sin_port
                    || paddrLen != state->paddrLen) {
                    LWARN("peer saddr does not match connection saddr");
                    continue;
                }
                break;
            } while (1);
           
            saw_readPacket(&i, buffer, BUFFER_SIZE);
            if (i.packetType == IACK) {
                state->cStatus  = CON;
                break;
            } else {
                LINFO("not an `IACK` packet dropping");
                continue;
            }
        }
    } while (1);
    LINFO("handshake completed.");

    return rcode;
}

int saw_accept(int fd, saw_state_t* state)
{
    LINFO("unimplemented");
    int rcode = 0, r = 0;
    return rcode;
}

