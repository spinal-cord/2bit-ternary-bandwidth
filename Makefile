# Makefile for 2-bit Ternary Bandwidth Micro-Benchmark
# Copyright (C) 2024 HyperFold Technologies UK Ltd.

CC = gcc
CFLAGS = -O3 -Wall -Wextra -march=native
LDFLAGS = -lrt

TARGET = benchmark
SOURCE = benchmark.c

# Default target: build without perf counters
all: $(TARGET)

# Build with hardware performance counters (requires Linux perf)
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
	./$(TARGET)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Help
help:
	@echo "2-bit Ternary Bandwidth Micro-Benchmark"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build benchmark (time measurement only)"
	@echo "  make perf     - Build with hardware performance counters"
	@echo "  make run      - Build and run benchmark"
	@echo "  make run-perf - Build with perf and run"
	@echo "  make clean    - Remove build artifacts"
	@echo ""
	@echo "Performance Counter Setup (Linux):"
	@echo "  sudo sysctl kernel.perf_event_paranoid=-1"
	@echo ""
	@echo "Or run as root:"
	@echo "  sudo ./benchmark"

.PHONY: all perf run run-perf clean help
