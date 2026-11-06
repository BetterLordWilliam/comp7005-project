#ifndef UDP_SAW
#define UDP_SAW


#include <stdatomic.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/random.h>
#include <string.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <poll.h>

#ifdef UDP_DEBUG
#include <assert.h>
#endif

#include "common.h"

/*
Notes
- `c17` -> `gnu17` because `time.h` does not define `clock_gettime` syscall
as this is part of POSIX, which is not part of the strict C ISO 
- `atomic_int_fast32_t`
    - 

Limitations
- supports only the IPv4 protocol
- supports only the `SOCK_DGRAM` protocol type / mode
- use `saw_socket` & `saw_bind` to get a valid socket always
*/


typedef enum saw_error {
    SAWENONE    = 0,
    SAWENCONN   = -1
} saw_error_t;

extern atomic_int saw_error;


#define BUFFER_SIZE (4096)

#define HEADER_LEN  (8) // why is this 40?

#define PACKETID_M  (0xFF0000000000)
#define PACKETID_IM (0x00FFFFFFFFFF)
#define PACKETID_L  (1)         // byte length of this header field

#define PACKETTYPE_M    (0x00FF00000000)
#define PACKETTYPE_IM   (0xFF00FFFFFFFF)
#define PACKETTYPE_L    (1)

#define CONNID_M    (0x0000FFFF00000000)
#define CONNID_IM   (0xFFFF0000FFFFFFFF)
#define CONNID_L    (2)

#define CHECKSUM_M  (0x00000000FFFF0000)
#define CHECKSUM_IM (0xFFFFFFFF0000FFFF)
#define CHECKSUM_L  (2)

#define LENGTH_M    (0x000000000000FFFF)
#define LENGTH_IM   (0xFFFFFFFFFFFF0000)
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
    uint8_t     packetId;       // PACKET ID -> SEQ
    uint8_t     packetType;
    uint16_t    connId;
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

#define NTIME(timespec) ( \
    timespec.tv_sec * (1000) + ( timespec.tv_nsec / (1000000) ) \
)

#define SAW_NXID(ack) \
    ((ack + (1)) % (2))

typedef enum saw_cstatus {
    NEW, INIT, CON, DISCON
} saw_cstatus_t;

/**
SAW (stop-and-wait) "connection" state.
    Storing connection state in a struct like this should help in the future
    Sliding window implementation when we will have multiple of these 
    'connections' simultaneously

the initial timeout duration is 1S or 1000MS (units of `timeoutDur` are MS)
*/
typedef struct saw_state {
    uint8_t cSeqNo;             // current ack no
    uint8_t nSeqNo;             // next ack no
    uint16_t connId;            // connection ID
    saw_cstatus_t cStatus;      // connection status
    time_t timeoutDur;          // timeout duration (should be calculated from RTT)
    time_t timeout;             // expiration time
    time_t rtime;               // running time
    int timeoutc;               // timeout counter
    struct sockaddr_in paddr;   // [TODO] ditch this, peer ipv4 address
    socklen_t paddrLen;         // [TODO] ditch this, peer ipv4 address length
} saw_state_t;

void saw_initState(saw_state_t* state);

// int saw_genConnId(saw_state_t* state);
// int saw_calcTimeoutDur(saw_state_t* state);
// int saw_calcTimeout(saw_state_t* state);


// [TODO]
int saw_socket();
// [TODO]
int saw_bind();

/**
Listen for incoming SAW proto connection handshakes.

wait for I message types, init the state of the passed connection struct

[WO] this mechanism is valid only if there is 1 client that we are listening
for, think a size 1 buffer (saw_state struct size) that is the address passed

    for a greater number of clients, this would have to write to a buffer that is
    allocated, like the actual `listen` call, it should accept a parameter which
    specifies the number of allowed incoming connections, then write the details
    to that entry to that queue.

    but we wont worry about that for now


    check `saw_error` for errors (-1 return)
*/
int saw_listen(int fd, saw_state_t* state);

/**
Go through the connection initiation lifecycle on a socket FD w/ handshake
define in SAW proto.

    check `saw_error` for errors (-1 return)
*/
int saw_connect(int fd, saw_state_t* state);


/**
Send on a saw connection.

    send up to `netBufSize` bytes from `netBuf`

    check `saw_error` for errors (-1 return)
*/
int saw_send(int fd, saw_state_t* state,
    const char* netBuf, size_t netBufSize);

/**
Receive from a saw connection.
    
    receive up to `netBufSize` bytes from `netbuf`

    check `saw_error` for errors (-1 return)
*/
int saw_recv(int fd, saw_state_t* state,
    char* netBuf, size_t netBufSize);


#endif

