#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>

#define BASE_CHUNK_SIZE 8192  // Starting chunk size (8 KB)
#define MAX_CHUNK_SIZE (100 * 1024 * 1024)  // Max chunk size (100 MB)
#define MAX_ALLOC_SIZE (1UL << 30)  // 1 GiB maximum allocation
#define MAX_HISTOGRAM_BUCKETS 10    // Number of histogram buckets
#define MAX_FILES 10000              // Maximum number of files in corpus mode

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

// Struct to hold aggregate statistics for a corpus of files
typedef struct {
    size_t file_count;             // Number of files processed
    size_t total_lines_analyzed;   // Sum of all lines across files
    size_t max_line_across_corpus; // Length of longest line in corpus
    off_t max_line_file_index;     // Index of file with longest line
    char *max_line_filename;       // Name of file with longest line
    double avg_line_length_corpus; // Average line length across corpus
    size_t files_with_long_lines;  // Files with lines >100KB
    size_t files_with_very_long_lines;  // Files with lines >1MB
} CorpusStats;

// Output format enum
typedef enum {
    OUTPUT_TEXT,
    OUTPUT_JSON
} OutputFormat;

// Mode enum
typedef enum {
    MODE_SINGLE_FILE,
    MODE_CORPUS_ANALYSIS
} OperationMode;

// Calculate optimal chunk size based on file size (copied from slice3.c)
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

void init_stats(LineStats *stats) {
    memset(stats, 0, sizeof(LineStats));
}

void init_corpus_stats(CorpusStats *stats) {
    stats->file_count = 0;
    stats->total_lines_analyzed = 0;
    stats->max_line_across_corpus = 0;
    stats->max_line_file_index = -1;
    stats->max_line_filename = NULL;
    stats->avg_line_length_corpus = 0;
    stats->files_with_long_lines = 0;
    stats->files_with_very_long_lines = 0;
}

void print_stats_text(const char *filename, LineStats *stats, int debug) {
    printf("LINE LENGTH ANALYSIS FOR: %s\n", filename);
    printf("----------------------------------------\n");
    printf("Total lines:                 %zu\n", stats->total_lines);
    printf("Longest line:                %zu bytes\n", stats->max_line_length);
    printf("Position of longest line:    byte offset %lld\n", (long long)stats->max_line_position);
    
    if (stats->total_lines > 0) {
        printf("Average line length:         %zu bytes\n", stats->avg_line_length);
    }
    
    printf("\nLINE LENGTH DISTRIBUTION:\n");
    printf("----------------------------------------\n");
    printf("Lines over 1KB:              %zu\n", stats->lines_over_1k);
    printf("Lines over 10KB:             %zu\n", stats->lines_over_10k);
    printf("Lines over 100KB:            %zu\n", stats->lines_over_100k);
    printf("Lines over 1MB:              %zu\n", stats->lines_over_1m);
    
    printf("\nHISTOGRAM OF LINE LENGTHS:\n");
    printf("----------------------------------------\n");
    
    const char *bucket_labels[] = {
        "0-64 bytes", "65-128 bytes", "129-256 bytes", "257-512 bytes", 
        "513-1KB", "1KB-2KB", "2KB-4KB", "4KB-8KB", "8KB-16KB", "16KB+"
    };
    
    for (int i = 0; i < MAX_HISTOGRAM_BUCKETS; i++) {
        printf("%-20s %zu lines (%.1f%%)\n", 
               bucket_labels[i], 
               stats->histogram[i],
               stats->total_lines > 0 ? (stats->histogram[i] * 100.0 / stats->total_lines) : 0);
    }
    
    // Calculate next power of 2 for optimal chunk size
    size_t optimal_chunk_size = next_power_of_2(stats->max_line_length);
    
    if (stats->max_line_length > 0) {
        printf("\nRECOMMENDATIONS:\n");
        printf("----------------------------------------\n");
        
        // Add optimal chunk size recommendation
        printf("Optimal minimum chunk size: %zu bytes (%.2f KB)\n", 
               optimal_chunk_size, optimal_chunk_size / 1024.0);
               
        if (stats->max_line_length > 1024 * 1024) {
            printf("\n! WARNING: File contains extremely long lines (>1MB)\n");
            printf("  - Using '--full-lines-only' may result in empty output for some slices\n");
            printf("  - Consider processing this file without '--full-lines-only'\n");
        } else if (stats->lines_over_100k > 0) {
            printf("\n! CAUTION: File contains very long lines (>100KB)\n");
            printf("  - Be aware that '--full-lines-only' may filter out content\n");
        }
        
        printf("\nSample commands:\n");
        // Command to extract longest line
        printf("  # Extract the longest line:\n");
        printf("  slice --start %lld --size %zu --file %s\n\n", 
               (long long)stats->max_line_position, stats->max_line_length, filename);
               
        // Command with optimal chunk size
        printf("  # Process with optimal chunk size:\n");
        printf("  export SLICE_CHUNK_SIZE=%zu  # Set optimal chunk size environment variable\n", optimal_chunk_size);
        printf("  slice --start 0 --size 1048576 --file %s --full-lines-only\n", filename);
    }
    
    if (debug) {
        printf("\nDEBUG INFORMATION:\n");
        printf("----------------------------------------\n");
        printf("sizeof(off_t): %zu bytes\n", sizeof(off_t));
        printf("sizeof(size_t): %zu bytes\n", sizeof(size_t));
    }
}

