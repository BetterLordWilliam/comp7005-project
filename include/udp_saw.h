#ifndef UDP_SAW
#define UDP_SAW

#include <stdint.h>

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

typedef struct udp_saw_state {
    int ack;                    // current ack no
} udp_sawState_t;

#define HEADER_LEN  (40)

#define PACKETID_M  (0xFF00000000)
#define PACKETID_IM (0x00FFFFFFFF)
#define CHECKSUM_M  (0x00FFFF0000)
#define CHECKSUM_IM (0xFF0000FFFF)
#define LENGTH_M    (0x000000FFFF)
#define LENGTH_IM   (0xFFFFFF0000)

typedef struct udp_saw_header {
    uint8_t     packetId;
    uint16_t    checksum;
    uint16_t    length;         // length of the packet contents
} udp_sawHeader_t;


int sawDoNothing();

#endif

