CC := gcc
CFLAGS := -std=gnu17 -Wall -Wextra -Wpedantic -O2 -Iinclude

.RECIPEPREFIX = >
.PHONY: all build clean


BUILD_DIR = ./build
SRC_DIR = ./src


all: build_project

debug: CFLAGS += -g
debug: CFLAGS += -DUDP_DEBUG
debug: build_project


test: build_project_tester

debug_test: CFLAGS += -g
debug_test: CFLAGS += -DUDP_DEBUG
debug_test: build_project_tester


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


# locations

$(BUILD_DIR):
> mkdir -p $(BUILD_DIR)


# utilities

clean: clean_obj

clean_obj:
> rm $(BUILD_DIR)/*.o

# kinda dangerous
dclean:
> echo "rm $(BUILD_DIR)/*"

