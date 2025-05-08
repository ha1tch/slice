#ifndef LINESTATS_H
#define LINESTATS_H

#include <stddef.h>
#include <sys/types.h>
#include "utils.h"

// Struct to hold line statistics for a single file
typedef struct {
    size_t total_lines;          // Total number of lines in file
    size_t max_line_length;      // Length of longest line
    off_t max_line_position;     // Byte position of longest line start (using off_t for large files)
    size_t avg_line_length;      // Average line length
    size_t lines_over_1k;        // Lines over 1KB
    size_t lines_over_10k;       // Lines over 10KB
    size_t lines_over_100k;      // Lines over 100KB
    size_t lines_over_1m;        // Lines over 1MB
    size_t histogram[MAX_HISTOGRAM_BUCKETS]; // Line length histogram
} LineStats;

// Initialize the line statistics structure
void init_stats(LineStats *stats);

// Update the histogram for a line of a specific length
int update_histogram(LineStats *stats, size_t line_length);

// Analyze a file and collect line statistics
int analyze_file(const char *filename, LineStats *stats, int debug);

#endif /* LINESTATS_H */