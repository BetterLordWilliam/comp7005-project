#ifndef UDP_SAW
#define UDP_SAW

#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>

/**
Stop and wait requirements:
    1. introduce a format for messages
    2. define DATA / ACK message types
    3. sender emits DATA(n), receiver gets this & transmits ACK(n)

Notes:

> first implement using the alternating ID

> 


edge cases
    1. packet is lost on transmission from the sender
    2. ack from the receiver

*/

typedef enum udp_saw_packetType {
    DATA,
    ACK
} udp_sawPacketType_t;

typedef struct udp_saw_state {
    int ack;                    // current ack no
} udp_sawState_t;

#define HEADER_LEN  (40)

#define PACKETID_M  (0xFF0000000000)
#define PACKETID_IM (0x00FFFFFFFFFF)
#define PACKETID_L  (1)         // byte length of this header field

#define PACKETTYPE_M    (0x00FF00000000)
#define PACKETTYPE_IM   (0xFF00FFFFFFFF)
#define PACKETTYPE_L    (1)

#define CHECKSUM_M  (0x0000FFFF0000)
#define CHECKSUM_IM (0xFFFF0000FFFF)
#define CHECKSUM_L  (2)

#define LENGTH_M    (0x00000000FFFF)
#define LENGTH_IM   (0xFFFFFFFF0000)
#define LENGTH_L    (2)


/**
Store all info such that this packet is as a window into the buffer
    for other application processing to use 
    though I don't think its required that either server nor client process
    this in any significant way other than passing it back and forth
*/
typedef struct udp_saw_header {
    uint8_t     packetId;
    uint8_t     packetType;
    uint16_t    checksum;
    uint16_t    length;         // length of the packet contents
} udp_sawHeader_t;


int sawDoNothing(void);

void saw_initHeader(udp_sawHeader_t* header);

/**
Writes from the given `dataBuf` 
*/
void saw_writePacket(const udp_sawHeader_t* header,
    char* netBuf, size_t netBufSize, const char* dataBuf, size_t dataBufSize);

/**
Reads from the given `netBuf` network buffer & initialzies the given `udp_sawHeader`
w/ values from the expected bytes in the network buffer (as per the udp_sawHeader definition).
*/
void saw_readPacket(udp_sawHeader_t* header,
    const char* netBuf, size_t netBufSize);

#endif

