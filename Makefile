CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LDFLAGS = -lncurses

# Directories
SRC_DIR = src
LIB_DIR = lib/cupidconf
BUILD_DIR = build
BIN_DIR = bin

# Source files
MAIN_SRC = $(SRC_DIR)/main.c
LIB_SRC = $(LIB_DIR)/cupidconf.c

# Object files
MAIN_OBJ = $(BUILD_DIR)/main.o
LIB_OBJ = $(BUILD_DIR)/cupidconf.o
OBJS = $(MAIN_OBJ) $(LIB_OBJ)

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
$(MAIN_OBJ): $(MAIN_SRC) $(LIB_DIR)/cupidconf.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(LIB_DIR) -c $(MAIN_SRC) -o $(MAIN_OBJ)

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

