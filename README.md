# UDP network simulator starter

This starter separates the application from the simulated network:

```text
udp_client -> network.c -> udp_server
udp_client <- network.c <- udp_server
```

The client and server use ordinary UDP. The provided `network.c` relay can drop,
independently delay, and corrupt datagrams in either direction. Independent
random delays can naturally make later datagrams arrive before earlier ones.

## Build

Run on Linux:

```sh
make build
```

## Run the echo demo

Open three terminals from this directory.

Terminal 1, start the real server on port 9001:

```sh
./udp_server 9001
```

Terminal 2, start the simulated network on port 9000:

```sh
./network --listen-port 9000 --server-ip 127.0.0.1 --server-port 9001 --loss 0.10 --delay 0.30 --corrupt 0.05 --max-delay-ms 500 --seed 42
```

Terminal 3, point the client at the simulated network, not the real server:

```sh
./udp_client 127.0.0.1 9000
```

Every line typed into the client is one UDP datagram. The server prints any
datagram that reaches it and sends the same bytes back. The reply also travels
through `network.c`, so either direction may experience loss, delay, naturally
out-of-order arrival, or payload corruption.

The client watches stdin and its UDP socket at the same time. It can therefore
send several lines without waiting for earlier echoes. It does not retransmit.
There is no send-then-wait cycle: stdin readiness triggers `sendto()`, while UDP
socket readiness independently triggers `recvfrom()` through the same `poll()`.
This starter deliberately has no reliable application protocol yet: the
server's echo is application data, not a reliability ACK.

## Demonstrate delay-induced out-of-order delivery

Give every datagram its own independent random delay of up to one second:

```sh
./network --listen-port 9000 --server-ip 127.0.0.1 --server-port 9001 --loss 0 --delay 1 --corrupt 0 --max-delay-ms 1000 --seed 42
```

Start the server and client as above, then paste many numbered lines into the
client at once, for example `1` through `20`. Each datagram receives a different
delay, so a later datagram can become due first and reach the server first.
The exact arrival order depends on the seed and timing.

Typing by hand can be too slow because the previous datagram may already have
been forwarded before the next one arrives. Out-of-order arrival needs at least
two independently delayed datagrams in flight at the same time. There is no
special out-of-order mode: it emerges naturally from variable network delay.

## `network` arguments

```text
./network --listen-port PORT --server-ip IPV4 --server-port PORT [options]
```

- `--listen-port`: the port used by the client, for example `9000`.
- `--server-ip` and `--server-port`: the real server endpoint.
- `--loss`: probability that a datagram is dropped, in `[0,1]`.
- `--delay`: probability that a datagram receives its own independent random
  delay, in `[0,1]`. Different delays can change arrival order.
- `--corrupt`: probability that one randomly selected bit in a non-empty
  datagram's payload is flipped, in `[0,1]`.
- `--max-delay-ms`: the largest random delay used by `--delay`, in milliseconds.
- `--seed`: optional random seed. Reusing a seed makes a test easier to repeat.

Only `--listen-port`, `--server-ip`, and `--server-port` are required. The three
probabilities default to `0`, and `--max-delay-ms` defaults to `500`. This makes
it easy to enable or adjust one network condition without counting positional arguments. Run
`./network --help` to see the complete list.

## Why the additional arguments are needed

The three `[0,1]` values describe probabilities, not endpoints or time. The
relay therefore also needs two port numbers, the real server address, and a
maximum delay measured in milliseconds. The optional seed is useful for
repeatable demonstrations and grading.

Corruption happens inside the relay before it forwards the datagram. The relay
therefore sends a valid UDP datagram containing altered application bytes. This
simulates payload corruption visible to the application; it is not a raw packet
with a deliberately invalid UDP checksum.

The relay intentionally supports one active client to keep the code small. A
later version can identify clients with a session ID when the project adds its
reliable application protocol.
