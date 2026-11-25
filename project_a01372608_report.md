---
title: COMP7005 SLW Protocol
version: 1
---

# COMP7005 SLW Protocol
### A Reliable UDP Protocol

*Will Otterbien, A01372608, 2026*


This report describes my implementation of a reliability layer on top of the UDP protocol.

This protocol is called the SLW protocol, because it is based off the sliding window protocol that we discussed in lecture with Go-Back-N as the retransmission mechanism (more on that in the **Protocol Implementation** section).


## Application Message format and Integrity

The SLW protocol uses a network segment of 4096 bytes (this was the given buffer size & I decided to incorporate this as a segment size limitation), in the source the maximum network segment size is defined as `BUFFER_SIZE` in `include/common.h`.

These segments contain the SLW protocol header & some number of user data bytes.
 
```c
typedef struct slw_packet {
    uint8_t id;
    uint8_t ack;
    uint8_t flags;
    uint16_t checksum;
    uint16_t length;
    char data[SLW_MAX_PAYLOAD];
} slw_packet_t;
```

The header size is 7 bytes. One byte id number gives a 2^8 ID space, if this is the ID space then ACK field must also be 1 byte. Another byte is given for flags, which is how SLW determines the type of packet. SLW only uses 2 bits of this field at present, but in the name of consistency and potential extensibility it's nice to have another 5 bits to put to use. The checksum field is 2 bytes because this is the length of the checksum used by TCP, no need to reinvent the wheel. The length being 2 bytes means that maximum possible number of user bytes is (according to this field) is 2^16 = 65,536, cozily capping the maximum number of bytes to 65,535 which is also the maximum UDP datagram size. Of course the 7 header bytes would need to be subtracted from whatever user amount is supplied still, but this is handled by the protocol primitives to be discussed in the next section.

When written to the kernel buffer, the bytes are written in the order they are declared in the `slw_packet_t` type.

As a diagram, the header takes the following shape, this shape nested within the UDP datagram:

```mermaid
packet
+8: "ID"
+8: "ACK"
16-21: "Reserved"
22: "DAT"
23: "ACK"
+16: "Checksum"
+16: "Length"
```


## Protocol Implementation

As mentioned in the introduction, SLW is an implementation of the sliding window protocol. The SLW library is defined in `include/udp_slw.h` & implemented in `src/udp_slw.c` using go-back-N for retransmission.

The goal of SLW is to improve upon the reliability of UDP, specifically with regard to the following:
- recovery from data loss
- recovery from out of order data
- recovery from data corruption
- recovery from duplicated data

At a macro level, SLW provides peer applications with send and receive windows within which outgoing & incoming packets can be queued. SLW guarantees that packets will be uncorrupted & that packets will be delivered in order, without duplication & retransmitted if necessary. Duplex communication is enabled by "piggy-backed" ACKs, meaning that any packet can simultaneously serve as a DAT & ACK packet, transmitting reply bytes with acknowledging received packets. SLW will adjust retransmission timers according to the state of the network, which will reduce unnecessary retransmission (as much as can be done using go-back-N strategy anyways).

```mermaid
sequenceDiagram
    participant p1 as peer1
    participant p2 as peer2

    note right of p1: p1 sends pkt0, pkt1
    note right of p1: p1 starts timer
    p1 ->> p2: ID: 0, ACK: 0, FLAGS: 2, ..., DATA: "m1"
    p1 ->> p2: ID: 1, ACK: 0, FLAGS: 2, ..., DATA: "m2"
    note right of p2: p2 receive's pkt0, pkt1, p2 sends pkt0 reply
    note right of p2: p2 starts timer
    p2 ->> p1: ID: 0, ACK: 2, FLAGS: 3, ..., DATA: "r1"
    note right of p1: p1 receives ACK 1, frees slots in send window
    note right of p1: p1 has no replies, transmits ACK 1
    p1 ->> p2: ID: 0, ACK: 1, FLAGS: 1, ...
    note right of p2: p2 receives ACK 1, frees slot in send window

    note right of p1: p1 sends pkt2, pkt3, pkt4
    note right of p1: p1 starts timer
    p1 ->> p2: ID 2, ACK: 1, FLAGS: 3, ..., DATA: "m3"
    note over p1,p2: pk2 is lost
    p1 ->> p2: ID 3, ACK: 1, FLAGS: 3, ..., DATA: "m4"
    p1 ->> p2: ID 4, ACK: 1, FLAGS: 3, ..., DATA: "m5"
    note right of p2: p2 receives pkt3, pkt4, out of order, discards

    note right of p1: p1 timeout expires
    note right of p1: p1 re-sends pkt2, pkt3, pkt4
    note right of p1: p1 starts timer
    p1 ->> p2: ID 2, ACK: 1, FLAGS: 3, ..., DATA: "m3"
    p1 ->> p2: ID 3, ACK: 1, FLAGS: 3, ..., DATA: "m4"
    p1 ->> p2: ID 4, ACK: 1, FLAGS: 3, ..., DATA: "m5"
    note right of p2: p2 receives pk1,pk2, and pkt3
    note right of p2: p2 has no replies, transmit ACK 5
    p2 ->> p1: ID: 0, ACK: 5, FLAGS: 1, ...
    note right of p1: p1 receives ACK 5, frees slot(s) in send window
```
*etc*


From a calling applications perspective, the three most important primitives that the protocol library exposes are `slw_send`, `slw_recv`, `slw_process`. Other interfaces are exposed to calling applications, `slw_init` & `slw_free` protocol state initializers/"uninitializers", and the `slw_window_empty` function, but these functions are helpful utilities have a limited role compared to the three aforementioned primitives.

Rather than meticulously comb over every function in the library, we will discuss the implementation of SLW in terms of the implementation of the three primitives & how the `udp_server` & `udp_client` applications were modified to use the SLW protocol, instead of pure UDP.


### `slw_send`

```c
ssize_t slw_send(slw_state_t* state, const char* buf, size_t bufsize,
    const struct sockaddr* socka, socklen_t sockalen);
```

In brief, the purpose of `slw_send` is to attempt to append a packet or packets to the SLW protocol's send window.

If the send



### `slw_recv`

```c
ssize_t slw_recv(slw_state_t* state, char* buf, size_t bufSize);
```

In brief, the purpose of `slw_recv` is to attempt to consolidate available packets in the SLW protocol's send window & write their data into the user's supplied buffer.


### `slw_process`

*perhaps the most important function*

```c
int slw_process(slw_state_t* state,
    struct sockaddr* socka, socklen_t* sockalen);
```

In brief, the purpose of `slw_process` is to handle the transmission logic of the protocol, typically executed first when the caller has determined that the underlying socket has data to be read from the kernel. This function reads UDP packets from the wire, & parses the SLW packet.




### Server's use of SLW

This section will discuss how `udp_server` was modified to work with the SLW protocol.


### Client's use of SLW

This section will discuss how `udp_client` was modified to work with the SLW protocol.


## Testing Scenarios

## Throughput Comparison




## Reflection and Novelty



## Appendix

### Serial Arithmetic and RFC 1982

Because SLW has an ID space of 2^8 and the fact that SLW uses go-back-N for retransmission, maximum possible windows size would be 255, except that I implement the comparison arithmetic for serial numbers defined in [RFC 1982](https://www.rfc-editor.org/info/rfc1982/), where such comparisons only have meaning for 2^(n-1) in a space of 2^(n). Practically, this means that the maximum window size is 127 (128-1).

### SLW Checksum Calculation


