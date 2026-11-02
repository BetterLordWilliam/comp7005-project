CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2 -Iinclude

.RECIPEPREFIX = >
.PHONY: all build clean


BUILD_DIR = ./build
SRC_DIR = ./src


all: build_project


build_project: $(BUILD_DIR) common udp_saw network udp_server udp_client
build_project_tester: $(BUILD_DIR) common udp_saw udp_tester


# "libraries"

common: $(SRC_DIR)/common.c
> $(CC) $(CFLAGS) $(SRC_DIR)/common.c -c -o $(BUILD_DIR)/common.o

udp_saw: $(SRC_DIR)/udp_saw.c
> $(CC) $(CFLAGS) $(SRC_DIR)/udp_saw.c -c -o $(BUILD_DIR)/udp_saw.o


# binaries

network: $(SRC_DIR)/network.c
> $(CC) $(CFLAGS) $(BUILD_DIR)/*.o $(SRC_DIR)/network.c -o $(BUILD_DIR)/network

udp_server: $(SRC_DIR)/udp_server.c
> $(CC) $(CFLAGS) $(BUILD_DIR)/*.o $(SRC_DIR)/udp_server.c -o $(BUILD_DIR)/udp_server

udp_client: $(SRC_DIR)/udp_client.c
> $(CC) $(CFLAGS) $(BUILD_DIR)/*.o $(SRC_DIR)/udp_client.c -o $(BUILD_DIR)/udp_client


udp_tester: $(SRC_DIR)/udp_tester.c
> $(CC) $(CFLAGS) $(BUILD_DIR)/*.o $(SRC_DIR)/udp_tester.c -o $(BUILD_DIR)/udp_tester


$(BUILD_DIR):
> mkdir -p $(BUILD_DIR)


clean:
> rm $(BUILD_DIR)/*.o
> rm $(BUILD_DIR)/network
> rm $(BUILD_DIR)/udp_server
> rm $(BUILD_DIR)/udp_client

