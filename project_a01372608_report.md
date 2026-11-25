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

The header size is 7 bytes. One byte id number gives a 2^8 ID space, if this is the ID space then ACK field must also be 1 byte. Another byte is given for flags, which is how SLW determines the type of packet. SLW only uses 2 bits of this field at present, but in the name of consistency and potential extensiblity it's nice to have another 5 bits to put to use. The checksum field is 2 bytes because this is the length of the checksum used by TCP, no need to reinvent the wheel. The length being 2 bytes means that maximum possible number of user bytes is (according to this field) is 2^16 = 65,536, cozily capping the maximum number of bytes to 65,535 which is also the maximum UDP datagram size. Of course the 7 header bytes would need to be subtracted from whatever user amount is supplied still, but this is handled by the protocol primitives to be discussed in the next section.

When written to the kernel buffer, the bytes are written in the order they are declared in the `slw_packet_t` type.

As a diagram, the protocol would appear to be the following:

```mermaid
packet
+8: "ID"
+8: "ACK"
+8: "Flags"
+16: "Checksum"
+16: "Length"
```


## Protocol Implementation

From a calling applications perspective, the three most important primitives that the protocol library exposes are `slw_send`, `slw_recv`, `slw_process`. Other interfaces are exposed to calling applications, `slw_init` & `slw_free` protocol state initializers/uninitializers, and the `slw_window_empty` function, but these functions are helpful utilities have a limited role compared to the three aforementioned primitives.


### `slw_send`

```c
ssize_t slw_send(slw_state_t* state, const char* buf, size_t bufsize,
    const struct sockaddr* socka, socklen_t sockalen);
```



### `slw_recv`

```c
ssize_t slw_recv(slw_state_t* state, char* buf, size_t bufSize);
```

### `slw_process`

```c
int slw_process(slw_state_t* state,
    struct sockaddr* socka, socklen_t* sockalen);
```


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