void print_corpus_stats_text(CorpusStats *stats, int debug) {
    printf("CORPUS LINE LENGTH ANALYSIS\n");
    printf("----------------------------------------\n");
    printf("Files analyzed:             %zu\n", stats->file_count);
    printf("Total lines across corpus:  %zu\n", stats->total_lines_analyzed);
    printf("Longest line in corpus:     %zu bytes\n", stats->max_line_across_corpus);
    printf("Found in file:              %s\n", 
           stats->max_line_filename ? stats->max_line_filename : "unknown");
    
    if (stats->total_lines_analyzed > 0) {
        printf("Average line length:        %.2f bytes\n", stats->avg_line_length_corpus);
    }
    
    printf("\nFILE DISTRIBUTION:\n");
    printf("----------------------------------------\n");
    printf("Files with lines >100KB:    %zu (%.1f%%)\n", 
           stats->files_with_long_lines,
           stats->file_count > 0 ? (stats->files_with_long_lines * 100.0 / stats->file_count) : 0);
    printf("Files with lines >1MB:      %zu (%.1f%%)\n", 
           stats->files_with_very_long_lines,
           stats->file_count > 0 ? (stats->files_with_very_long_lines * 100.0 / stats->file_count) : 0);
    
    // Calculate next power of 2 for optimal chunk size
    size_t optimal_chunk_size = next_power_of_2(stats->max_line_across_corpus);
    
    printf("\nRECOMMENDATIONS:\n");
    printf("----------------------------------------\n");
    printf("Optimal corpus-wide chunk size: %zu bytes (%.2f KB)\n", 
           optimal_chunk_size, optimal_chunk_size / 1024.0);
    
    if (stats->max_line_across_corpus > 1024 * 1024) {
        printf("\n! WARNING: Corpus contains extremely long lines (>1MB)\n");
        printf("  - Using '--full-lines-only' may result in empty output for some slices\n");
        printf("  - Process files with very long lines separately or without '--full-lines-only'\n");
    } else if (stats->files_with_long_lines > 0) {
        printf("\n! CAUTION: %.1f%% of files contain very long lines (>100KB)\n",
               stats->file_count > 0 ? (stats->files_with_long_lines * 100.0 / stats->file_count) : 0);
        printf("  - Be aware that '--full-lines-only' may filter out content\n");
    }
    
    if (debug) {
        printf("\nDEBUG INFORMATION:\n");
        printf("----------------------------------------\n");
        printf("sizeof(off_t): %zu bytes\n", sizeof(off_t));
        printf("sizeof(size_t): %zu bytes\n", sizeof(size_t));
    }
}

