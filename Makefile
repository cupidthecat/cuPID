CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lncurses

# Directories
SRC_DIR = src
LIB_DIR = lib/cupidconf
BUILD_DIR = build
BIN_DIR = bin

# Source files
MAIN_SRC = $(SRC_DIR)/main.c
CONFIG_SRC = $(SRC_DIR)/config.c
PROCESS_SRC = $(SRC_DIR)/process.c
CPU_SRC = $(SRC_DIR)/cpu.c
MEMORY_SRC = $(SRC_DIR)/memory.c
LIB_SRC = $(LIB_DIR)/cupidconf.c

# Object files
MAIN_OBJ = $(BUILD_DIR)/main.o
CONFIG_OBJ = $(BUILD_DIR)/config.o
PROCESS_OBJ = $(BUILD_DIR)/process.o
CPU_OBJ = $(BUILD_DIR)/cpu.o
MEMORY_OBJ = $(BUILD_DIR)/memory.o
LIB_OBJ = $(BUILD_DIR)/cupidconf.o
OBJS = $(MAIN_OBJ) $(CONFIG_OBJ) $(PROCESS_OBJ) $(CPU_OBJ) $(MEMORY_OBJ) $(LIB_OBJ)

# Target executable
TARGET = $(BIN_DIR)/cuPID

# Default target
all: $(TARGET)

# Create directories if they don't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build the main executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile main.c
$(MAIN_OBJ): $(MAIN_SRC) $(SRC_DIR)/cpu.h $(SRC_DIR)/memory.h $(SRC_DIR)/process.h $(SRC_DIR)/config.h $(LIB_DIR)/cupidconf.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(MAIN_SRC) -o $(MAIN_OBJ)

$(CONFIG_OBJ): $(CONFIG_SRC) $(SRC_DIR)/config.h $(LIB_DIR)/cupidconf.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(CONFIG_SRC) -o $(CONFIG_OBJ)

$(PROCESS_OBJ): $(PROCESS_SRC) $(SRC_DIR)/process.h $(SRC_DIR)/config.h $(LIB_DIR)/cupidconf.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(PROCESS_SRC) -o $(PROCESS_OBJ)

$(CPU_OBJ): $(CPU_SRC) $(SRC_DIR)/cpu.h $(SRC_DIR)/config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(CPU_SRC) -o $(CPU_OBJ)

$(MEMORY_OBJ): $(MEMORY_SRC) $(SRC_DIR)/memory.h $(SRC_DIR)/config.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(MEMORY_SRC) -o $(MEMORY_OBJ)

# Compile cupidconf.c
$(LIB_OBJ): $(LIB_SRC) $(LIB_DIR)/cupidconf.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(LIB_SRC) -o $(LIB_OBJ)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install (optional - copies binary to /usr/local/bin)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/cupid

.PHONY: all clean install uninstall

