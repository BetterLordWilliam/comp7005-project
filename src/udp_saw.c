#include "udp_saw.h"
#include <string.h>


atomic_int saw_error = SAWENONE;


void saw_initHeader(saw_header_t* header)
{
#ifdef UDP_DEBUG
    assert(header != NULL);
#endif

    header->packetId    = 0;
    header->packetType  = (uint8_t)DATA;
    header->connId      = (uint16_t)0;
    header->checksum    = (uint16_t)0;
    header->length      = (uint16_t)0;
}

void saw_writePacket(const saw_header_t* header,
    char* netBuf, size_t netBufSize, const char* dataBuf, size_t dataBufSize
) {
#ifdef UDP_DEBUG
    assert(netBuf != NULL);
    assert(netBufSize - HEADER_LEN >= dataBufSize);
    assert(header->length == dataBufSize);
#endif
    (void)netBufSize;

    int offset = 0;
    uint16_t tConnId, tChecksum, tLength;

    memcpy((netBuf + offset), &header->packetId, PACKETID_L);
    offset += PACKETID_L;       // 1 byte, no `hton*`

    memcpy((netBuf + offset), &header->packetType, PACKETTYPE_L);
    offset += PACKETTYPE_L;     // 1 byte, no `hton*`
  
    tConnId = htons(header->connId);
    memcpy((netBuf + offset), &tConnId, CONNID_L);
    offset += CONNID_L;
 
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
    (void)netBufSize;

    int offset = 0;

    memcpy(&header->packetId, netBuf + offset, PACKETID_L);
    offset += PACKETID_L; // 1 byte
    
    memcpy(&header->packetType, netBuf + offset, PACKETTYPE_L);
    offset += PACKETTYPE_L; // 1 byte

    memcpy(&header->connId, netBuf + offset, CONNID_L);
    header->connId = ntohs(header->connId);
    offset += CONNID_L;

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
 
    state->cSeqNo   = (uint8_t)0;
    state->nSeqNo   = (uint8_t)0;
    
    state->timeout      = (time_t)0;
    state->rtime        = (time_t)0;
    state->timeoutDur   = (time_t)I_TIMEOUTDUR_MS;
    state->timeoutc     = 0;

    state->cStatus = INIT;

    state->paddr    = (struct sockaddr_in){ 0 };
    state->paddrLen = sizeof(state->paddr);
}

// [WO] does this need to be presented as an external API?
static int saw_genConnId(saw_state_t* state)
{
    uint16_t rconnId;
    if (getrandom(&rconnId, sizeof(rconnId), 0) == -1) {
        perror(""); 
        LERROR("error generating connection id.");
        return -1;
    }
    state->connId = rconnId;
    return 0;
}

static int saw_calcTimeoutDur(saw_state_t* state)
{
    state->timeoutDur = I_TIMEOUTDUR_MS; // previous RTTs are not tracked (yet)
    return 0;
}

static int saw_calcTimeout(saw_state_t* state)
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

static int saw_calcRtime(saw_state_t* state)
{
    struct timespec ts = { 0 };
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        perror("clock_gettime");
        LERROR("error getting monotonic time.");
        return -1;
    }
    time_t ntime = NTIME(ts);
    state->rtime = ntime;
    return 0;
}

static int saw_calcChecksum(const char* netBuf, size_t netBufSize) {
    // use a simple checksum
    int sum = 0;
    for (size_t i = 0; i < netBufSize; i++) {
        sum += (int)*(netBuf + i);
    }
    return sum;
}

