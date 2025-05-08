#define _FILE_OFFSET_BITS 64
#include "linestats.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void init_stats(LineStats *stats) {
    memset(stats, 0, sizeof(LineStats));
}

int update_histogram(LineStats *stats, size_t line_length) {
    int bucket = 0;
    size_t threshold = 64;  // Starting threshold
    
    while (bucket < MAX_HISTOGRAM_BUCKETS - 1 && line_length > threshold) {
        bucket++;
        threshold *= 2;
    }
    
    stats->histogram[bucket]++;
    return bucket;
}


int analyze_file(const char *filename, LineStats *stats, int debug) {
    int fd = -1;
    char *buffer = NULL;
    char *overflow_buffer = NULL;  // Buffer for lines that span chunks
    size_t overflow_size = 0;      // Size of data in overflow buffer
    size_t overflow_capacity = 0;  // Capacity of overflow buffer
    int exit_code = 0;
    size_t total_line_length = 0;
    
    // Open file using low-level I/O
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", filename, strerror(errno));
        return 1;
    }

    // Get file size for adaptive buffer sizing
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: not a regular file: %s\n", filename);
        exit_code = 1;
        goto cleanup;
    }

    off_t file_size = st.st_size;
    
    // Calculate optimal chunk size based on file size
    size_t chunk_size = calculate_chunk_size(file_size);
    
    if (debug) {
        fprintf(stderr, "[DEBUG] File size: %lld bytes\n", (long long)file_size);
        fprintf(stderr, "[DEBUG] Calculated chunk size: %zu bytes\n", chunk_size);
    }

    // Allocate buffer for chunks
    buffer = malloc(chunk_size);
    if (!buffer) {
        fprintf(stderr, "Error: memory allocation failed for buffer of size %zu\n", chunk_size);
        exit_code = 1;
        goto cleanup;
    }
    
    // Initialize overflow buffer with a reasonable size
    overflow_capacity = 1024;  // Start with 1KB
    overflow_buffer = malloc(overflow_capacity);
    if (!overflow_buffer) {
        fprintf(stderr, "Error: memory allocation failed for overflow buffer\n");
        exit_code = 1;
        goto cleanup;
    }
    
    off_t total_bytes = 0;           // Total bytes processed
    size_t current_line_length = 0;   // Length of current line
    off_t current_line_start = 0;     // Start position of current line
    int line_in_overflow = 0;         // Flag to indicate if current line is in overflow
    
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, chunk_size)) > 0) {
        // Process the chunk
        for (ssize_t i = 0; i < bytes_read; i++) {
            // Increment line length safely
            if (SIZE_MAX - current_line_length < 1) {
                fprintf(stderr, "Error: Line length overflow detected\n");
                exit_code = 1;
                goto cleanup;
            }
            current_line_length++;
            
            // Check if this is a newline character
            if (buffer[i] == '\n') {
                // Line complete
                stats->total_lines++;
                
                // Safe addition for total_line_length
                size_t new_total;
                if (safe_add(total_line_length, current_line_length, &new_total) != 0) {
                    fprintf(stderr, "Error: Total line length overflow\n");
                    exit_code = 1;
                    goto cleanup;
                }
                total_line_length = new_total;
                
                // Track long lines
                if (current_line_length > 1024)
                    stats->lines_over_1k++;
                if (current_line_length > 10 * 1024)
                    stats->lines_over_10k++;
                if (current_line_length > 100 * 1024)
                    stats->lines_over_100k++;
                if (current_line_length > 1024 * 1024)
                    stats->lines_over_1m++;
                
                // Update histogram
                update_histogram(stats, current_line_length);
                
                // Check if this is the longest line
                if (current_line_length > stats->max_line_length) {
                    stats->max_line_length = current_line_length;
                    stats->max_line_position = current_line_start;
                }
                
                // Reset for next line
                current_line_length = 0;
                current_line_start = total_bytes + i + 1;
                
                // If we were using overflow buffer, reset it
                if (line_in_overflow) {
                    overflow_size = 0;
                    line_in_overflow = 0;
                }
            } else if (i == bytes_read - 1) {
                // End of chunk but not end of line - handle line continuation
                
                // Move any partial line to overflow buffer
                if (!line_in_overflow) {
                    // This is a new line spanning chunks - move it to overflow
                    line_in_overflow = 1;
                    
                    // Ensure overflow buffer has enough capacity
                    if (overflow_capacity < i + 1) {
                        size_t new_capacity = next_power_of_2(i + 1);
                        char *new_buffer = realloc(overflow_buffer, new_capacity);
                        if (!new_buffer) {
                            fprintf(stderr, "Error: memory allocation failed for overflow buffer resize\n");
                            exit_code = 1;
                            goto cleanup;
                        }
                        overflow_buffer = new_buffer;
                        overflow_capacity = new_capacity;
                    }
                    
                    // Copy from the line start to the end of chunk
                    size_t line_start_in_chunk = 0;
                    for (ssize_t j = 0; j < i; j++) {
                        if (buffer[j] == '\n') {
                            line_start_in_chunk = j + 1;
                        }
                    }
                    
                    size_t bytes_to_copy = i + 1 - line_start_in_chunk;
                    memcpy(overflow_buffer, buffer + line_start_in_chunk, bytes_to_copy);
                    overflow_size = bytes_to_copy;
                } else {
                    // Already have content in overflow, append this chunk
                    size_t new_size = overflow_size + i + 1;
                    
                    // Ensure overflow buffer has enough capacity
                    if (overflow_capacity < new_size) {
                        size_t new_capacity = next_power_of_2(new_size);
                        char *new_buffer = realloc(overflow_buffer, new_capacity);
                        if (!new_buffer) {
                            fprintf(stderr, "Error: memory allocation failed for overflow buffer resize\n");
                            exit_code = 1;
                            goto cleanup;
                        }
                        overflow_buffer = new_buffer;
                        overflow_capacity = new_capacity;
                    }
                    
                    // Append this chunk to overflow
                    memcpy(overflow_buffer + overflow_size, buffer, i + 1);
                    overflow_size = new_size;
                }
            }
        }
        
        // Update total bytes read
        total_bytes += bytes_read;
    }
    
    // Handle last line if not terminated with newline
    if (current_line_length > 0 || overflow_size > 0) {
        // Determine final line length
        size_t final_line_length = line_in_overflow ? overflow_size : current_line_length;
        
        stats->total_lines++;
        
        // Safe addition for total_line_length
        size_t new_total;
        if (safe_add(total_line_length, final_line_length, &new_total) != 0) {
            fprintf(stderr, "Error: Total line length overflow\n");
            exit_code = 1;
            goto cleanup;
        }
        total_line_length = new_total;
        
        // Track long lines
        if (final_line_length > 1024)
            stats->lines_over_1k++;
        if (final_line_length > 10 * 1024)
            stats->lines_over_10k++;
        if (final_line_length > 100 * 1024)
            stats->lines_over_100k++;
        if (final_line_length > 1024 * 1024)
            stats->lines_over_1m++;
        
        // Update histogram
        update_histogram(stats, final_line_length);
        
        // Check if this is the longest line
        if (final_line_length > stats->max_line_length) {
            stats->max_line_length = final_line_length;
            stats->max_line_position = current_line_start;
        }
    }
    
    // Calculate average line length
    if (stats->total_lines > 0) {
        stats->avg_line_length = total_line_length / stats->total_lines;
    }
    
cleanup:
    if (buffer) free(buffer);
    if (overflow_buffer) free(overflow_buffer);
    if (fd >= 0) close(fd);
    return exit_code;
}