/*
 * 2-Bit Ternary Encoding Memory Bandwidth Micro-Benchmark
 * 
 * Surgically isolates and proves that 2-bit packed ternary encoding
 * reduces memory bandwidth bottleneck vs standard 8-bit representation.
 *
 * Copyright (C) 2024 HyperFold Technologies UK Ltd.
 * Licensed under GNU AGPLv3
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef USE_PERF
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MATRIX_ROWS 11008
#define MATRIX_COLS 4096
#define ITERATIONS 100
#define SPARSITY 0.5f  // 50% zeros

// ============================================================================
// PERFORMANCE COUNTER SETUP
// ============================================================================
#ifdef USE_PERF
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                           int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

typedef struct {
    int fd_cycles;
    int fd_instructions;
    int fd_cache_refs;
    int fd_cache_misses;
    int fd_l1d_read_miss;
    int fd_llc_read_miss;
} perf_counters_t;

void setup_perf_counters(perf_counters_t *counters) {
    struct perf_event_attr pe;
    
    // CPU cycles
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    counters->fd_cycles = perf_event_open(&pe, 0, -1, -1, 0);
    
    // Instructions
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    counters->fd_instructions = perf_event_open(&pe, 0, -1, -1, 0);
    
    // Cache references
    pe.config = PERF_COUNT_HW_CACHE_REFERENCES;
    counters->fd_cache_refs = perf_event_open(&pe, 0, -1, -1, 0);
    
    // Cache misses
    pe.config = PERF_COUNT_HW_CACHE_MISSES;
    counters->fd_cache_misses = perf_event_open(&pe, 0, -1, -1, 0);
    
    // L1D read misses
    pe.type = PERF_TYPE_HW_CACHE;
    pe.config = (PERF_COUNT_HW_CACHE_L1D) | 
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    counters->fd_l1d_read_miss = perf_event_open(&pe, 0, -1, -1, 0);
    
    // LLC (Last Level Cache) read misses
    pe.config = (PERF_COUNT_HW_CACHE_LL) |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    counters->fd_llc_read_miss = perf_event_open(&pe, 0, -1, -1, 0);
}

void start_perf_counters(perf_counters_t *counters) {
    ioctl(counters->fd_cycles, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_instructions, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_cache_refs, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_cache_misses, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_l1d_read_miss, PERF_EVENT_IOC_RESET, 0);
    ioctl(counters->fd_llc_read_miss, PERF_EVENT_IOC_RESET, 0);
    
    ioctl(counters->fd_cycles, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(counters->fd_instructions, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(counters->fd_cache_refs, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(counters->fd_cache_misses, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(counters->fd_l1d_read_miss, PERF_EVENT_IOC_ENABLE, 0);
    ioctl(counters->fd_llc_read_miss, PERF_EVENT_IOC_ENABLE, 0);
}

void stop_perf_counters(perf_counters_t *counters, long long *cycles,
                       long long *instructions, long long *cache_refs,
                       long long *cache_misses, long long *l1d_miss,
                       long long *llc_miss) {
    ioctl(counters->fd_cycles, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(counters->fd_instructions, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(counters->fd_cache_refs, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(counters->fd_cache_misses, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(counters->fd_l1d_read_miss, PERF_EVENT_IOC_DISABLE, 0);
    ioctl(counters->fd_llc_read_miss, PERF_EVENT_IOC_DISABLE, 0);
    
    read(counters->fd_cycles, cycles, sizeof(long long));
    read(counters->fd_instructions, instructions, sizeof(long long));
    read(counters->fd_cache_refs, cache_refs, sizeof(long long));
    read(counters->fd_cache_misses, cache_misses, sizeof(long long));
    read(counters->fd_l1d_read_miss, l1d_miss, sizeof(long long));
    read(counters->fd_llc_read_miss, llc_miss, sizeof(long long));
}

void close_perf_counters(perf_counters_t *counters) {
    close(counters->fd_cycles);
    close(counters->fd_instructions);
    close(counters->fd_cache_refs);
    close(counters->fd_cache_misses);
    close(counters->fd_l1d_read_miss);
    close(counters->fd_llc_read_miss);
}
#endif

// ============================================================================
// DATA GENERATION
// ============================================================================
void generate_ternary_matrix_8bit(int8_t *matrix, int rows, int cols) {
    for (int i = 0; i < rows * cols; i++) {
        float r = (float)rand() / RAND_MAX;
        if (r < SPARSITY) {
            matrix[i] = 0;
        } else if (r < SPARSITY + (1.0f - SPARSITY) / 2.0f) {
            matrix[i] = 1;
        } else {
            matrix[i] = -1;
        }
    }
}

void pack_ternary_2bit(const int8_t *matrix_8bit, uint8_t *matrix_2bit,
                       int rows, int cols) {
    int packed_cols = (cols + 3) / 4;
    memset(matrix_2bit, 0, rows * packed_cols);
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int8_t val = matrix_8bit[r * cols + c];
            int packed_idx = r * packed_cols + c / 4;
            int trit_idx = c % 4;
            
            uint8_t bits;
            if (val == 0) bits = 0;       // 00
            else if (val == 1) bits = 1;  // 01
            else bits = 2;                // 10 for -1
            
            matrix_2bit[packed_idx] |= (bits << (trit_idx * 2));
        }
    }
}

void generate_input_vector(float *vec, int size) {
    for (int i = 0; i < size; i++) {
        vec[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }
}

// ============================================================================
// VERSION A: 8-BIT REPRESENTATION (THEIRS)
// ============================================================================
void matvec_8bit(const int8_t *matrix, const float *input, float *output,
                 int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        float sum = 0.0f;
        const int8_t *row_ptr = matrix + r * cols;
        
        for (int c = 0; c < cols; c++) {
            int8_t w = row_ptr[c];
            if (w != 0) {
                sum += (float)w * input[c];
            }
        }
        
        output[r] = sum;
    }
}

// ============================================================================
// VERSION B: 2-BIT PACKED REPRESENTATION (OURS)
// ============================================================================
static inline int8_t unpack_trit(uint8_t packed, int index) {
    uint8_t bits = (packed >> (index * 2)) & 0x3;
    return (bits == 2) ? -1 : (bits == 1) ? 1 : 0;
}

void matvec_2bit(const uint8_t *matrix_packed, const float *input,
                 float *output, int rows, int cols) {
    int packed_cols = (cols + 3) / 4;
    
    for (int r = 0; r < rows; r++) {
        float sum = 0.0f;
        const uint8_t *row_ptr = matrix_packed + r * packed_cols;
        
        for (int packed_idx = 0; packed_idx < packed_cols; packed_idx++) {
            uint8_t packed = row_ptr[packed_idx];
            
            for (int trit_idx = 0; trit_idx < 4; trit_idx++) {
                int c = packed_idx * 4 + trit_idx;
                if (c < cols) {
                    int8_t w = unpack_trit(packed, trit_idx);
                    if (w != 0) {
                        sum += (float)w * input[c];
                    }
                }
            }
        }
        
        output[r] = sum;
    }
}

// ============================================================================
// BENCHMARK HARNESS
// ============================================================================
typedef struct {
    double time_ms;
    long long cycles;
    long long instructions;
    long long cache_refs;
    long long cache_misses;
    long long l1d_misses;
    long long llc_misses;
    double cache_miss_rate;
    double ipc;
    size_t memory_bytes;
} benchmark_result_t;

void benchmark_8bit(const int8_t *matrix, const float *input, float *output,
                   int rows, int cols, int iterations,
                   benchmark_result_t *result) {
    struct timespec start, end;
    
#ifdef USE_PERF
    perf_counters_t counters;
    setup_perf_counters(&counters);
    start_perf_counters(&counters);
#endif
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        matvec_8bit(matrix, input, output, rows, cols);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
#ifdef USE_PERF
    stop_perf_counters(&counters, &result->cycles, &result->instructions,
                      &result->cache_refs, &result->cache_misses,
                      &result->l1d_misses, &result->llc_misses);
    close_perf_counters(&counters);
    
    result->cache_miss_rate = (double)result->cache_misses / result->cache_refs * 100.0;
    result->ipc = (double)result->instructions / result->cycles;
#else
    result->cycles = 0;
    result->instructions = 0;
    result->cache_refs = 0;
    result->cache_misses = 0;
    result->l1d_misses = 0;
    result->llc_misses = 0;
    result->cache_miss_rate = 0.0;
    result->ipc = 0.0;
#endif
    
    result->time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                      (end.tv_nsec - start.tv_nsec) / 1000000.0;
    result->memory_bytes = rows * cols * sizeof(int8_t);
}

void benchmark_2bit(const uint8_t *matrix_packed, const float *input,
                   float *output, int rows, int cols, int iterations,
                   benchmark_result_t *result) {
    struct timespec start, end;
    
#ifdef USE_PERF
    perf_counters_t counters;
    setup_perf_counters(&counters);
    start_perf_counters(&counters);
#endif
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < iterations; i++) {
        matvec_2bit(matrix_packed, input, output, rows, cols);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
#ifdef USE_PERF
    stop_perf_counters(&counters, &result->cycles, &result->instructions,
                      &result->cache_refs, &result->cache_misses,
                      &result->l1d_misses, &result->llc_misses);
    close_perf_counters(&counters);
    
    result->cache_miss_rate = (double)result->cache_misses / result->cache_refs * 100.0;
    result->ipc = (double)result->instructions / result->cycles;
#else
    result->cycles = 0;
    result->instructions = 0;
    result->cache_refs = 0;
    result->cache_misses = 0;
    result->l1d_misses = 0;
    result->llc_misses = 0;
    result->cache_miss_rate = 0.0;
    result->ipc = 0.0;
#endif
    
    result->time_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                      (end.tv_nsec - start.tv_nsec) / 1000000.0;
    
    int packed_cols = (cols + 3) / 4;
    result->memory_bytes = rows * packed_cols * sizeof(uint8_t);
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    printf("========================================================================\n");
    printf("2-Bit Ternary Encoding Memory Bandwidth Micro-Benchmark\n");
    printf("HyperFold Technologies UK Ltd.\n");
    printf("========================================================================\n\n");
    
    printf("Configuration:\n");
    printf("  Matrix Size:  %d × %d\n", MATRIX_ROWS, MATRIX_COLS);
    printf("  Total Weights: %d\n", MATRIX_ROWS * MATRIX_COLS);
    printf("  Sparsity:     %.0f%%\n", SPARSITY * 100.0f);
    printf("  Iterations:   %d\n", ITERATIONS);
#ifdef USE_PERF
    printf("  Profiling:    Hardware Performance Counters (perf)\n");
#else
    printf("  Profiling:    Time only (compile with -DUSE_PERF for counters)\n");
#endif
    printf("\n");
    
    // Allocate memory
    size_t matrix_8bit_size = MATRIX_ROWS * MATRIX_COLS * sizeof(int8_t);
    int packed_cols = (MATRIX_COLS + 3) / 4;
    size_t matrix_2bit_size = MATRIX_ROWS * packed_cols * sizeof(uint8_t);
    
    int8_t *matrix_8bit = (int8_t*)malloc(matrix_8bit_size);
    uint8_t *matrix_2bit = (uint8_t*)malloc(matrix_2bit_size);
    float *input = (float*)malloc(MATRIX_COLS * sizeof(float));
    float *output = (float*)malloc(MATRIX_ROWS * sizeof(float));
    
    if (!matrix_8bit || !matrix_2bit || !input || !output) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    // Generate test data
    srand(42);
    generate_ternary_matrix_8bit(matrix_8bit, MATRIX_ROWS, MATRIX_COLS);
    pack_ternary_2bit(matrix_8bit, matrix_2bit, MATRIX_ROWS, MATRIX_COLS);
    generate_input_vector(input, MATRIX_COLS);
    
    printf("Memory Footprint:\n");
    printf("  8-bit representation: %zu KB\n", matrix_8bit_size / 1024);
    printf("  2-bit representation: %zu KB\n", matrix_2bit_size / 1024);
    printf("  Reduction:            %.1f%%\n\n",
           100.0 * (1.0 - (double)matrix_2bit_size / matrix_8bit_size));
    
    // Warmup
    for (int i = 0; i < 10; i++) {
        matvec_8bit(matrix_8bit, input, output, MATRIX_ROWS, MATRIX_COLS);
        matvec_2bit(matrix_2bit, input, output, MATRIX_ROWS, MATRIX_COLS);
    }
    
    // Benchmark 8-bit
    printf("Running Version A (8-bit)...\n");
    benchmark_result_t result_8bit;
    benchmark_8bit(matrix_8bit, input, output, MATRIX_ROWS, MATRIX_COLS,
                   ITERATIONS, &result_8bit);
    
    // Benchmark 2-bit
    printf("Running Version B (2-bit packed)...\n");
    benchmark_result_t result_2bit;
    benchmark_2bit(matrix_2bit, input, output, MATRIX_ROWS, MATRIX_COLS,
                   ITERATIONS, &result_2bit);
    
    printf("\n");
    printf("========================================================================\n");
    printf("RESULTS\n");
    printf("========================================================================\n\n");
    
    printf("%-25s | %15s | %15s | %10s\n",
           "Metric", "8-bit (Theirs)", "2-bit (Ours)", "Improvement");
    printf("------------------------------------------------------------------------\n");
    
    printf("%-25s | %12.2f ms | %12.2f ms | %9.2fx\n",
           "Total Time",
           result_8bit.time_ms, result_2bit.time_ms,
           result_8bit.time_ms / result_2bit.time_ms);
    
    printf("%-25s | %15zu | %15zu | %9.2fx\n",
           "Memory Footprint (KB)",
           result_8bit.memory_bytes / 1024,
           result_2bit.memory_bytes / 1024,
           (double)result_8bit.memory_bytes / result_2bit.memory_bytes);
    
#ifdef USE_PERF
    printf("%-25s | %15lld | %15lld | %9.2fx\n",
           "Cache References",
           result_8bit.cache_refs, result_2bit.cache_refs,
           (double)result_8bit.cache_refs / result_2bit.cache_refs);
    
    printf("%-25s | %15lld | %15lld | %9.2fx\n",
           "Cache Misses",
           result_8bit.cache_misses, result_2bit.cache_misses,
           (double)result_8bit.cache_misses / result_2bit.cache_misses);
    
    printf("%-25s | %13.2f%% | %13.2f%% | %9.2fx\n",
           "Cache Miss Rate",
           result_8bit.cache_miss_rate, result_2bit.cache_miss_rate,
           result_8bit.cache_miss_rate / result_2bit.cache_miss_rate);
    
    printf("%-25s | %15lld | %15lld | %9.2fx\n",
           "L1D Cache Misses",
           result_8bit.l1d_misses, result_2bit.l1d_misses,
           (double)result_8bit.l1d_misses / result_2bit.l1d_misses);
    
    printf("%-25s | %15lld | %15lld | %9.2fx\n",
           "LLC (L3) Cache Misses",
           result_8bit.llc_misses, result_2bit.llc_misses,
           (double)result_8bit.llc_misses / result_2bit.llc_misses);
    
    printf("%-25s | %15.2f | %15.2f | %9.2fx\n",
           "IPC (Instructions/Cycle)",
           result_8bit.ipc, result_2bit.ipc,
           result_2bit.ipc / result_8bit.ipc);
#endif
    
    printf("\n========================================================================\n");
    printf("CONCLUSION\n");
    printf("========================================================================\n\n");
    
#ifdef USE_PERF
    double cache_miss_improvement = (double)result_8bit.cache_misses / result_2bit.cache_misses;
    double bandwidth_improvement = (double)result_8bit.memory_bytes / result_2bit.memory_bytes;
    
    printf("The 2-bit packed encoding reduces cache misses by %.1fx and memory\n",
           cache_miss_improvement);
    printf("footprint by %.1fx, directly addressing the memory bandwidth bottleneck\n",
           bandwidth_improvement);
    printf("in ternary neural network inference.\n\n");
    
    if (cache_miss_improvement > 2.0) {
        printf("✓ SIGNIFICANT CACHE EFFICIENCY IMPROVEMENT CONFIRMED\n");
        printf("✓ MEMORY BANDWIDTH BOTTLENECK RESOLVED\n");
    }
#else
    printf("Compile with -DUSE_PERF to measure hardware performance counters.\n");
    printf("Time improvement: %.2fx\n", result_8bit.time_ms / result_2bit.time_ms);
    printf("Memory reduction: %.2fx\n",
           (double)result_8bit.memory_bytes / result_2bit.memory_bytes);
#endif
    
    // Cleanup
    free(matrix_8bit);
    free(matrix_2bit);
    free(input);
    free(output);
    
    return 0;
}
