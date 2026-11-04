#include <string.h>
#include <stdio.h>

#include "common.h"
#include "udp_saw.h"

int main() {
    // LINFO("this is msg");
    // LWARN("this is a warning");
    // LERROR("this is an error");


    char rbuf[4096] = { 0 };
    char wbuf[4096] = { 0 };
   
    saw_header_t h = { 0 };
    saw_initHeader(&h);

    // writing of a packet

#ifdef UDP_DEBUG
    printf("program in debug mode\n");
#endif

    char* str = "this is a test";
    h.length = strlen(str);
    LINFO("%d", h.length);
    LINFO("%d", PLENU(h));
    saw_writePacket(&h, wbuf, PLENU(h), str, h.length);

    FILE* tester = fopen("data.bin", "wb");
    fwrite(wbuf, 1, PLENU(h), tester);
    fclose(tester);


    // read the packet, sort of like reading it from the net
    // we know we need to read 6 bytes (or HEADER_LEN)
    // then we check what the 
    saw_header_t h1 = { 0 };
    saw_initHeader(&h1);

    FILE* tester2 = fopen("data.bin", "rb");
    fread(&rbuf, 1, 4096, tester2);
    
    saw_readPacket(&h1, rbuf, 4096);

    LINFO("READ FOLLOWING PACKET INFO\n\ttype:%d;length:%d",
        h1.packetType, h1.length);
    fclose(tester2);

    return 0;
}
