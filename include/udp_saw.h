#ifndef UDP_SAW
#define UDP_SAW


/**
Stop and wait requirements:
    1. introduce a format for messages
    2. define DATA / ACK message types
    3. sender emits DATA(n), receiver gets this & transmits ACK(n)


edge cases
    1. packet is lost on transmission from the sender
    2. ack from the receiver

*/

typedef struct udp_saw_state {
    int ack;
} udp_sawState_t;


#endif

