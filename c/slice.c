#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>

#define MAX_ALLOC_SIZE (1UL << 30)  // 1 GiB

void show_help() {
    printf("Usage: slice --start <offset> --size <bytes> --file <filename> [--full-lines-only] [--debug]\n\n");
    printf("Extract a slice of bytes from a file.\n\n");
    printf("Options:\n");
    printf("  --start <offset>        Byte offset to start reading (0-based)\n");
    printf("  --size <bytes>          Number of bytes to read\n");
    printf("  --file <filename>       File to read from\n");
    printf("  --full-lines-only       Remove truncated lines at start/end of slice\n");
    printf("  --debug                 Print internal debug info\n");
    printf("  --help                  Show this help message\n");
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
    FILE *fp = NULL;
    char *buffer = NULL;
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

    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", filename, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(fileno(fp), &st) != 0 || !S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: not a regular file: %s\n", filename);
        exit_code = 1;
        goto cleanup;
    }

    if (fseeko(fp, 0, SEEK_END) != 0) {
        perror("fseeko");
        exit_code = 1;
        goto cleanup;
    }

    off_t file_size = ftello(fp);
    if (file_size < 0) {
        perror("ftello");
        exit_code = 1;
        goto cleanup;
    }

    if (start >= (size_t)file_size) {
        goto cleanup;
    }

    size_t to_read = (start + size > (size_t)file_size) ? (size_t)file_size - start : size;

    if (to_read == 0) {
        fprintf(stderr, "Error: nothing to read\n");
        goto cleanup;
    }

    if (to_read > MAX_ALLOC_SIZE) {
        fprintf(stderr, "Error: requested read size (%zu bytes) exceeds limit (%lu bytes)\n",
                to_read, MAX_ALLOC_SIZE);
        exit_code = 1;
        goto cleanup;
    }

    if (fseeko(fp, (off_t)start, SEEK_SET) != 0) {
        perror("fseeko");
        exit_code = 1;
        goto cleanup;
    }

    buffer = malloc(to_read);
    if (!buffer) {
        perror("malloc");
        exit_code = 1;
        goto cleanup;
    }

    errno = 0;
    size_t read = fread(buffer, 1, to_read, fp);
    if (read != to_read && ferror(fp)) {
        perror("fread");
        exit_code = 1;
        goto cleanup;
    }

    if (debug) {
        fprintf(stderr, "[DEBUG] Requested start: %zu\n", start);
        fprintf(stderr, "[DEBUG] Requested size: %zu\n", size);
        fprintf(stderr, "[DEBUG] Actual bytes read: %zu\n", read);
    }

    if (read > 0) {
        char *out_buf = buffer;
        size_t out_len = read;

        if (trim_lines) {
            // Trim partial first line
            if (start > 0) {
                char *first_nl = memchr(buffer, '\n', read);
                if (first_nl) {
                    size_t offset = (first_nl - buffer) + 1;
                    if (offset <= read) {
                        out_buf = buffer + offset;
                        out_len = read - offset;
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
            }
        }

        if (out_len > 0) {
            size_t written = fwrite(out_buf, 1, out_len, stdout);
            if (written != out_len) {
                perror("fwrite");
                exit_code = 1;
                goto cleanup;
            }
        }
    }

cleanup:
    free(buffer);
    if (fp) fclose(fp);
    return exit_code;
}