void print_stats_json(const char *filename, LineStats *stats, int debug) {
    const char *bucket_labels[] = {
        "0-64 bytes", "65-128 bytes", "129-256 bytes", "257-512 bytes", 
        "513-1KB", "1KB-2KB", "2KB-4KB", "4KB-8KB", "8KB-16KB", "16KB+"
    };
    
    // Calculate next power of 2 for optimal chunk size
    size_t optimal_chunk_size = next_power_of_2(stats->max_line_length);
    
    // Start JSON object
    printf("{\n");
    printf("  \"filename\": \"%s\",\n", filename);
    printf("  \"stats\": {\n");
    printf("    \"total_lines\": %zu,\n", stats->total_lines);
    printf("    \"max_line_length\": %zu,\n", stats->max_line_length);
    printf("    \"max_line_position\": %lld,\n", (long long)stats->max_line_position);
    printf("    \"avg_line_length\": %zu,\n", stats->avg_line_length);
    printf("    \"lines_over_1k\": %zu,\n", stats->lines_over_1k);
    printf("    \"lines_over_10k\": %zu,\n", stats->lines_over_10k);
    printf("    \"lines_over_100k\": %zu,\n", stats->lines_over_100k);
    printf("    \"lines_over_1m\": %zu\n", stats->lines_over_1m);
    printf("  },\n");
    
    // Histogram data
    printf("  \"histogram\": [\n");
    for (int i = 0; i < MAX_HISTOGRAM_BUCKETS; i++) {
        printf("    {\n");
        printf("      \"range\": \"%s\",\n", bucket_labels[i]);
        printf("      \"count\": %zu,\n", stats->histogram[i]);
        printf("      \"percentage\": %.1f\n", 
               stats->total_lines > 0 ? (stats->histogram[i] * 100.0 / stats->total_lines) : 0);
        printf("    }%s\n", (i < MAX_HISTOGRAM_BUCKETS - 1) ? "," : "");
    }
    printf("  ],\n");
    
    // Recommendations
    printf("  \"recommendations\": {\n");
    printf("    \"optimal_chunk_size\": %zu,\n", optimal_chunk_size);
    printf("    \"optimal_chunk_size_kb\": %.2f,\n", optimal_chunk_size / 1024.0);
    
    if (stats->max_line_length > 1024 * 1024) {
        printf("    \"warning\": \"File contains extremely long lines (>1MB)\",\n");
        printf("    \"suggestion\": \"Using '--full-lines-only' may result in empty output for some slices\",\n");
        printf("    \"severity\": \"warning\"\n");
    } else if (stats->lines_over_100k > 0) {
        printf("    \"warning\": \"File contains very long lines (>100KB)\",\n");
        printf("    \"suggestion\": \"Be aware that '--full-lines-only' may filter out content\",\n");
        printf("    \"severity\": \"caution\"\n");
    } else {
        printf("    \"suggestion\": \"No specific warnings for this file\",\n");
        printf("    \"severity\": \"none\"\n");
    }
    printf("  },\n");
    
    // Sample commands
    printf("  \"commands\": {\n");
    printf("    \"extract_longest_line\": \"slice --start %lld --size %zu --file %s\",\n", 
           (long long)stats->max_line_position, stats->max_line_length, filename);
    printf("    \"set_optimal_chunk_size\": \"export SLICE_CHUNK_SIZE=%zu\"\n", 
           optimal_chunk_size);
    printf("  }");
    
    // Debug information (if requested)
    if (debug) {
        printf(",\n  \"debug\": {\n");
        printf("    \"sizeof_off_t\": %zu,\n", sizeof(off_t));
        printf("    \"sizeof_size_t\": %zu\n", sizeof(size_t));
        printf("  }\n");
    } else {
        printf("\n");
    }
    
    // End JSON object
    printf("}\n");
}

