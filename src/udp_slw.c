#include "udp_slw.h"


static uint16_t slw_compute_checksum(
    const uint8_t* header, uint16_t header_len,
    const char* body, uint16_t body_len
) {
#ifndef UDP_DEBUG
    //assert(header != NULL);
    //assert(body != NULL);
#endif
    uint32_t sum = 0;
    for (int i = 0; i < header_len; i++) {
        if (i == 3 || i == 4) continue;
        sum += (uint8_t)header[i];
    }
    for (int i = 0; i < body_len; i++) {
        sum += (uint8_t)body[i];
    }
#ifdef UDP_DEBUG
    LINFO("computed checksum value of %d", sum);
#endif
    return (uint16_t)sum;
}


static int slw_parse_packet(
   const char* raw_buf, ssize_t raw_buf_bytes, slw_packet_t* pkt
) {
    if (raw_buf_bytes < SLW_HEADER) {
#ifdef UDP_DEBUG
        LERROR("packet must be at least the size of the header");
#endif
        return 0;
    }
    pkt->id     = (uint8_t)raw_buf[0];
    pkt->ack    = (uint8_t)raw_buf[1];
    pkt->flags  = (uint8_t)raw_buf[2];

    // re-order multi-byte header fields
    uint16_t nchecksum, nlength;
    memcpy(&nchecksum, raw_buf + 3, sizeof(uint16_t));
    memcpy(&nlength, raw_buf + 5, sizeof(uint16_t));

    pkt->checksum = ntohs(nchecksum);
    pkt->length = ntohs(nlength);
    
    if (
        pkt->length > SLW_MAX_PAYLOAD
        || raw_buf_bytes < SLW_HEADER + pkt->length
    ) {
#ifdef UDP_DEBUG
        LERROR("packet length cannot exceed the maximum payload size or buffer bytes");
#endif
        return 0;
    }

    memcpy(pkt->data, raw_buf + SLW_HEADER, pkt->length);

    uint16_t computed = slw_compute_checksum(
        (const uint8_t*)raw_buf, SLW_HEADER,
        pkt->data, pkt->length
    );
    if (computed != pkt->checksum) {
#ifdef UDP_DEBUG
        LERROR("packet checksum did not match calculated checksum");
#endif
        return 0;
    }

    return 1;
}


static void slw_init_packet(
    slw_packet_t* pkt, uint8_t id, uint8_t ack, uint8_t flags,
    const char* buf, uint16_t len
) {
    pkt->id = id;
    pkt->ack = ack;
    pkt->flags = flags;
    pkt->length = len;
    memcpy(pkt->data, buf, len);

    uint8_t raw_hdr[SLW_HEADER];
    raw_hdr[0] = pkt->id;
    raw_hdr[1] = pkt->ack;
    raw_hdr[2] = pkt->flags;
    
    uint16_t nchecksum = 0;
    uint16_t nlength = htons(pkt->length);
    
    memcpy(raw_hdr + 3, &nchecksum, sizeof(uint16_t));
    memcpy(raw_hdr + 5, &nlength, sizeof(uint16_t));

    pkt->checksum = slw_compute_checksum(
        raw_hdr, SLW_HEADER, pkt->data, pkt->length
    );
}

