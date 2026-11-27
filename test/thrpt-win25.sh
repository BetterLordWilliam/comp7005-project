#!/usr/bin/bash

INTERVAL=0.05
DURATION=10

./build/udp_server 8080 25          \
    > ./logs/win25-server.log       \
    2> ./logs/win25-server-err.log  \
    & server_pid=$!

./build/network                                     \
    --server-ip 127.0.0.1 --server-port 8080        \
    --listen-port 8081 --loss 0.10 --delay 0.50     \
    --corrupt 0.05 --max-delay-ms 500 --seed 7005   \
    &> ./logs/win25-network.log                     \
    & network_pid=$!


CMD="./build/udp_client 127.0.0.1 8081 25"

CMD_OUT="$(
python3 - "$INTERVAL" "$DURATION" <<'EOF' | \
    $CMD 2> ./logs/win25-client-err.log | tee ./logs/win25-client.log
import sys, time

interval = float(sys.argv[1])
duration = float(sys.argv[2])

start = time.monotonic()
deadline = start + duration
i = 1
while True:
    now = time.monotonic()
    if now >= deadline:
        break
    print(f"msg-{i}", flush=True)
    i += 1
    next_tick = start + i * interval
    sleep_for = min(next_tick, deadline) - time.monotonic()
    if sleep_for > 0:
        time.sleep(sleep_for)
EOF
)"
printf "%s\n" "$CMD_OUT" | grep "^echo:" | wc -l


kill "$server_pid" 2> /dev/null
kill "$network_pid" 2> /dev/null

