#include "udp_slw.h"
#include <bits/time.h>


static uint16_t slw_compute_checksum(
    const uint8_t* header, uint16_t header_len,
    const char* body, uint16_t body_len
) {
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


static int slw_read_packet(
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


static int slw_write_packet(
   char* raw_buf, ssize_t raw_buf_bytes, slw_packet_t* pkt
) {
    if (raw_buf_bytes < SLW_HEADER + pkt->length) {
#ifdef UDP_DEBUG
        LERROR("supplied buffer was too small must be large enough for slw heeader & packet length");
#endif
        return 0;
    }
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
#ifdef UDP_DEBUG
    LINFO("writing packet produced following checksum %d", checksum);
#endif
    uint16_t nchecksum = htons(checksum);
    memcpy(raw_buf + 3, &nchecksum, sizeof(uint16_t));
    memcpy(raw_buf + SLW_HEADER, pkt->data, pkt->length);

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
    pkt->checksum = 0; // computed when written to the network segment
    memcpy(pkt->data, buf, len); // should init also check that the data is the correct size?
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
    return abs(idx + 1) % win->slots;
}

static inline int slw_retreat(int idx, slw_window_t* win) {
    return abs(idx - 1) % win->slots;
}

static inline int slw_advancen(int idx, int n, slw_window_t* win) {
    return (idx + n) % win->slots;
}

static inline int slw_seq_less(uint8_t a, uint8_t b) {
    return (uint8_t)(b - a) < (SLW_MAX_SLOTS/2) && a != b;
}

static inline int slw_seq_greater(uint8_t a, uint8_t b) {
    return (uint8_t)(a - b) < (SLW_MAX_SLOTS/2) && a != b;
}

static inline int slw_packet_size(slw_packet_t* pkt) {
    return SLW_HEADER + pkt->length;
}

static inline time_t slw_nmtime_ms(struct timespec ts) {
    return ( ts.tv_sec * 1000 ) + ( ts.tv_nsec / 1000000 );
}

static inline time_t slw_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return slw_nmtime_ms(ts);
}

static inline time_t slw_calc_timeout(slw_state_t* state) {
    time_t time_ms = slw_time_ms();
    return time_ms + state->timeoutdur;
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
    slw_window_t* sndwind = &state->sndwind;
    slw_window_t* rcvwind = &state->rcvwind;

    // cannot send if the send window is full ie.
    int next = slw_advance(sndwind->next, sndwind);
    if (next == sndwind->base) {
#ifdef UDP_DEBUG
        LWARN("send window is full");
#endif
        return SLW_EWOULDBLOCK;
    }

    // calculate the number of required packets
    size_t packets          = (bufsize + SLW_MAX_PAYLOAD) - 1 / SLW_MAX_PAYLOAD;
    size_t offset           = 0;
    size_t sent_count       = 0;

    // chunk
    while (sent_count < packets) {
        // window is full mid chunking
        int candidate_next = slw_advance(sndwind->next, sndwind);
        if (candidate_next == sndwind->base)
            break;

        // create a packet for the data, put it at next, advance next
        //      the packets ACK depends if there is and ACK that needs to be send
        uint16_t chunk_len = (uint16_t)((bufsize - offset < SLW_MAX_PAYLOAD)
            ? bufsize - offset : SLW_MAX_PAYLOAD);
        slw_packet_t* pkt = &sndwind->packets[sndwind->next];
        slw_init_packet(pkt,
            (uint8_t)sndwind->next,
            (uint8_t)rcvwind->ack,
            0,
            buf + offset,
            chunk_len); // if the remaining number of user bytes < max payload
        sndwind->slot_filled[sndwind->next] = 1;

        // advance the next pointer
        int curr = sndwind->next;
        sndwind->next = candidate_next;

        // if first unacked packet calculate the timeout
        if (curr == sndwind->base)
            state->timeout = slw_calc_timeout(state);

        // write the packet to a network segment buffer
        char net_segment[BUFFER_SIZE];
        slw_write_packet(net_segment, BUFFER_SIZE, pkt);

        // send the packet
        ssize_t sent_bytes = sendto_s(state->fd, net_segment, BUFFER_SIZE, 0, socka, sockalen);
        if (sent_bytes < 0) {
#ifdef UDP_DEBUG
            LERROR("faild to send packet");
#endif
            return SLW_ERROR;
        }   

        offset += chunk_len;
        sent_count++;
    }
    return offset;
}

ssize_t slw_recv(slw_state_t* state, char* buf, size_t bufSize
) {
    slw_window_t* rcvwind = &state->rcvwind;
    slw_window_t* sndwind = &state->sndwind;

    // return would block if the receive queue is empty
    // the receive queue is empty if next == base
    if (!rcvwind->slot_filled[rcvwind->base]) {
#ifdef UDP_DEBUG
        LWARN("no packets to receive");
#endif
        return SLW_EWOULDBLOCK;
    }

    size_t total_copied = 0;
    uint8_t last_ack = rcvwind->ack;
    int curr = rcvwind->base;

    // copy what can fit into the users supplied buffer
    while (rcvwind->slot_filled[curr]) {
        slw_packet_t* pkt = &rcvwind->packets[curr];
        if (total_copied + pkt->length > bufSize) {
            break;
        }
        memcpy(buf + total_copied, pkt->data, pkt->length);
        total_copied += pkt->length;
        last_ack = pkt->ack;
        curr = slw_advance(curr, rcvwind);
    }

    // edge case where nothing was copied, the buffer was too small for even 1 packet
    if (total_copied == 0) {
#ifdef UDP_DEBUG
        LERROR("the user supplied buffer is too small for the next packet");
#endif
        return SLW_EBUFSIZE;
    }

    while(rcvwind->base != curr) {
        rcvwind->slot_filled[rcvwind->base] = 0;
        rcvwind->base = slw_advance(rcvwind->base, rcvwind);
    }
    rcvwind->ack = last_ack;

    return (ssize_t)total_copied;
}


int slw_process(slw_state_t* state,
    const struct sockaddr* socka, socklen_t sockalen
) {
    slw_window_t* sndwind = &state->sndwind;
    slw_window_t* rcvwind = &state->rcvwind;

    // check if the timeout has expired & trigger retransmission if so

    // receive an incoming packet

    // parse the packet
    //      drop the packet if checksum validation fails

    // if the ACK is populated (send window updates)
    //      determine that the ACK is valid (ie its actually in the send window)
    //      advance the base of the send window
    //      calculate a new timeout for the new base (oldest unacked packet)

    // if the length of the packet is zero it would be a control packet of sorts
    //      nothing to add to the receive window in that case & right now the only control
    //      packet that the protocol specifies is the pure ACK so do nothing
    //      since pure ACK is handled by the prior block of logic.
    //
    //      in short -> drop 0 length packets

    // if the packet id matches the next expected packet && rcvwind is not full
    //      packet id matches next pointer of the receive window
    //      append the packet to the rcvwind
    //      advance the rcvwind next pointer
    //
    // else if the packet id has already been acknowledge (< base)
    //      transmit a pure ACK for that packet 

    return SLW_OK;
}