void print_corpus_stats_json(CorpusStats *stats, int debug) {
    // Calculate next power of 2 for optimal chunk size
    size_t optimal_chunk_size = next_power_of_2(stats->max_line_across_corpus);
    
    // Start JSON object
    printf("{\n");
    printf("  \"corpus_stats\": {\n");
    printf("    \"files_analyzed\": %zu,\n", stats->file_count);
    printf("    \"total_lines\": %zu,\n", stats->total_lines_analyzed);
    printf("    \"max_line_length\": %zu,\n", stats->max_line_across_corpus);
    printf("    \"max_line_file\": \"%s\",\n", 
           stats->max_line_filename ? stats->max_line_filename : "unknown");
    printf("    \"avg_line_length\": %.2f,\n", stats->avg_line_length_corpus);
    printf("    \"files_with_long_lines\": %zu,\n", stats->files_with_long_lines);
    printf("    \"files_with_very_long_lines\": %zu,\n", stats->files_with_very_long_lines);
    printf("    \"pct_files_with_long_lines\": %.1f,\n", 
           stats->file_count > 0 ? (stats->files_with_long_lines * 100.0 / stats->file_count) : 0);
    printf("    \"pct_files_with_very_long_lines\": %.1f\n", 
           stats->file_count > 0 ? (stats->files_with_very_long_lines * 100.0 / stats->file_count) : 0);
    printf("  },\n");
    
    // Recommendations
    printf("  \"recommendations\": {\n");
    printf("    \"optimal_chunk_size\": %zu,\n", optimal_chunk_size);
    printf("    \"optimal_chunk_size_kb\": %.2f,\n", optimal_chunk_size / 1024.0);
    
    if (stats->max_line_across_corpus > 1024 * 1024) {
        printf("    \"warning\": \"Corpus contains extremely long lines (>1MB)\",\n");
        printf("    \"suggestion\": \"Using '--full-lines-only' may result in empty output for some slices\",\n");
        printf("    \"severity\": \"warning\"\n");
    } else if (stats->files_with_long_lines > 0) {
        printf("    \"warning\": \"%.1f%% of files contain very long lines (>100KB)\",\n",
               stats->file_count > 0 ? (stats->files_with_long_lines * 100.0 / stats->file_count) : 0);
        printf("    \"suggestion\": \"Be aware that '--full-lines-only' may filter out content\",\n");
        printf("    \"severity\": \"caution\"\n");
    } else {
        printf("    \"suggestion\": \"No specific warnings for this corpus\",\n");
        printf("    \"severity\": \"none\"\n");
    }
    printf("  }");
    
    // Debug information (if requested)
    if (debug) {
        printf(",\n  \"debug\": {\n");
        printf("    \"sizeof_off_t\": %zu,\n", sizeof(off_t));
        printf("    \"sizeof_size_t\": %zu\n", sizeof(size_t));
        printf("  }\n");
    } else {
        printf("\n");
    }
    
    // End JSON object
    printf("}\n");
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

// Safe addition checking for overflow
int safe_add(size_t a, size_t b, size_t *result) {
    if (SIZE_MAX - a < b) {
        return -1; // Overflow
    }
    *result = a + b;
    return 0;
}

int analyze_file(const char *filename, LineStats *stats, int debug) {
    int fd = -1;
    char *buffer = NULL;
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
    
    off_t total_bytes = 0;           // Total bytes processed
    size_t current_line_length = 0;   // Length of current line
    off_t current_line_start = 0;     // Start position of current line
    
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, chunk_size)) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            // Safe increment with overflow check
            if (SIZE_MAX - current_line_length < 1) {
                fprintf(stderr, "Error: Line length overflow detected\n");
                exit_code = 1;
                goto cleanup;
            }
            current_line_length++;
            
            if (buffer[i] == '\n') {
                // Line complete - update statistics
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
            }
        }
        
        // Update total bytes read
        total_bytes += bytes_read;
    }
    
    // Handle last line if not terminated with newline
    if (current_line_length > 0) {
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
    }
    
    // Calculate average line length
    if (stats->total_lines > 0) {
        stats->avg_line_length = total_line_length / stats->total_lines;
    }
    
cleanup:
    if (buffer) free(buffer);
    if (fd >= 0) close(fd);
    return exit_code;
}

// Function to check if a file has a specific extension
int has_extension(const char *filename, const char *extension) {
    size_t name_len = strlen(filename);
    size_t ext_len = strlen(extension);
    
    if (name_len <= ext_len) {
        return 0;
    }
    
    return strcmp(filename + name_len - ext_len, extension) == 0;
}

// Function to scan a directory and build a list of files
int scan_directory(const char *dir_path, char ***filenames, int *file_count, 
                  const char *extension, int recursive, int debug) {
    DIR *dir;
    struct dirent *entry;
    int capacity = 100;
    int count = 0;
    char path[PATH_MAX];
    
    // Allocate initial array for filenames
    *filenames = malloc(capacity * sizeof(char*));
    if (!*filenames) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return 1;
    }
    
    // Open directory
    dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory '%s': %s\n", 
                dir_path, strerror(errno));
        free(*filenames);
        *filenames = NULL;
        return 1;
    }
    
    if (debug) {
        fprintf(stderr, "[DEBUG] Scanning directory: %s\n", dir_path);
        if (extension) {
            fprintf(stderr, "[DEBUG] Filtering by extension: %s\n", extension);
        }
    }
    
    // Read directory entries
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Build full path
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        
        // Get file stats
        struct stat st;
        if (stat(path, &st) != 0) {
            if (debug) {
                fprintf(stderr, "[DEBUG] Cannot stat '%s': %s\n", path, strerror(errno));
            }
            continue;
        }
        
        // Handle subdirectories if recursive
        if (S_ISDIR(st.st_mode) && recursive) {
            char **subdir_files = NULL;
            int subdir_count = 0;
            
            if (scan_directory(path, &subdir_files, &subdir_count, extension, recursive, debug) == 0) {
                // Add subdirectory files to our list
                if (count + subdir_count >= capacity) {
                    // Resize array
                    int new_capacity = capacity * 2;
                    if (count + subdir_count > new_capacity) {
                        new_capacity = count + subdir_count + 100;
                    }
                    
                    char **new_array = realloc(*filenames, new_capacity * sizeof(char*));
                    if (!new_array) {
                        fprintf(stderr, "Error: Memory allocation failed\n");
                        
                        // Clean up already allocated strings
                        for (int i = 0; i < count; i++) {
                            free((*filenames)[i]);
                        }
                        for (int i = 0; i < subdir_count; i++) {
                            free(subdir_files[i]);
                        }
                        free(subdir_files);
                        free(*filenames);
                        *filenames = NULL;
                        closedir(dir);
                        return 1;
                    }
                    
                    *filenames = new_array;
                    capacity = new_capacity;
                }
                
                // Copy filenames from subdirectory
                for (int i = 0; i < subdir_count; i++) {
                    (*filenames)[count++] = subdir_files[i];
                }
                
                // Free only the array, not the strings
                free(subdir_files);
            }
            continue;
        }
        
        // Skip non-regular files
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        
        // Check extension if specified
        if (extension && !has_extension(entry->d_name, extension)) {
            continue;
        }
        
        // Resize array if needed
        if (count >= capacity) {
            capacity *= 2;
            char **new_array = realloc(*filenames, capacity * sizeof(char*));
            if (!new_array) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                
                // Clean up already allocated strings
                for (int i = 0; i < count; i++) {
                    free((*filenames)[i]);
                }
                free(*filenames);
                *filenames = NULL;
                closedir(dir);
                return 1;
            }
            *filenames = new_array;
        }
        
        // Copy filename with full path
        (*filenames)[count] = strdup(path);
        if (!(*filenames)[count]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            
            // Clean up already allocated strings
            for (int i = 0; i < count; i++) {
                free((*filenames)[i]);
            }
            free(*filenames);
            *filenames = NULL;
            closedir(dir);
            return 1;
        }
        
        count++;
        
        // Check if we've reached maximum file count
        if (count >= MAX_FILES) {
            fprintf(stderr, "Warning: Maximum file count (%d) reached, truncating list\n", MAX_FILES);
            break;
        }
    }
    
    closedir(dir);
    *file_count = count;

