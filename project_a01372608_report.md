# COMP7005 Reliable UDP Protocol
### Will Otterbien, A01372608, 2026

This report describes my implementation of a reliability layer on top of the UDP protocol.

This protocol is called the SLW protocol, because it is based off the sliding window protocol that we discussed in lecture with Go-Back-N as the retransmission mechanism (more on that in the **Protocol Implementation** section).


## Application Message format and Integrity

The protocol uses a network segment of 4096 bytes, these segments contain the SLW header & whatever user supplied bytes there are.


## Protocol Implementation

From a calling applications perspective, the three most important primitives that the protocol library exposes are `slw_send`, `slw_recv`, `slw_process`. Other interfaces that are exposed to calling applications are the protocol state initialization / cleanup functions `slw_init` & `slw_free`, and the `slw_window_empty` function.


### `slw_send`

### `slw_recv`

### `slw_process`

### `slw_`


### Server's use of SLW

### Client's use of SLW

This section will diverge from the raw protocol implementation & discuss how the 


## Testing Scenarios

## Throughput Comparison




## Reflection and Novelty



