
# ==========================================
# Metro Management System - Linux Makefile
# ========================================== 

CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude
LDFLAGS = -pthread -lm

TARGET = build/metro

# Source files
SRC = src/main.c \
      src/bst.c \
      src/route.c \
      src/storage.c \
      src/auth.c \
      src/audit_log.c \
      src/audit_worker.c

# Object files
OBJ = $(SRC:src/%.c=build/%.o)

# Default target
all: $(TARGET)

# Link executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(TARGET)
	@echo "=========================================="
	@echo "Metro Management System built successfully!"
	@echo "Executable: $(TARGET)"
	@echo "=========================================="

# Compile each C source file
build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

# Run application
run: $(TARGET)
	./$(TARGET)

# Clean generated files
clean:
	rm -f build/*.o
	rm -f $(TARGET)

# Rebuild everything
rebuild: clean all

# Run existing C unit tests
test:
	@echo "Compile and execute individual test files separately."

.PHONY: all run clean rebuild test
