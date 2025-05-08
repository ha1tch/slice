#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>

// Maximum number of histogram buckets for line length analysis
#define MAX_HISTOGRAM_BUCKETS 10

// Maximum number of files to process
#define MAX_FILES 10000

// Base chunk size for file reading (16 KB)
#define BASE_CHUNK_SIZE (16 * 1024)

// Maximum chunk size for file reading (16 MB)
#define MAX_CHUNK_SIZE (16 * 1024 * 1024)

// Calculate optimal chunk size based on file size
size_t calculate_chunk_size(off_t file_size);

// Calculate next power of 2 for optimal chunk size
size_t next_power_of_2(size_t n);

// Safe addition checking for overflow
int safe_add(size_t a, size_t b, size_t *result);

// Free an array of strings
void free_string_array(char **array, int count);

#endif /* UTILS_H */