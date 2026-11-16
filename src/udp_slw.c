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


static void slw_write_packet(
   char* raw_buf, ssize_t raw_buf_bytes, slw_packet_t* pkt
) {
    memset(raw_buf, 0, SLW_HEADER);
    raw_buf[0] = pkt->id;
    raw_buf[1] = pkt->ack;
    raw_buf[2] = pkt->flags;
    raw_buf[3] = pkt->checksum;  // initially 0
    raw_buf[5] = pkt->length;
    
    uint16_t nlength = htons(pkt->length);
    memcpy(raw_buf + 5, &nlength, sizeof(uint16_t));

    uint16_t checksum = slw_compute_checksum(
        (uint8_t*)raw_buf, SLW_HEADER, raw_buf + SLW_HEADER, pkt->length);
    uint16_t nchecksum = htons(checksum);
    memcpy(raw_buf + 3, &nchecksum, sizeof(uint16_t));
    memcpy(raw_buf + SLW_HEADER, pkt->data, pkt->length);
}


static void slw_init_packet(
    slw_packet_t* pkt, uint8_t id, uint8_t ack, uint8_t flags,
    const char* buf, uint16_t len
) {
    pkt->id = id;
    pkt->ack = ack;
    pkt->flags = flags;
    pkt->length = len;
    pkt->checksum = 0; // computed when written to the network segment
    memcpy(pkt->data, buf, len);
    /*
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
    */
}


static inline int slw_advance(int idx, slw_window_t* win) {
    return (idx + 1) % win->slots;
}

static inline int slw_packet_size(slw_packet_t* pkt) {
    return SLW_HEADER + pkt->length;
}

static inline time_t slw_nmtime_ms(struct timespec ts) {
    return ( ts.tv_sec * 1000 ) + ( ts.tv_nsec / 1000000 );
}

static inline void slw_calc_timeout(slw_state_t* state) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    state->timeout = slw_nmtime_ms(ts) + state->timeoutdur;
}


int slw_init(slw_state_t* state, int fd, int window_slots, int timeoutdur)
{
    state->fd           = fd;
    state->slots        = window_slots;
    state->timeoutdur   = timeoutdur;

    state->rcvwind = (slw_window_t){
        .slots  = 2,
        .length = 1,
        .base   = 0,
        .next   = 0,
        .ack    = 0
    };
    state->sndwind = (slw_window_t){
        .slots  = state->slots,
        .length = state->slots - 1,
        .base   = 0,
        .next   = 0,
        .ack    = 0,
    };

    state->rcvwind.slot_filled  = (int*)calloc(
        state->rcvwind.slots, sizeof(int)
    );
    if (state->rcvwind.slot_filled == NULL) {
#ifdef UDP_DEBUG
        LERROR("error allocating slot_filled for rcvwind");
#endif
        return -1;
    }
    state->rcvwind.packets      = (slw_packet_t*)calloc(
        state->rcvwind.slots, sizeof(slw_packet_t)
    );
    if (state->rcvwind.packets == NULL) {
#ifdef UDP_DEBUG
        LERROR("error allocating packets for rcvwind");
#endif
        return -1;
    }
    
    state->sndwind.slot_filled = (int*)calloc(
        state->sndwind.slots, sizeof(int)
    );
    if (state->sndwind.slot_filled == NULL) {
#ifdef UDP_DEBUG
        LERROR("error allocating slot_filled for sndwind");
#endif
        return -1;
    }
    state->sndwind.packets = (slw_packet_t*)calloc(
        state->sndwind.slots, sizeof(slw_packet_t)
    );
    if (state->sndwind.packets == NULL) {
#ifdef UDP_DEBUG
        LERROR("error allocating packets for sndwind");
#endif
        return -1;
    }

    return 0;
}

void slw_free(slw_state_t* state) {
    free(state->rcvwind.slot_filled);
    free(state->rcvwind.packets);
    free(state->sndwind.slot_filled);
    free(state->sndwind.packets);
}


static int slw_retransmit(
    slw_state_t* state, const struct sockaddr* dest_addr, socklen_t addrlen
) {
    slw_window_t *win = &state->sndwind;
    int curr = win->base;

    while (curr != win->next) {
        if (win->slot_filled[curr]) {
            char raw_buf[BUFFER_SIZE];
            slw_write_packet(raw_buf, BUFFER_SIZE, &win->packets[curr]);
            ssize_t r = sendto_s(
                state->fd, raw_buf, slw_packet_size(&win->packets[curr]), 0, dest_addr, addrlen);
            if (r == -1) {
#ifdef UDP_DEBUG
                LERROR("retransmission failed");
#endif
                return SLW_ERROR;
            }
        }
        curr = slw_advance(curr, win);
    }
    return SLW_OK;
}


ssize_t slw_send(slw_state_t* state, const char* buf, size_t bufsize,
    const struct sockaddr* socka, socklen_t sockalen
) {
    return SLW_OK;
}

ssize_t slw_recv(slw_state_t* state, char* buf, size_t bufSize
) {
    return SLW_OK;
}

int slw_process(slw_state_t* state,
    const struct sockaddr* socka, socklen_t sockalen
) {
    return SLW_OK;
}

