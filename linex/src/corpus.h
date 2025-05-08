#ifndef CORPUS_H
#define CORPUS_H

#include <stddef.h>
#include <sys/types.h>

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

// Initialize corpus statistics
void init_corpus_stats(CorpusStats *stats);

// Function to analyze a corpus of files
int analyze_corpus(char **filenames, int file_count, int sample_size, int random_seed, 
                   CorpusStats *corpus_stats, int debug);

// Function to generate a configuration file from corpus analysis
void generate_config_file(const char *config_path, CorpusStats *stats);

#endif /* CORPUS_H */