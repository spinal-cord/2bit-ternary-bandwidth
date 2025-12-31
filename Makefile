# Makefile for 2-bit Ternary Bandwidth Micro-Benchmark
# Copyright (C) 2024 HyperFold Technologies UK Ltd.

CC = gcc
CFLAGS = -O3 -Wall -Wextra
LDFLAGS = 

# Detect macOS and adjust flags
UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
    # macOS - no librt needed, don't use -march=native
    CFLAGS += -mmacosx-version-min=10.12
else ifeq ($(UNAME), Linux)
    # Linux - use librt and native optimizations
    CFLAGS += -march=native
    LDFLAGS += -lrt
else
    # Other Unix-like systems
    LDFLAGS += -lrt
endif

TARGET = benchmark
SOURCE = benchmark.c

# Default target: build without perf counters
all: $(TARGET)

# Build with hardware performance counters (requires Linux perf)
# Note: perf counters only work on Linux
perf: CFLAGS += -DUSE_PERF
perf: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

# Run the benchmark
run: $(TARGET)
	./$(TARGET)

# Run with perf counters (requires sudo or perf_event_paranoid=0)
run-perf: perf
	@echo "Note: May require: sudo sysctl kernel.perf_event_paranoid=-1"
	@./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Help
help:
	@echo "2-bit Ternary Bandwidth Micro-Benchmark"
	@echo "Detected OS: $(UNAME)"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build benchmark (time measurement only)"
	@echo "  make perf     - Build with hardware performance counters (Linux only)"
	@echo "  make run      - Build and run benchmark"
	@echo "  make run-perf - Build with perf and run (Linux only)"
	@echo "  make clean    - Remove build artifacts"
	@echo ""
ifeq ($(UNAME), Linux)
	@echo "Performance Counter Setup (Linux):"
	@echo "  sudo sysctl kernel.perf_event_paranoid=-1"
	@echo ""
	@echo "Or run as root:"
	@echo "  sudo ./benchmark"
else
	@echo "Note: Hardware performance counters only available on Linux."
	@echo "      On macOS, only timing measurements will be available."
endif

.PHONY: all perf run run-perf clean help