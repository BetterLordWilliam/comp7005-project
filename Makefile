CC := gcc
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -O2

.RECIPEPREFIX = >
.PHONY: all build clean


BUILD_DIR = build


all: build

build: network udp_server udp_client


network: network.c
> $(CC) $(CFLAGS) network.c -o $(BUILD_DIR)/network.o

udp_server: udp_server.c
> $(CC) $(CFLAGS) udp_server.c -o $(BUILD_DIR)/udp_server.o

udp_client: udp_client.c
> $(CC) $(CFLAGS) udp_client.c -o $(BUILD_DIR)/udp_client.o


clean:
> rm -f $(BUILD_DIR)/*.o

