CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -D_GNU_SOURCE
LDFLAGS = 
TARGET = cruntime

# Directories
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

# Source files
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/core/runtime.c \
          $(SRC_DIR)/namespace/namespace.c \
          $(SRC_DIR)/cgroup/cgroup.c \
          $(SRC_DIR)/network/network.c \
          $(SRC_DIR)/storage/storage.c \
          $(SRC_DIR)/utils/utils.c

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default target
all: $(BIN_DIR)/$(TARGET)

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR)/core $(OBJ_DIR)/namespace $(OBJ_DIR)/cgroup $(OBJ_DIR)/network $(OBJ_DIR)/storage $(OBJ_DIR)/utils:
	mkdir -p $@

# Build executable
$(BIN_DIR)/$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@echo "Built $(TARGET) successfully"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR) $(OBJ_DIR)/core $(OBJ_DIR)/namespace $(OBJ_DIR)/cgroup $(OBJ_DIR)/network $(OBJ_DIR)/storage $(OBJ_DIR)/utils
	$(CC) $(CFLAGS) -c $< -o $@

# Install
install: $(BIN_DIR)/$(TARGET)
	install -d /usr/local/bin
	install -m 755 $(BIN_DIR)/$(TARGET) /usr/local/bin/
	install -d /var/run/cruntime
	install -d /var/lib/cruntime/state
	install -d /var/lib/cruntime/images
	install -d /var/run/cruntime/netns
	@echo "Installed $(TARGET) to /usr/local/bin"

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstalled $(TARGET)"

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned build artifacts"

# Development build with debug symbols
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Static build
static: LDFLAGS += -static
static: clean all

# Check for required tools
check:
	@echo "Checking for required tools..."
	@which ip > /dev/null || echo "Warning: 'ip' command not found (iproute2 package)"
	@which iptables > /dev/null || echo "Warning: 'iptables' not found"
	@which nsenter > /dev/null || echo "Warning: 'nsenter' not found (util-linux package)"
	@[ -d /sys/fs/cgroup ] || echo "Warning: cgroups v2 not available at /sys/fs/cgroup"
	@echo "Check complete"

# Help
help:
	@echo "CRuntime Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build the container runtime (default)"
	@echo "  install  - Install to /usr/local/bin (requires root)"
	@echo "  uninstall- Remove from /usr/local/bin"
	@echo "  clean    - Remove build artifacts"
	@echo "  debug    - Build with debug symbols"
	@echo "  static   - Build static binary"
	@echo "  check    - Check for required system tools"
	@echo "  help     - Show this help message"

.PHONY: all install uninstall clean debug static check help
