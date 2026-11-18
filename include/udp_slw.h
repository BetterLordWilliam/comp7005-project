#ifndef UDP_SLW
#define UDP_SLW

/*
Implementation Notes & Resources
Will Otterbein

Useful Resources

- https://www.rfc-editor.org/info/rfc1982/
    - serial number arithmetic
    - comparison of ACK / SEQ numbers useful to simplify window operations
    - example 5.2 is my exact use case regarding serial arithmetic
*/

#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <math.h>

#ifdef UDP_DEBUG
#include <assert.h>
#endif

#include "common.h"


#define SLW_HEADER          ((uint16_t)7)
#define SLW_MAX_PAYLOAD     (BUFFER_SIZE - SLW_HEADER)
#define SLW_MAX_SLOTS       (256)
#define SLW_MAX_WIND_LEN    ((SLW_MAX_SLOTS/2) - 1)

typedef enum slw_error_code {
    SLW_OK = 0,
    SLW_ERROR       = -1,
    SLW_EWOULDBLOCK = -3,
    SLW_EBUFSIZE    = -2
} slw_code_t;


typedef struct slw_packet {
    uint8_t id;
    uint8_t ack;
    uint8_t flags;
    uint16_t checksum;
    uint16_t length;
    char data[SLW_MAX_PAYLOAD];
} slw_packet_t;

#define SLW_F_ACK   (0b00000001)
#define SLW_F_ACK_I (0b11111110)


typedef struct slw_window {
    slw_packet_t* packets;
    int* slot_filled;
    int slots;
    int length;
    int base;
    int next;
    int ack;
} slw_window_t;


typedef struct slw_state {
    int fd;
    int slots;
    time_t timeout;
    int timeoutdur;
    slw_window_t sndwind;
    slw_window_t rcvwind;
} slw_state_t;



int slw_init(slw_state_t* state, int fd, int window_slots, int timeoutdur);

void slw_free(slw_state_t* state);



ssize_t slw_send(slw_state_t* state, const char* buf, size_t bufsize,
    const struct sockaddr* socka, socklen_t sockalen);

ssize_t slw_recv(slw_state_t* state, char* buf, size_t bufSize);

int slw_process(slw_state_t* state,
    const struct sockaddr* socka, socklen_t sockalen);



int slw_window_empty(slw_window_t* wind);

#endif

