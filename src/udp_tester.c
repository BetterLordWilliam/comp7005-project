#include <bits/time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include <assert.h>

#include "common.h"
#include "udp_saw.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
 
    // LINFO("this is msg");
    // LWARN("this is a warning");
    // LERROR("this is an error");

#ifdef UDP_DEBUG
    printf("program in debug mode\n");
#endif

    char rbuf[4096] = { 0 };
    char wbuf[4096] = { 0 };


    // writing of a packet
    saw_header_t h = { 0 };
    saw_initHeader(&h);
    h.packetType = ACK;
    char* str = "this is a test";
    h.length = strlen(str);
    LINFO("%d", h.length);
    LINFO("%d", PLENU(h));
    saw_writePacket(&h, wbuf, PLENU(h), str, h.length);

    FILE* tester = fopen("data.bin", "wb");
    fwrite(wbuf, 1, PLENU(h), tester);
    fclose(tester);

    // read the packet
    saw_header_t h1 = { 0 };
    saw_initHeader(&h1);

    FILE* tester2 = fopen("data.bin", "rb");
    fread(&rbuf, 1, 4096, tester2);
    
    saw_readPacket(&h1, rbuf, 4096);

    assert(h1.packetType == h.packetType);
    assert(h1.length == h.length);

    LINFO("READ FOLLOWING PACKET INFO\n\ttype:%d;length:%d",
        h1.packetType, h1.length);
    fclose(tester2);


    // connection & time testing
    saw_state_t state = { 0 };
    saw_initState(&state);
    saw_calcTimeoutDur(&state);
    saw_calcTimeout(&state);

    // diff should be ~1000
    struct timespec t = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &t);
    LINFO("now: %ld,\ttimeout: %ld,\tdiff:%ld", 
        NTIME(t), state.timeout, state.timeout - NTIME(t));


    return 0;
}