if (debug) {
        fprintf(stderr, "[DEBUG] Found %d files in directory: %s\n", count, dir_path);
    }
    
    // If no files found, return error
    if (count == 0) {
        fprintf(stderr, "Error: No matching files found in directory '%s'\n", dir_path);
        free(*filenames);
        *filenames = NULL;
        return 1;
    }
    
    return 0;
}

// Function to analyze a corpus of files
int analyze_corpus(char **filenames, int file_count, int sample_size, int random_seed, 
                   CorpusStats *corpus_stats, int debug) {
    LineStats file_stats;
    int analyzed_count = 0;
    int exit_code = 0;
    
    // Initialize corpus stats
    init_corpus_stats(corpus_stats);
    
    // Total lines and length for average calculation
    size_t total_line_length_sum = 0;
    
    // Setup sampling if required
    int *sample_indices = NULL;
    if (sample_size > 0 && sample_size < file_count) {
        if (debug) {
            fprintf(stderr, "[DEBUG] Using sampling mode: %d files from %d (seed: %d)\n", 
                    sample_size, file_count, random_seed);
        }
        
        // Initialize random seed
        srand(random_seed);
        
        // Allocate and initialize array for tracking which files are sampled
        sample_indices = calloc(file_count, sizeof(int));
        if (!sample_indices) {
            fprintf(stderr, "Error: memory allocation failed for sample indices\n");
            return 1;
        }
        
        // Select random files for sampling
        for (int i = 0; i < sample_size; i++) {
            int idx;
            do {
                idx = rand() % file_count;
            } while (sample_indices[idx]); // Ensure no duplicates
            sample_indices[idx] = 1;
        }
    }
    
    // Process each file
    for (int i = 0; i < file_count; i++) {
        // Skip if not in sample
        if (sample_indices && !sample_indices[i]) {
            continue;
        }
        
        // Reset stats for this file
        init_stats(&file_stats);
        
        if (debug) {
            fprintf(stderr, "[DEBUG] Analyzing file %d of %d: %s\n", 
                    analyzed_count + 1, 
                    sample_size > 0 ? sample_size : file_count, 
                    filenames[i]);
        }
        
        // Analyze this file
        int result = analyze_file(filenames[i], &file_stats, debug);
        if (result != 0) {
            fprintf(stderr, "Warning: Error analyzing file %s, skipping\n", filenames[i]);
            continue;
        }
        
        analyzed_count++;
        
        // Update corpus statistics
        corpus_stats->file_count++;
        corpus_stats->total_lines_analyzed += file_stats.total_lines;
        
        // Track file with longest line
        if (file_stats.max_line_length > corpus_stats->max_line_across_corpus) {
            corpus_stats->max_line_across_corpus = file_stats.max_line_length;
            corpus_stats->max_line_file_index = i;
            
            // Store filename with longest line
            if (corpus_stats->max_line_filename != NULL) {
                free(corpus_stats->max_line_filename);
            }
            corpus_stats->max_line_filename = strdup(filenames[i]);
        }
        
        // Update average line length (weighted)
        if (file_stats.total_lines > 0) {
            total_line_length_sum += (size_t)file_stats.avg_line_length * file_stats.total_lines;
        }
        
        // Count files with problematic line lengths
        if (file_stats.lines_over_100k > 0) {
            corpus_stats->files_with_long_lines++;
        }
        if (file_stats.lines_over_1m > 0) {
            corpus_stats->files_with_very_long_lines++;
        }
    }
    
    // Calculate corpus-wide average line length
    if (corpus_stats->total_lines_analyzed > 0) {
        corpus_stats->avg_line_length_corpus = (double)total_line_length_sum / corpus_stats->total_lines_analyzed;
    }
    
    if (sample_indices) {
        free(sample_indices);
    }
    
    if (analyzed_count == 0) {
        fprintf(stderr, "Error: No files were successfully analyzed\n");
        exit_code = 1;
    } else if (debug) {
        fprintf(stderr, "[DEBUG] Successfully analyzed %d files\n", analyzed_count);
    }
    
    return exit_code;
}

