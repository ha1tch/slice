#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>  // for SIZE_MAX
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#define BASE_CHUNK_SIZE 8192  // Starting chunk size (8 KB)
#define MAX_CHUNK_SIZE (100 * 1024 * 1024)  // Max chunk size (100 MB)
#define MAX_ALLOC_SIZE (1UL << 30)  // 1 GiB maximum allocation

// Calculate optimal chunk size based on file size
size_t calculate_chunk_size(size_t file_size, int trim_lines, int debug) {
    // First, check if SLICE_CHUNK_SIZE environment variable is set
    const char *env_chunk_size = getenv("SLICE_CHUNK_SIZE");
    if (env_chunk_size != NULL) {
        char *endptr;
        errno = 0;
        unsigned long val = strtoul(env_chunk_size, &endptr, 10);
        if (errno == 0 && *endptr == '\0' && val > 0 && val <= MAX_CHUNK_SIZE) {
            if (debug) {
                fprintf(stderr, "[DEBUG] Using environment variable SLICE_CHUNK_SIZE: %lu bytes\n", val);
            }
            return (size_t)val;
        }
        // If invalid, log the error and fall back to default calculation
        if (debug) {
            fprintf(stderr, "[DEBUG] Invalid SLICE_CHUNK_SIZE value: %s, using calculated size\n", env_chunk_size);
        }
    }
    
    // Start with base chunk size
    size_t chunk_size = BASE_CHUNK_SIZE;
    
    // For each power of 10 increase in file size, double the chunk size
    // This gives a geometric progression
    for (size_t threshold = 100 * 1024; // 100 KB threshold
         threshold < file_size && chunk_size < MAX_CHUNK_SIZE; 
         threshold *= 10) {
        chunk_size *= 2;
    }
    
    // If --full-lines-only is specified, use a more conservative chunk size
    // to better handle potentially long lines
    if (trim_lines) {
        // Double the chunk size as a safety margin for long lines
        chunk_size *= 2;
        
        if (debug) {
            fprintf(stderr, "[DEBUG] Applied safety factor for --full-lines-only\n");
        }
    }
    
    // Ensure we don't exceed the maximum chunk size
    if (chunk_size > MAX_CHUNK_SIZE) {
        chunk_size = MAX_CHUNK_SIZE;
    }
    
    return chunk_size;
}

void show_help() {
    printf("Usage: slice4 --start <offset> --size <bytes> --file <filename> [--full-lines-only] [--debug]\n\n");
    printf("Extract a slice of bytes from a file.\n\n");
    printf("Options:\n");
    printf("  --start <offset>        Byte offset to start reading (0-based)\n");
    printf("  --size <bytes>          Number of bytes to read\n");
    printf("  --file <filename>       File to read from\n");
    printf("  --full-lines-only       Remove truncated lines at start/end of slice\n");
    printf("  --debug                 Print internal debug info\n");
    printf("  --help                  Show this help message\n");
    printf("\nEnvironment variables:\n");
    printf("  SLICE_CHUNK_SIZE        Override the chunk size (in bytes) for reading\n");
    printf("                          (can be set by 'linex' tool based on corpus analysis)\n");
}

// Portable replacement for GNU memrchr
void *memrchr_portable(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--) {
        if (*(--p) == (unsigned char)c)
            return (void *)p;
    }
    return NULL;
}

size_t parse_size(const char *arg, const char *name) {
    if (arg[0] == '-') {
        fprintf(stderr, "Invalid value for %s: negative number not allowed: %s\n", name, arg);
        exit(1);
    }

    char *end;
    errno = 0;
    unsigned long val = strtoul(arg, &end, 10);
    if (errno || *end != '\0') {
        fprintf(stderr, "Invalid value for %s: %s\n", name, arg);
        exit(1);
    }

    if (val > SIZE_MAX) {
        fprintf(stderr, "Invalid value for %s: exceeds system size limit\n", name);
        exit(1);
    }

    return (size_t)val;
}

