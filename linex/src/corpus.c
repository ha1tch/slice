#include "corpus.h"
#include "linestats.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

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
    fprintf(f, "# slice4 --start 0 --size 1048576 --file your_file.txt --full-lines-only\n");
    
    fclose(f);
    
    printf("Generated configuration file: %s\n", config_path);
    printf("To use: source %s\n", config_path);
}