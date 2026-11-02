#include "udp_saw.h"
#include <netinet/in.h>

int sawDoNothing(void) {
    return 10;
}


void saw_initHeader(udp_sawHeader_t* header)
{
    header->packetId    = 0;
    header->checksum    = 0;
    header->length      = 0;
}

void saw_writePacket(const udp_sawHeader_t* header,
    char* netBuf, size_t netBufSize, const char* dataBuf, size_t dataBufSize
) {
    (void)header;
    (void)netBuf;
    (void)netBufSize;
    (void)dataBuf;
    (void)dataBufSize;

    assert(netBuf != NULL);
    assert(netBufSize - HEADER_LEN >= dataBufSize);
}

void saw_readPacket(udp_sawHeader_t* header,
    const char* netBuf, size_t netBufSize
) {
    assert(netBuf != NULL);
    assert(netBufSize >= HEADER_LEN);

    int offset = 0;

    memcpy(&header->packetId, netBuf + offset, PACKETID_L);
    offset += PACKETID_L; // re-ordering not necessary since this is just 1 byte

    memcpy(&header->checksum, netBuf + offset, CHECKSUM_L);
    header->checksum = ntohs(header->checksum);
    offset += CHECKSUM_L;

    memcpy(&header->length, netBuf + offset, LENGTH_L);
    header->length = ntohs(header->length);
    offset += LENGTH_L;

    assert(netBufSize - HEADER_LEN >= header->length);
}