// Function to generate a configuration file from corpus analysis
void generate_config_file(const char *config_path, CorpusStats *stats) {
    FILE *f = fopen(config_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create config file %s: %s\n", 
                config_path, strerror(errno));
        return;
    }
    
    // Calculate next power of 2 for optimal chunk size
    size_t optimal_chunk_size = next_power_of_2(stats->max_line_across_corpus);
    
    // Add a small buffer for safety (20%)
    size_t safe_size = optimal_chunk_size + (optimal_chunk_size / 5);
    // Round back to power of 2 if needed
    if (safe_size > optimal_chunk_size) {
        optimal_chunk_size *= 2;
    }
    
    // Get current time for timestamp
    time_t now = time(NULL);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(f, "# linex corpus configuration\n");
    fprintf(f, "# Generated on %s\n", timestamp);
    fprintf(f, "# Based on analysis of %zu files\n\n", stats->file_count);
    
    fprintf(f, "# Recommended chunk size for this corpus\n");
    fprintf(f, "export SLICE_CHUNK_SIZE=%zu\n\n", optimal_chunk_size);
    
    fprintf(f, "# Corpus statistics\n");
    fprintf(f, "# Longest line: %zu bytes in file: %s\n", 
            stats->max_line_across_corpus, 
            stats->max_line_filename ? stats->max_line_filename : "unknown");
    fprintf(f, "# Average line length: %.1f bytes\n", stats->avg_line_length_corpus);
    fprintf(f, "# Files with lines >100KB: %zu of %zu (%.1f%%)\n", 
            stats->files_with_long_lines, stats->file_count,
            stats->file_count > 0 ? (stats->files_with_long_lines * 100.0 / stats->file_count) : 0);
    fprintf(f, "# Files with lines >1MB: %zu of %zu (%.1f%%)\n", 
            stats->files_with_very_long_lines, stats->file_count,
            stats->file_count > 0 ? (stats->files_with_very_long_lines * 100.0 / stats->file_count) : 0);
    
    // Include a sample slice command
    fprintf(f, "\n# Sample command with optimal chunk size:\n");
    fprintf(f, "# slice --start 0 --size 1048576 --file your_file.txt --full-lines-only\n");
    
    fclose(f);
    
    printf("Generated configuration file: %s\n", config_path);
    printf("To use: source %s\n", config_path);
}

// Read a file list for corpus analysis
int read_file_list(const char *list_filename, char ***filenames, int *file_count) {
    FILE *f = fopen(list_filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file list %s: %s\n", 
                list_filename, strerror(errno));
        return 1;
    }
    
    // Allocate initial array
    int capacity = 100;
    *filenames = malloc(capacity * sizeof(char*));
    if (!*filenames) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return 1;
    }
    
    *file_count = 0;
    char line[PATH_MAX];
    
    // Read lines from file
    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        // Skip empty lines
        if (len == 0) {
            continue;
        }
        
        // Resize array if needed
        if (*file_count >= capacity) {
            capacity *= 2;
            char **new_array = realloc(*filenames, capacity * sizeof(char*));
            if (!new_array) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                fclose(f);
                
                // Clean up already allocated strings
                for (int i = 0; i < *file_count; i++) {
                    free((*filenames)[i]);
                }
                free(*filenames);
                *filenames = NULL;
                *file_count = 0;
                return 1;
            }
            *filenames = new_array;
        }
        
        // Copy filename
        (*filenames)[*file_count] = strdup(line);
        if (!(*filenames)[*file_count]) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            fclose(f);
            
            // Clean up already allocated strings
            for (int i = 0; i < *file_count; i++) {
                free((*filenames)[i]);
            }
            free(*filenames);
            *filenames = NULL;
            *file_count = 0;
            return 1;
        }
        
        (*file_count)++;
        
        // Check if we've reached maximum file count
        if (*file_count >= MAX_FILES) {
            fprintf(stderr, "Warning: Maximum file count (%d) reached, truncating list\n", MAX_FILES);
            break;
        }
    }
    
    fclose(f);
    return 0;
}

