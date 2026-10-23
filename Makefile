CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2

.RECIPEPREFIX = >
.PHONY: all build clean


BUILD_DIR = build
SRC_DIR = src


all: build

build: network udp_server udp_client


network: $(SRC_DIR)/network.c
> $(CC) $(CFLAGS) $(SRC_DIR)/network.c -o $(BUILD_DIR)/network

udp_server: $(SRC_DIR)/udp_server.c
> $(CC) $(CFLAGS) $(SRC_DIR)/udp_server.c -o $(BUILD_DIR)/udp_server

udp_client: $(SRC_DIR)/udp_client.c
> $(CC) $(CFLAGS) $(SRC_DIR)/udp_client.c -o $(BUILD_DIR)/udp_client


clean:
> rm $(BUILD_DIR)/network
> rm $(BUILD_DIR)/udp_server
> rm $(BUILD_DIR)/udp_client

