#include "udp_saw.h"


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

    memcpy((netBuf + offset), dataBuf, dataBufSize);
    offset += dataBufSize;      // same byte ordering this is client data
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

