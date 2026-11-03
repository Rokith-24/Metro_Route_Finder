
# ==========================================================
# METRO MANAGEMENT SYSTEM
# Linux Build, Testing and Code Analysis Makefile
# ==========================================================

CC = gcc
CXX = g++

# ----------------------------------------------------------
# Compiler flags
# ----------------------------------------------------------

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -Iinclude -pthread
CXXFLAGS = -std=c++14 -Wall -Wextra -Wpedantic -g -Iinclude

LDFLAGS = -pthread -lm

TARGET = build/metro
GTEST_TARGET = build/metro_gtests

# ----------------------------------------------------------
# Source files
# ----------------------------------------------------------

SRC = src/main.c \
      src/bst.c \
      src/route.c \
      src/storage.c \
      src/auth.c \
      src/audit_log.c \
      src/audit_worker.c

OBJ = $(patsubst src/%.c,build/%.o,$(SRC))

# ----------------------------------------------------------
# Google Test configuration
# ----------------------------------------------------------

GTEST_SRC = test_gtest/test_bst_gtest.cpp \
            test_gtest/test_route_gtest.cpp \
            test_gtest/test_storage_gtest.cpp \
            test_gtest/test_auth_gtest.cpp \
            test_gtest/test_audit_log_gtest.cpp

GTEST_MODULES = bst route storage auth audit_log

GTEST_OBJ = $(addprefix build/gtest_obj/,$(addsuffix .o,$(GTEST_MODULES)))

GTEST_LIBS = $(shell pkg-config --libs gtest_main 2>/dev/null || echo "-lgtest_main -lgtest")

# ----------------------------------------------------------
# Static analysis configuration
# ----------------------------------------------------------

CPPCHECK = cppcheck

MISRA_ADDON ?= /usr/share/cppcheck/addons/misra.py

# ----------------------------------------------------------
# Default target
# ----------------------------------------------------------

all: $(TARGET)

# ----------------------------------------------------------
# Build application
# ----------------------------------------------------------

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	@echo "Metro Management System built successfully!"

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------------------------------------
# Run application
# ----------------------------------------------------------

run: $(TARGET)
	./$(TARGET)

# ----------------------------------------------------------
# Cppcheck - Static code analysis
# ----------------------------------------------------------

cppcheck:
	$(CPPCHECK) --enable=all \
	--std=c11 \
	--suppress=missingIncludeSystem \
	-I include \
	src/

# ----------------------------------------------------------
# MISRA C - Coding standard analysis
# ----------------------------------------------------------

misra:
	$(CPPCHECK) --enable=warning,style,performance,portability \
	--addon=$(MISRA_ADDON) \
	--std=c11 \
	-I include \
	src/

# ----------------------------------------------------------
# Valgrind - Memory analysis
# ----------------------------------------------------------

valgrind: $(TARGET)
	valgrind --tool=memcheck \
	--leak-check=full \
	--show-leak-kinds=all \
	--track-origins=yes \
	--error-exitcode=1 \
	./$(TARGET)

# ----------------------------------------------------------
# Helgrind - Thread analysis
# ----------------------------------------------------------

helgrind: $(TARGET)
	valgrind --tool=helgrind \
	--history-level=full \
	--error-exitcode=1 \
	./$(TARGET)

# ----------------------------------------------------------
# Google Test - Compile C modules
# ----------------------------------------------------------

build/gtest_obj/%.o: src/%.c
	@mkdir -p build/gtest_obj
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------------------------------------
# Google Test - Build test executable
# ----------------------------------------------------------

gtest: $(GTEST_OBJ)
	$(CXX) $(CXXFLAGS) \
	$(GTEST_SRC) \
	$(GTEST_OBJ) \
	$(GTEST_LIBS) \
	-pthread \
	-o $(GTEST_TARGET)

# ----------------------------------------------------------
# Google Test - Run tests
# ----------------------------------------------------------

gtest-run: gtest
	./$(GTEST_TARGET)

test: gtest-run

# ----------------------------------------------------------
# GCOV - Coverage build
# ----------------------------------------------------------

COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage

COVERAGE_OBJ = $(patsubst src/%.c,build/coverage/%.o,$(SRC))

build/coverage/%.o: src/%.c
	@mkdir -p build/coverage
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

build/metro_coverage: $(COVERAGE_OBJ)
	$(CC) $(COVERAGE_OBJ) $(LDFLAGS) \
	$(COVERAGE_FLAGS) -o $@

gcov: build/metro_coverage
	@echo "Coverage executable created."
	@echo "Run it using: ./build/metro_coverage"
	@echo "After exiting, execute: make gcov-report"

gcov-report:
	gcov -o build/coverage $(SRC)

# ----------------------------------------------------------
# Clean
# ----------------------------------------------------------

clean:
	rm -rf build/*.o
	rm -rf build/gtest_obj
	rm -rf build/coverage
	rm -f $(TARGET)
	rm -f $(GTEST_TARGET)
	rm -f build/metro_coverage
	rm -f *.gcda *.gcno *.gcov

# ----------------------------------------------------------
# Rebuild
# ----------------------------------------------------------

rebuild: clean all

.PHONY: all run cppcheck misra valgrind helgrind \
        gtest gtest-run test gcov gcov-report clean rebuild