void cleanup_file_list(char **filenames, int file_count) {
    if (filenames) {
        for (int i = 0; i < file_count; i++) {
            free(filenames[i]);
        }
        free(filenames);
    }
}

void show_help() {
    printf("Usage: linex [MODE] [OPTIONS]\n\n");
    printf("Examine line structure and distribution in files.\n\n");
    printf("MODES:\n");
    printf("  --file <filename>          Analyze a single file (default mode)\n");
    printf("  --directory <path>         Analyze files in a directory\n");
    printf("  --corpus-analysis          Analyze multiple files as a corpus\n\n");
    printf("OPTIONS:\n");
    printf("  --file-list <filename>     File containing list of files to analyze (one per line)\n");
    printf("  --extension <ext>          Only process files with this extension when using --directory\n");
    printf("  --recursive                Process subdirectories recursively when using --directory\n");
    printf("  --sample <n>               Analyze a random sample of n files from the list/directory\n");
    printf("  --seed <n>                 Set random seed for sampling (default: current time)\n");
    printf("  --config-output <path>     Output path for configuration file (default: .linexrc)\n");
    printf("  --json                     Output results in JSON format\n");
    printf("  --debug                    Print internal debug info\n");
    printf("  --help                     Show this help message\n\n");
    printf("EXAMPLES:\n");
    printf("  linex --file myfile.txt                   # Analyze a single file\n");
    printf("  linex --directory /path/to/logs           # Analyze all files in directory\n");
    printf("  linex --directory /path/to/logs --extension .log  # Analyze only .log files\n");
    printf("  linex --directory /path/to/logs --recursive  # Analyze recursively\n");
    printf("  linex --corpus-analysis --file-list files.txt  # Analyze a corpus\n");
    printf("  linex --corpus-analysis --directory /path/logs  # Analyze directory as corpus\n");
}

