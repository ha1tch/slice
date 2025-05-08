#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

// Calculate optimal chunk size based on file size
size_t calculate_chunk_size(off_t file_size) {
    // Start with base chunk size
    size_t chunk_size = BASE_CHUNK_SIZE;
    
    // For each power of 10 increase in file size, double the chunk size
    for (off_t threshold = 100 * 1024; // 100 KB threshold
         threshold < file_size && chunk_size < MAX_CHUNK_SIZE; 
         threshold *= 10) {
        chunk_size *= 2;
    }
    
    // Ensure we don't exceed the maximum chunk size
    if (chunk_size > MAX_CHUNK_SIZE) {
        chunk_size = MAX_CHUNK_SIZE;
    }
    
    return chunk_size;
}

// Calculate next power of 2 for optimal chunk size
size_t next_power_of_2(size_t n) {
    size_t power_of_2 = 1;
    while (power_of_2 < n)
        power_of_2 *= 2;
    return power_of_2;
}

// Safe addition checking for overflow
int safe_add(size_t a, size_t b, size_t *result) {
    if (SIZE_MAX - a < b) {
        return -1; // Overflow
    }
    *result = a + b;
    return 0;
}

// Free an array of strings
void free_string_array(char **array, int count) {
    if (array) {
        for (int i = 0; i < count; i++) {
            free(array[i]);
        }
        free(array);
    }
}