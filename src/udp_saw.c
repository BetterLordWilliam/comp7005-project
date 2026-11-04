#include "udp_saw.h"
#include <netinet/in.h>

int sawDoNothing(void) {
    return 10;
}


void saw_initHeader(udp_sawHeader_t* header)
{
    header->packetId    = 0;
    header->packetType  = DATA;
    header->checksum    = 0;
    header->length      = 0;
}

void saw_writePacket(const udp_sawHeader_t* header,
    char* netBuf, size_t netBufSize, const char* dataBuf, size_t dataBufSize
) {
    assert(netBuf != NULL);
    assert(netBufSize - HEADER_LEN >= dataBufSize);
    assert(header->length == dataBufSize);

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

void saw_readPacket(udp_sawHeader_t* header,
    const char* netBuf, size_t netBufSize
) {
    assert(netBuf != NULL);
    assert(netBufSize >= HEADER_LEN);

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

    assert(netBufSize - HEADER_LEN >= header->length);
}

