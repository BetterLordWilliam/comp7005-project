#!/usr/bin/bash

INTERVAL=0.05
DURATION=10

useage() {
    cat <<EOF >&2
Useage $0 [-w window_size] [-r reliability] [-h]

Options
    -w <size>           sliding window size (default is 1)
    -r <reliability>    determines if a reliable or unreliable network will be used (default is 1)
    -h                  help message
EOF
}

timestamp="$(date +%s)"

window=1
reliable=1

while getopts ":w:r:h" opt; do
    case "$opt" in
        w) window="$OPTARG" ;;
        r) reliable="$OPTARG" ;;
        h) useage; exit 0 ;;
        :) useage ;;
    esac
done

server_out_log="./logs/$timestamp-server-out.log"
server_err_log="./logs/$timestamp-server-err.log"
./build/udp_server 8080 "$window"   \
    > "$server_out_log"             \
    2> "$server_err_log"            \
    & server_pid=$!

network_log="./logs/$timestamp-network.log"
if (( reliable == 1)); then
    printf "reliable network\n"
    ./build/network                                             \
        --server-ip 127.0.0.1 --server-port 8080                \
        --listen-port 8081 --loss 0.0 --delay 0.0               \
        --corrupt 0.0 --max-delay-ms 0 --seed 7005              \
        &> "$network_log"                                       \
        & network_pid=$!
else
    printf "unreliable network\n"
    ./build/network                                             \
        --server-ip 127.0.0.1 --server-port 8080                \
        --listen-port 8081 --loss 0.10 --delay 0.50             \
        --corrupt 0.05 --max-delay-ms 500 --seed 7005           \
        &> "$network_log"                                       \
        & network_pid=$!
fi

client_out_log="./logs/$timestamp-client-out.log"
client_err_log="./logs/$timestamp-client-err.log"
CMD="./build/udp_client 127.0.0.1 8081 $window"

printf "starting test $timestamp...\n"

CMD_OUT="$(
python3 - "$INTERVAL" "$DURATION" <<EOF | \
    $CMD 2> "$client_err_log" | tee "$client_out_log"
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
printf "test $timestamp complete\n"
cat <<EOF >&1
test $timestamp log files:
[server-out]($server_out_log), [server-err]($server_err_log), [client-out]($client_out_log), [client-err]($client_err_log), [network]($network_log)
EOF

printf "%s\n" "$CMD_OUT" | grep "^echo:" | wc -l


kill "$server_pid" 2> /dev/null
kill "$network_pid" 2> /dev/null

