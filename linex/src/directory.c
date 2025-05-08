#define _FILE_OFFSET_BITS 64
#include "directory.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

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

// Clean up a file list
void cleanup_file_list(char **filenames, int file_count) {
    if (filenames) {
        for (int i = 0; i < file_count; i++) {
            free(filenames[i]);
        }
        free(filenames);
    }
}