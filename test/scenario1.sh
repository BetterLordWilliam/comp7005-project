#!/usr/bin/bash

# run this from the root of the project


# messages=("I am" "a student", "in" "cmpt7", "0", "0", "5")
messages="I am\na student\nin\ncmpt7\n0\n0\n5\nI am\na student\nin\ncmpt7\n0\n0\n5\nI am\na student\nin\ncmpt7\n0\n0\n5\n" # repeat x3

printf "$messages"

mkdir ./logs 2> /dev/null


# perfect
p_args=("--loss" "0" "--delay" "0" "--corrupt" "0" "--max-delay-ms" "0" "--seed" "7005")

# delay/order
do_args="--loss 0 --delay 1 --corrupt 0 --max-delay-ms 1000 --seed 7005"

# loss
l_args="--loss 0.50 --delay 0.10 --corrupt 0 --max-delay-ms 300 --seed 7005"

# corruption
c_args="--loss 0 --delay 0.10 --corrupt 0.50 --max-delay-ms 300 --seed 7005"

# combined
cb_args="--loss 0.10 --delay 0.50 --corrupt 0.05 --max-delay-ms 500 --seed 7005"


./build/network --server-ip 127.0.0.1 --server-port 8080 --listen-port 8081 "${p_args[@]}" &> ./logs/network.log &
sleep 1
./build/udp_server 8080 &> ./logs/udp_server.log &
sleep 1
printf "$messages" | ./build/udp_client 127.0.0.1 8081 &> ./logs/udp_client.log &