static int saw_calcHeaderChecksum(saw_header_t header) {
    int sum = 0;
    header.checksum = 0; // exclude the checksum when calculating the checksum
    char* fbuf = (char*)&header;
    for (size_t i = 0; i < sizeof(header); i++) {
        sum += (int)*(fbuf + i);
    }
    return sum;
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
    struct pollfd pfd = {
        .fd         = fd,
        .events     = POLLIN,
        .revents    = 0
    };

    // await `I` message from the client
    LINFO("waiting for `I` message from peer");
    do {
        r = poll(&pfd, 1, -1);
        LINFO("returned %d", r);
        if (r == -1 && errno == EINTR)
            continue;
        else if (r == -1 || r == 0) {
            if (r == 0) LERROR("poll timeout is impossible");
            LERROR("bonnafide poll error while waiting for client message");
            rcode = -1;
            return rcode;
        }

        if (pfd.revents & (POLLERR)) {
            LERROR("poll error");
            rcode = -1;
            break;
        } else if (pfd.revents & ( POLLHUP | POLLIN )) {
            saw_header_t i; saw_initHeader(&i);
            do {
                r = recvfrom(fd, buffer, BUFFER_SIZE, 0,
                    (struct sockaddr*)&paddr, &paddrLen);
                if (r == -1 && errno == EINTR)
                    continue;
                if (r == -1) {
                    LERROR("recvfrom error aborting handshake");
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
                state->connId   = i.connId;
                state->cStatus  = INIT;
                break;
            } else {
                LINFO("not an `I` packet dropping");
                continue;
            }
        } else {
            LERROR("unexpected poll event aborting handshake");
            rcode = -1;
            return rcode;
        }
    } while (1);
    LINFO("`I` received from peer");
     
    // submit the `IACK` message to the client
    LINFO("submitting `IACK` reply to the peer.");
    memset(buffer, 0, BUFFER_SIZE);
    saw_header_t iack; saw_initHeader(&iack);
    iack.connId = state->connId;
    iack.packetType = IACK;
    iack.length = 0;
    saw_writePacket(&iack, buffer, BUFFER_SIZE, NULL, 0);
    do {
        r = sendto(fd, buffer, HEADER_LEN + iack.length, 0,
            (struct sockaddr*)&paddr, paddrLen);
        if (r == -1 && errno == EINTR)
            continue;
        if (r == -1) {
            LERROR("sendto error aborting handshake");
            rcode = -1;
            return rcode;
        }
        if (r < HEADER_LEN) {
            LERROR("sendto transmission dropped part of the message aborting handshake");
            rcode = -1;
            return rcode;
        }
        break;
    } while (1);
    LINFO("`IACK` submitted to the peer");

    // wait for the `IACK` from the client
    memset(buffer, 0, BUFFER_SIZE);
    LINFO("waiting for client `IACK` to confirm connection.");
    do {
        r = poll(&pfd, 1, -1);
        if (r == -1 && errno == EINTR)
            continue;
        else if (r == -1 || r == 0) {
            if (r == 0) LERROR("poll timeout is impossible");
            LERROR("bonnafide poll error while waiting for peer `IACK`message");
            rcode = -1;
            return rcode;
        }

        if (pfd.revents & (POLLERR)) {
            LERROR("poll error");
            rcode = -1;
            break;
        } else if (pfd.revents & ( POLLHUP | POLLIN )) {
            saw_header_t i; saw_initHeader(&i);
            do {
                r = recvfrom(fd, buffer, BUFFER_SIZE, 0,
                    (struct sockaddr*)&paddr, &paddrLen);
                if (r == -1 && errno == EINTR)
                    continue;
                if (r == -1) {
                    LERROR("recvfrom error aborting handshake");
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
            // reply saddr info does not match the peers saddr info
            if (i.connId != state->connId) {
                LWARN("peer connection id does not match");
                continue;
            }
            if (i.packetType == IACK) {
                state->cStatus  = CON;
                state->paddr = paddr;           // save peer address
                state->paddrLen = paddrLen;     // save peer address length
                break;
            } else {
                LINFO("not an `IACK` packet dropping");
                continue;
            }
        } else {
            LERROR("unexpected poll event aborting handshake");
            rcode = -1;
            return rcode;
        }
    } while (1);
    LINFO("handshake completed.");

    return rcode;
}

/*
int saw_accept(int fd, saw_state_t* state)
{
    LINFO("unimplemented");
    int rcode = 0, r = 0;
    return rcode;
}
*/

int saw_connect(int fd, saw_state_t* state)
{
    int rcode = 0, r = 0;

    char buffer[BUFFER_SIZE];
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0
    };
    struct sockaddr_in paddr;
    socklen_t paddrLen = sizeof(paddr);

    // generate connection ID
    saw_genConnId(state);
   
    // transmit `I` connection initiation packet
    LINFO("submitting `I` to peer.");
    memset(buffer, 0, BUFFER_SIZE);
    saw_header_t iack; saw_initHeader(&iack);
    iack.packetType = I;
    iack.connId = state->connId;
    iack.length = 0;
    saw_writePacket(&iack, buffer, BUFFER_SIZE, NULL, 0);
    do {
        r = sendto(fd, buffer, HEADER_LEN + iack.length, 0,
            (struct sockaddr*)&state->paddr, state->paddrLen);
        if (r == -1 && errno == EINTR)
            continue;
        if (r == -1) {
            LERROR("sendto error aborting handshake");
            rcode = -1;
            return rcode;
        }
        if (r < HEADER_LEN) {
            LERROR("sendto transmission dropped part of the message aborting handshake");
            rcode = -1;
            return rcode;
        }
        break;
    } while (1);
    LINFO("`I` submitted to peer");

    // listen for `IACK` packet
    LINFO("waiting for peer `IACK` to confirm connection.");
    memset(buffer, 0, BUFFER_SIZE);
    do {
        r = poll(&pfd, 1, -1);
        if (r == -1 && errno == EINTR)
            continue;
        else if (r == -1 || r == 0) {
            if (r == 0) LERROR("poll timeout is impossible");
            LERROR("bonnafide poll error while waiting for peer `IACK`message");
            rcode = -1;
            return rcode;
        }

        if (pfd.revents & (POLLERR)) {
            LERROR("poll error");
            rcode = -1;
            break;
        } else if (pfd.revents & ( POLLHUP | POLLIN )) {
            saw_header_t i; saw_initHeader(&i);
            do {
                r = recvfrom(fd, buffer, BUFFER_SIZE, 0,
                    (struct sockaddr*)&paddr, &paddrLen);
                if (r == -1 && errno == EINTR)
                    continue;
                if (r == -1) {
                    LERROR("recvfrom error aborting handshake");
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
            if (i.connId != state->connId) {
                LWARN("peer connId does not match connection connId");
                continue;
            }
            if (i.packetType == IACK) {
                state->cStatus  = CON;
                break;
            } else {
                LINFO("not an `IACK` packet dropping");
                continue;
            }
        } else {
            LERROR("unexpected poll event aborting handshake");
            rcode = -1;
            return rcode;
        }
    } while (1);
    
    // transmit `IACK` connection confirmation packet
    LINFO("submitting `IACK` to peer.");
    memset(buffer, 0, BUFFER_SIZE);
    saw_initHeader(&iack);
    iack.connId = state->connId;
    iack.packetType = IACK;
    iack.length = 0;
    saw_writePacket(&iack, buffer, BUFFER_SIZE, NULL, 0);
    do {
        r = sendto(fd, buffer, HEADER_LEN + iack.length, 0,
            (struct sockaddr*)&state->paddr, state->paddrLen);
        if (r == -1 && errno == EINTR)
            continue;
        if (r == -1) {
            LERROR("sendto error aborting handshake");
            rcode = -1;
            return rcode;
        }
        if (r < HEADER_LEN) {
            LERROR("sendto transmission dropped part of the message aborting handshake");
            rcode = -1;
            return rcode;
        }
        break;
    } while (1);
    LINFO("`IACK` submitted to peer.");
 
    return rcode;
}

int saw_send(int fd, saw_state_t* state,
    const char* netBuf, size_t netBufSize)
{
#ifdef UDP_DEBUG
    assert(state != NULL);
    assert(netBuf != NULL);
#endif

    if (state->cStatus != CON)
        return (int)SAWENCONN;
    int rcode = 0, r = 0, cpd = 0, ns = 0;

    // struct timespec rtime = { 0 };
    struct pollfd pfd = {
        .fd         = fd,
        .events     = POLLIN,
        .revents    = 0
    };
    char wbuffer[BUFFER_SIZE];
    char rbuffer[BUFFER_SIZE];

    
    memset(wbuffer, 0, BUFFER_SIZE);
    saw_header_t reqh = { 0 }; saw_initHeader(&reqh);
    reqh.packetId   = state->cSeqNo;
    reqh.connId     = state->connId;
    reqh.length     = netBufSize;
    reqh.checksum   = (
        saw_calcHeaderChecksum(reqh) + saw_calcChecksum(netBuf, netBufSize)
    );
    saw_writePacket(&reqh, wbuffer, BUFFER_SIZE, netBuf, netBufSize);

    state->timeoutc = 0;
    int ackr = 0;

    // segment transmission loop
    // for both saw & sliding window this entire process needs to be looped
    do {
        if (state->timeoutc >= M_RETRANS) {
            LERROR("maximum retransmissions reached for packet %d", state->cSeqNo);
            return -1;
        }
    
        do {
            r = cpd = sendto(fd, wbuffer, HEADER_LEN + reqh.length, 0,
                (struct sockaddr*)&state->paddr, state->paddrLen);
            if (r == -1 && errno == EINTR) continue;
            if (r == -1) {
                LERROR("sendto syscall error transmitting DATA segment %d", state->cSeqNo);
                return -1;
            }
            break;
        } while (1);
    
        // ACK wait loop
        // read message & determine if its the ACK we are expecting
        // assert that the ACK recevied is ( ( ACK n + 1 ) % 2 )
        do {
            r = poll(&pfd, 1, state->timeoutDur);
            if (r == -1) {
                if (errno == EINTR) continue;
                LERROR("poll syscall error waiting for ACK %d",
                    SAW_NXID(state->cSeqNo));
                return -1;
            }

            if (r == 0) {
                LERROR("connId %d timeout %d ACK %d not received",
                    state->connId, state->timeoutc, SAW_NXID(state->cSeqNo));
                state->timeoutc++;
                break;
            }
            if (!(pfd.revents & (POLLIN))) {
                LERROR("poll returned an unexpected event");
                return -1;
            }

            memset(rbuffer, 0, BUFFER_SIZE);
            do {
                r = recvfrom(fd, rbuffer, BUFFER_SIZE, 0, NULL, NULL);
                if (r == -1) {
                    if (errno == EINTR) continue;
                    LERROR("recvfrom syscall error attempting to read possible ACK %d",
                        SAW_NXID(state->cSeqNo));
                    return -1;
                }
                break;
            } while (1);

            saw_header_t reph = { 0 }; saw_initHeader(&reph);
            saw_readPacket(&reph, rbuffer, BUFFER_SIZE);
            if (reph.connId != state->connId) {
                LWARN("received packet connId does not match specified state connId while waiting for send ack %d",
                    SAW_NXID(state->cSeqNo));
                continue;
            }
            if (reph.packetId != SAW_NXID(state->cSeqNo)) {
                LWARN("prev ACK recieved %d, while waiting for ACK %d",
                    reph.packetId, SAW_NXID(state->cSeqNo));
                continue;
                // break;
            }

            // ACK received increment the counter
            // if there were multiple segments we loop again so some outer loop
            // would be involved
            // accumulate the number of sent bytes (excluding saw proto header bytes)
            ns += (cpd - HEADER_LEN);
            state->cSeqNo = SAW_NXID(state->cSeqNo);
            ackr = 1;
            break;

        } while (1); // ACK wait loop end
        
        if (ackr == 1 ) break;

    } while (1); // segment transmission loop end


    return ns;
}

int saw_recv(int fd, saw_state_t* state,
    char* netBuf, size_t netBufSize)
{
#ifdef UDP_DEBUG
    assert(state != NULL);
    assert(netBuf != NULL);
#endif

    if (state->cStatus != CON)
        return (int)SAWENCONN;
    int rcode = 0, r = 0, nr = 0;

    char rbuffer[BUFFER_SIZE];
    char wbuffer[BUFFER_SIZE];

    // read from socket store the number of bytes read
    do {
        memset(rbuffer, 0, BUFFER_SIZE);
        r = recvfrom(fd, rbuffer, BUFFER_SIZE, 0, NULL, NULL);
        if (r == -1) {
            if (errno == EINTR) continue;
            LERROR("recvfrom call failed for incoming DATA");
            return -1;
        }
        break;
    } while (1);

    saw_header_t rh = { 0 }; saw_initHeader(&rh);
    saw_initHeader(&rh);
    saw_readPacket(&rh, rbuffer, BUFFER_SIZE);

    // recompute checksum
    int rsum = (
        saw_calcHeaderChecksum(rh) + saw_calcChecksum(rbuffer + HEADER_LEN, rh.length)
    );
    if (rsum != rh.checksum) {
        LWARN("checksums do not match corrupted packet should be retransmitted.");
        return 0;
    }

    if (rh.packetType != DATA) {
        LWARN("packet should be DATA packet.");
        return 0;
    }
    if (rh.connId != state->connId) {
        LWARN("packet connId, %d, does not match specified state connId %d",
            rh.connId, state->connId);
        return -1;
    }
    if (rh.length > netBufSize) {
        LERROR("buffer too small for incoming data payload.");
        return -1;
    }
    if (rh.packetId != state->nSeqNo) {
        LWARN("recieved unexpected packetId %d, expected %d",
            rh.packetId, state->nSeqNo);

        saw_header_t prph = { 0 }; saw_initHeader(&prph);
        prph.connId     = state->connId;
        prph.packetId   = state->nSeqNo;
        prph.packetType = ACK;
        prph.length     = 0;
        saw_writePacket(&prph, wbuffer, HEADER_LEN + prph.length, NULL, 0);
        LINFO("dup retransmit prev ACK %d", state->nSeqNo);
        
        r = sendto(fd, wbuffer, HEADER_LEN + prph.length, 0,
            (struct sockaddr*)&state->paddr, state->paddrLen);

        return 0;
    }

    // copy the data bytes for the caller
    memcpy(netBuf, rbuffer + HEADER_LEN, rh.length);
    nr = rh.length;
    
    // transmit ACK
    memset(wbuffer, 0, BUFFER_SIZE);
    saw_header_t rph = { 0 }; saw_initHeader(&rph);
    rph.packetType  = ACK;
    rph.packetId    = SAW_NXID(state->nSeqNo);
    rph.length      = 0;
    rph.connId      = state->connId;
    saw_writePacket(&rph, wbuffer, HEADER_LEN + rph.length, NULL, 0);
    LINFO("--> DATA %d\tACK %d -->", rh.packetId, rph.packetId);

    do {
        r = sendto(fd, wbuffer, HEADER_LEN + rph.length, 0,
            (struct sockaddr*)&state->paddr, state->paddrLen);
        if (r == -1) {
            if (errno == EINTR) continue;
            perror("sendto");
            LERROR("sendto call failed for outgoing ACK");
            return -1;
        }
        break;
    } while (1);
    
    state->nSeqNo = SAW_NXID(state->nSeqNo);

    return nr; // number of recevied bytes
}

