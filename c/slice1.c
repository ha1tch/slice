#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>

#define BUFFER_SIZE 8192

// Custom implementation of memrchr for platforms that don't have it
void* my_memrchr(const void* s, int c, size_t n) {
    const unsigned char* cp = (const unsigned char*)s + n;
    while (n--) {
        if (*(--cp) == (unsigned char)c)
            return (void*)cp;
    }
    return NULL;
}

void show_help() {
    printf("Usage: slice --start <offset> --size <bytes> --file <filename> [--full-lines-only] [--debug]\n\n");
    printf("Extracts a slice of bytes from a file and optionally trims truncated lines.\n\n");
    printf("Options:\n");
    printf("  --start <offset>        Byte offset to start reading (0-based)\n");
    printf("  --size <bytes>          Number of bytes to read\n");
    printf("  --file <filename>       File to read from\n");
    printf("  --full-lines-only       Remove truncated lines at start/end of slice\n");
    printf("  --debug                 Print internal debug info\n");
    printf("  --help                  Show this help message\n\n");
    printf("Examples:\n");
    printf("  slice --start 2048 --size 1024 --file input.txt\n");
    printf("  slice --start 4096 --size 2048 --file input.txt --full-lines-only\n");
}

int main(int argc, char *argv[]) {
    // Default values
    off_t start = -1;
    size_t size = 0;
    char *file = NULL;
    bool full_lines_only = false;
    bool debug = false;
    
    // Define long options
    static struct option long_options[] = {
        {"start", required_argument, 0, 's'},
        {"size", required_argument, 0, 'z'},
        {"file", required_argument, 0, 'f'},
        {"full-lines-only", no_argument, 0, 'l'},
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Parse arguments
    int option_index = 0;
    int c;
    while ((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != -1) {
        switch (c) {
            case 's':
                start = atoll(optarg);
                break;
            case 'z':
                size = atoll(optarg);
                break;
            case 'f':
                file = optarg;
                break;
            case 'l':
                full_lines_only = true;
                break;
            case 'd':
                debug = true;
                break;
            case 'h':
                show_help();
                return 0;
            case '?':
                return 1;
            default:
                abort();
        }
    }
    
    // Validate input
    if (start < 0 || size <= 0 || file == NULL) {
        fprintf(stderr, "Error: --start must be >= 0, --size must be > 0, and --file is required.\n");
        show_help();
        return 1;
    }
    
    // Check if file exists
    struct stat st;
    if (stat(file, &st) != 0) {
        fprintf(stderr, "Error: file does not exist: %s\n", file);
        return 1;
    }
    
    if (debug) {
        fprintf(stderr, "DEBUG: start=%lld\n", (long long)start);
        fprintf(stderr, "DEBUG: size=%zu\n", size);
        fprintf(stderr, "DEBUG: file=%s\n", file);
        fprintf(stderr, "DEBUG: full_lines_only=%s\n", full_lines_only ? "true" : "false");
    }
    
    // Open file
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        return 1;
    }
    
    // Seek to the start position
    if (lseek(fd, start, SEEK_SET) != start) {
        perror("Error seeking in file");
        close(fd);
        return 1;
    }
    
    // Read data into memory buffer
    char *buffer = malloc(size + 1);  // +1 for null terminator
    if (!buffer) {
        perror("Memory allocation failed");
        close(fd);
        return 1;
    }
    
    ssize_t bytes_read = 0;
    size_t total_read = 0;
    char *p = buffer;
    
    // Read in chunks for better performance
    while (total_read < size) {
        size_t to_read = size - total_read;
        if (to_read > BUFFER_SIZE) to_read = BUFFER_SIZE;
        
        bytes_read = read(fd, p, to_read);
        if (bytes_read <= 0) break;  // EOF or error
        
        total_read += bytes_read;
        p += bytes_read;
    }
    
    close(fd);
    
    // Null terminate the buffer for string operations
    buffer[total_read] = '\0';
    
    if (debug) {
        fprintf(stderr, "DEBUG: Read %zu bytes\n", total_read);
    }
    
    // Process line trimming if requested
    if (full_lines_only) {
        char *result = buffer;
        size_t result_len = total_read;
        
        if (debug) {
            fprintf(stderr, "DEBUG: trimming full lines only...\n");
        }
        
        // Skip first partial line if we're not at the beginning of the file
        if (start > 0) {
            char *newline = strchr(buffer, '\n');
            if (newline) {
                result = newline + 1;
                result_len = total_read - (result - buffer);
                
                if (debug) {
                    fprintf(stderr, "DEBUG: after trimming first line, %zu bytes remain\n", result_len);
                }
            }
        }
        
        // Remove last partial line if needed
        if (result_len > 0 && result[result_len - 1] != '\n') {
            // Use our cross-platform memrchr implementation
            char *last_newline = my_memrchr(result, '\n', result_len);
            if (last_newline) {
                result_len = last_newline - result + 1;  // Include the newline
                
                if (debug) {
                    fprintf(stderr, "DEBUG: after trimming last line, %zu bytes remain\n", result_len);
                }
            } else {
                // No newlines, so result is empty
                result_len = 0;
                
                if (debug) {
                    fprintf(stderr, "DEBUG: no newlines, result is empty\n");
                }
            }
        }
        
        // Output the processed result
        if (result_len > 0) {
            write(STDOUT_FILENO, result, result_len);
        }
    } else {
        // Output the raw buffer
        if (total_read > 0) {
            write(STDOUT_FILENO, buffer, total_read);
        }
    }
    
    free(buffer);
    return 0;
}