int main(int argc, char *argv[]) {
    const char *filename = NULL;
    const char *directory = NULL;
    const char *file_list = NULL;
    const char *extension = NULL;
    const char *config_output = ".linexrc";
    int debug = 0;
    int exit_code = 0;
    int recursive = 0;
    OutputFormat format = OUTPUT_TEXT;
    OperationMode mode = MODE_SINGLE_FILE;
    int sample_size = 0;
    int random_seed = time(NULL);
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--debug")) {
            debug = 1;
        } else if (!strcmp(argv[i], "--json")) {
            format = OUTPUT_JSON;
        } else if (!strcmp(argv[i], "--recursive")) {
            recursive = 1;
        } else if (!strcmp(argv[i], "--corpus-analysis")) {
            mode = MODE_CORPUS_ANALYSIS;
        } else if (!strcmp(argv[i], "--help")) {
            show_help();
            return 0;
        } else if (!strcmp(argv[i], "--file") && i + 1 < argc) {
            filename = argv[++i];
        } else if (!strcmp(argv[i], "--directory") && i + 1 < argc) {
            directory = argv[++i];
        } else if (!strcmp(argv[i], "--extension") && i + 1 < argc) {
            extension = argv[++i];
        } else if (!strcmp(argv[i], "--file-list") && i + 1 < argc) {
            file_list = argv[++i];
        } else if (!strcmp(argv[i], "--config-output") && i + 1 < argc) {
            config_output = argv[++i];
        } else if (!strcmp(argv[i], "--sample") && i + 1 < argc) {
            sample_size = atoi(argv[++i]);
            if (sample_size <= 0) {
                fprintf(stderr, "Error: Sample size must be positive\n");
                return 1;
            }
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            random_seed = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            show_help();
            return 1;
        }
    }
    
    // Validate arguments based on mode
    if (mode == MODE_SINGLE_FILE) {
        if (filename == NULL && directory == NULL) {
            fprintf(stderr, "Error: Either --file <filename> or --directory <path> is required.\n");
            show_help();
            return 1;
        }
        
        if (filename != NULL && directory != NULL) {
            fprintf(stderr, "Error: Cannot specify both --file and --directory.\n");
            show_help();
            return 1;
        }
        
        if (file_list != NULL) {
            fprintf(stderr, "Warning: --file-list is ignored in single file mode\n");
        }
        
        if (extension != NULL && directory == NULL) {
            fprintf(stderr, "Warning: --extension is only used with --directory\n");
        }
        
        if (recursive && directory == NULL) {
            fprintf(stderr, "Warning: --recursive is only used with --directory\n");
        }
    } else if (mode == MODE_CORPUS_ANALYSIS) {
        if (file_list == NULL && directory == NULL) {
            fprintf(stderr, "Error: Either --file-list <filename> or --directory <path> is required for corpus analysis.\n");
            show_help();
            return 1;
        }
        
        if (file_list != NULL && directory != NULL) {
            fprintf(stderr, "Error: Cannot specify both --file-list and --directory for corpus analysis.\n");
            show_help();
            return 1;
        }
        
        if (filename != NULL) {
            fprintf(stderr, "Warning: --file is ignored in corpus analysis mode\n");
        }
    }
    
    // Create file list from directory if specified
    char **filenames = NULL;
    int file_count = 0;
    
    // Handle directory mode
    if (directory != NULL) {
        if (scan_directory(directory, &filenames, &file_count, extension, recursive, debug) != 0) {
            return 1;
        }
        
        if (file_count == 0) {
            fprintf(stderr, "Error: No files found in directory '%s'\n", directory);
            return 1;
        }
        
        if (debug) {
            fprintf(stderr, "[DEBUG] Found %d files in directory %s\n", file_count, directory);
        }
    }
    
    // Execute based on mode
    if (mode == MODE_SINGLE_FILE) {
        if (filename != NULL) {
            // Single file mode
            LineStats stats;
            init_stats(&stats);
            
            if (analyze_file(filename, &stats, debug) != 0) {
                return 1;
            }
            
            // Output results in requested format
            switch (format) {
                case OUTPUT_JSON:
                    print_stats_json(filename, &stats, debug);
                    break;
                case OUTPUT_TEXT:
                default:
                    print_stats_text(filename, &stats, debug);
                    break;
            }
        } else if (directory != NULL && filenames != NULL) {
            // Directory with single file analysis for each file
            for (int i = 0; i < file_count; i++) {
                LineStats stats;
                init_stats(&stats);
                
                if (analyze_file(filenames[i], &stats, debug) != 0) {
                    fprintf(stderr, "Warning: Error analyzing file %s, skipping\n", filenames[i]);
                    continue;
                }
                
                // Output results in requested format with a separator between files
                if (i > 0) {
                    printf("\n%s\n\n", "========================================");
                }
                
                switch (format) {
                    case OUTPUT_JSON:
                        print_stats_json(filenames[i], &stats, debug);
                        break;
                    case OUTPUT_TEXT:
                    default:
                        print_stats_text(filenames[i], &stats, debug);
                        break;
                }
            }
        }
    } else if (mode == MODE_CORPUS_ANALYSIS) {
        if (file_list != NULL && directory == NULL) {
            // Read file list
            if (read_file_list(file_list, &filenames, &file_count) != 0) {
                return 1;
            }
            
            if (file_count == 0) {
                fprintf(stderr, "Error: No files found in file list\n");
                return 1;
            }
        }
        // else directory mode - filenames already populated
        
        // Cap sample size to file count
        if (sample_size > file_count) {
            fprintf(stderr, "Warning: Sample size (%d) exceeds file count (%d), using all files\n", 
                    sample_size, file_count);
            sample_size = 0;  // Analyze all files
        }
        
        CorpusStats corpus_stats;
        
        // Analyze corpus
        if (analyze_corpus(filenames, file_count, sample_size, random_seed, 
                          &corpus_stats, debug) != 0) {
            cleanup_file_list(filenames, file_count);
            return 1;
        }
        
        // Output results in requested format
        switch (format) {
            case OUTPUT_JSON:
                print_corpus_stats_json(&corpus_stats, debug);
                break;
            case OUTPUT_TEXT:
            default:
                print_corpus_stats_text(&corpus_stats, debug);
                break;
        }
        
        // Generate configuration file
        generate_config_file(config_output, &corpus_stats);
        
        // Cleanup
        if (corpus_stats.max_line_filename) {
            free(corpus_stats.max_line_filename);
        }
    }
    
    // Clean up file list if allocated
    if (filenames != NULL) {
        cleanup_file_list(filenames, file_count);
    }
    
    return exit_code;
}