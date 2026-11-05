#ifndef UDP_SAW
#define UDP_SAW


#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "common.h"

/*
Notes
- `c17` -> `gnu17` because `time.h` does not define `clock_gettime` syscall
as this is part of POSIX, which is not part of the strict C ISO
- 
*/


#define HEADER_LEN  (6) // why is this 40?

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

#define PLEN(len) (HEADER_LEN + (len))

/* THIS MUST BE USED W/ HEADER STRUCT */
#define PLENU(head) (HEADER_LEN + (head.length))

/**
Defines the packet types for stop-and-wait.
*/
typedef enum saw_packetType {
    DATA, ACK, I, IACK, T, TACK
} saw_packetType_t;

/**
Store all info such that this packet is as a window into the buffer
    for other application processing to use 
    though I don't think its required that either server nor client process
    this in any significant way other than passing it back and forth
*/
typedef struct saw_header {
    uint8_t     packetId;
    uint8_t     packetType;
    uint16_t    checksum;
    uint16_t    length;         // length of the packet contents
} saw_header_t;

void saw_initHeader(saw_header_t* header);

/**
Writes from the given `dataBuf` to given `netBuf` network buffer according to
the given `udp_sawHeader_t` `header`.
*/
void saw_writePacket(const saw_header_t* header,
    char* netBuf, size_t netBufSize, const char* dataBuf, size_t dataBufSize);

/**
Reads from the given `netBuf` network buffer & initialzies the given
`udp_sawHeader_t` `header`.
*/
void saw_readPacket(saw_header_t* header,
    const char* netBuf, size_t netBufSize);


#define M_RETRANS       (3)
#define I_TIMEOUTDUR_MS (1000)

typedef enum saw_cstatus {
    INIT, CON, DISCON,
} saw_cstatus_t;

/**
SAW (stop-and-wait) "connection" state.
    Storing connection state in a struct like this should help in the future
    Sliding window implementation when we will have multiple of these 
    'connections' simultaneously

the initial timeout duration is 1S or 1000MS (units of `timeoutDur` are MS)
*/
typedef struct saw_state {
    uint8_t cSeqNo; // current ack no
    uint8_t nSeqNo; // next ack no
    int rtrans;     // connection retransmission counter
    saw_cstatus_t cStatus;  // connection status
    time_t timeoutDur; // timeout duration (should be calculated from RTT)
    time_t timeout;    // expiration time
} saw_state_t;

void saw_initState(saw_state_t* state);

void saw_calcTimeoutDur(saw_state_t* state);
void saw_calcTimeout(saw_state_t* state);

/**
Go through the connection initiation lifecycle on a socket FD w/ handshake
define in SAW proto
*/
int saw_connect(int fd, saw_state_t* state);



#endif

