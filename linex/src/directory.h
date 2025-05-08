#ifndef DIRECTORY_H
#define DIRECTORY_H

// Function to check if a file has a specific extension
int has_extension(const char *filename, const char *extension);

// Function to scan a directory and build a list of files
int scan_directory(const char *dir_path, char ***filenames, int *file_count, 
                  const char *extension, int recursive, int debug);

// Read a file list for corpus analysis
int read_file_list(const char *list_filename, char ***filenames, int *file_count);

// Clean up a file list
void cleanup_file_list(char **filenames, int file_count);

#endif /* DIRECTORY_H */