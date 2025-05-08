CC = gcc
CFLAGS = -Wall -O2 -std=c99
LDFLAGS = 
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Create these directories if they don't exist
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Get all C files in the src directory
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
# Generate corresponding object file names
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))

# Main target
all: $(BIN_DIR)/linex

# Link object files to create the executable
$(BIN_DIR)/linex: $(OBJ_FILES)
	$(CC) $(LDFLAGS) $^ -o $@

# Compile C files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Install the binary to /usr/local/bin (requires appropriate permissions)
install: $(BIN_DIR)/linex
	install -m 755 $(BIN_DIR)/linex /usr/local/bin/linex

# Clean object files and executable
clean:
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/linex

# Clean and rebuild
rebuild: clean all

.PHONY: all install clean rebuild