int main(int argc, char *argv[]) {
    size_t start = (size_t)-1, size = 0;
    const char *filename = NULL;
    int debug = 0, trim_lines = 0;
    int fd = -1;
    char *buffer = NULL;
    char *line_buffer = NULL;  // For line trimming
    int exit_code = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--start") && i + 1 < argc) {
            start = parse_size(argv[++i], "--start");
        } else if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            size = parse_size(argv[++i], "--size");
        } else if (!strcmp(argv[i], "--file") && i + 1 < argc) {
            filename = argv[++i];
        } else if (!strcmp(argv[i], "--debug")) {
            debug = 1;
        } else if (!strcmp(argv[i], "--full-lines-only")) {
            trim_lines = 1;
        } else if (!strcmp(argv[i], "--help")) {
            show_help();
            return 0;
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            show_help();
            return 1;
        }
    }

    if (start == (size_t)-1 || size == 0 || filename == NULL) {
        fprintf(stderr, "Error: --start, --size, and --file are required.\n");
        show_help();
        return 1;
    }

    if (SIZE_MAX - start < size) {
        fprintf(stderr, "Error: start + size causes overflow\n");
        return 1;
    }

    // Open file using low-level I/O for better performance
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", filename, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: not a regular file: %s\n", filename);
        exit_code = 1;
        goto cleanup;
    }

    off_t file_size = st.st_size;
    
    if (start >= (size_t)file_size) {
        if (debug) {
            fprintf(stderr, "[DEBUG] Start position %zu is beyond file size %lld\n", 
                    start, (long long)file_size);
        }
        goto cleanup;  // Nothing to read
    }

    size_t to_read = (start + size > (size_t)file_size) ? (size_t)file_size - start : size;

    if (to_read == 0) {
        fprintf(stderr, "Error: nothing to read\n");
        goto cleanup;
    }

    // Calculate optimal chunk size based on file size
    size_t chunk_size = calculate_chunk_size((size_t)file_size, trim_lines, debug);
    
    if (debug) {
        fprintf(stderr, "[DEBUG] File size: %lld bytes\n", (long long)file_size);
        fprintf(stderr, "[DEBUG] Calculated chunk size: %zu bytes\n", chunk_size);
        fprintf(stderr, "[DEBUG] Requested start: %zu\n", start);
        fprintf(stderr, "[DEBUG] Requested size: %zu\n", size);
        fprintf(stderr, "[DEBUG] Actual bytes to read: %zu\n", to_read);
    }

    // Seek to the start position
    if (lseek(fd, (off_t)start, SEEK_SET) != (off_t)start) {
        perror("lseek");
        exit_code = 1;
        goto cleanup;
    }

    // Allocate buffer for chunks
    buffer = malloc(chunk_size);
    if (!buffer) {
        perror("malloc for buffer");
        exit_code = 1;
        goto cleanup;
    }

    if (trim_lines) {
        // For line trimming, we need to keep track of all data
        line_buffer = malloc(to_read);
        if (!line_buffer) {
            perror("malloc for line buffer");
            exit_code = 1;
            goto cleanup;
        }
    }

    // Read data in chunks
    size_t total_read = 0;
    while (total_read < to_read) {
        size_t current_chunk = (to_read - total_read < chunk_size) ? (to_read - total_read) : chunk_size;
        
        ssize_t bytes_read = read(fd, buffer, current_chunk);
        if (bytes_read <= 0) {
            if (bytes_read < 0) {
                perror("read");
                exit_code = 1;
            }
            break;  // EOF or error
        }

        if (trim_lines) {
            // Store data for line trimming later
            memcpy(line_buffer + total_read, buffer, bytes_read);
        } else {
            // Direct output when no line trimming needed
            if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
                perror("write");
                exit_code = 1;
                goto cleanup;
            }
        }

        total_read += bytes_read;
    }

    if (debug) {
        fprintf(stderr, "[DEBUG] Total bytes read: %zu\n", total_read);
    }

    // Process line trimming if requested
    if (trim_lines && total_read > 0) {
        char *out_buf = line_buffer;
        size_t out_len = total_read;

        // Trim partial first line
        if (start > 0) {
            char *first_nl = memchr(line_buffer, '\n', total_read);
            if (first_nl) {
                size_t offset = (first_nl - line_buffer) + 1;
                if (offset <= total_read) {
                    out_buf = line_buffer + offset;
                    out_len = total_read - offset;
                } else {
                    out_len = 0;
                }
            } else {
                out_len = 0;
            }
        }

        // Trim partial last line
        if (out_len > 0 && out_buf[out_len - 1] != '\n') {
            char *last_nl = memrchr_portable(out_buf, '\n', out_len);
            if (last_nl && last_nl >= out_buf) {
                out_len = last_nl - out_buf + 1;
            } else {
                out_len = 0;
            }
        }

        if (debug) {
            fprintf(stderr, "[DEBUG] After trimming: output length = %zu\n", out_len);
            if (out_len == 0 && total_read > 0) {
                fprintf(stderr, "[DEBUG] Warning: All content was trimmed due to --full-lines-only\n");
                fprintf(stderr, "[DEBUG] Hint: Try using 'linex' tool to analyze line structure\n");
            }
        }

        // Write the trimmed output
        if (out_len > 0) {
            if (write(STDOUT_FILENO, out_buf, out_len) != (ssize_t)out_len) {
                perror("write");
                exit_code = 1;
                goto cleanup;
            }
        }
    }

cleanup:
    free(buffer);
    free(line_buffer);
    if (fd >= 0) close(fd);
    return exit_code;
}