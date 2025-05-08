#include "output.h"
#include <stdio.h>

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
        printf("  slice4 --start %lld --size %zu --file %s\n\n", 
               (long long)stats->max_line_position, stats->max_line_length, filename);
               
        // Command with optimal chunk size
        printf("  # Process with optimal chunk size:\n");
        printf("  export SLICE_CHUNK_SIZE=%zu  # Set optimal chunk size environment variable\n", optimal_chunk_size);
        printf("  slice4 --start 0 --size 1048576 --file %s --full-lines-only\n", filename);
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
    printf("  --markdown                 Analyze markdown structure (if file is markdown)\n");
    printf("  --json                     Output results in JSON format\n");
    printf("  --debug                    Print internal debug info\n");
    printf("  --help                     Show this help message\n\n");
    printf("EXAMPLES:\n");
    printf("  linex --file myfile.txt                   # Analyze a single file\n");
    printf("  linex --file README.md --markdown         # Analyze markdown structure\n");
    printf("  linex --directory /path/to/logs           # Analyze all files in directory\n");
    printf("  linex --directory /path/to/logs --extension .log  # Analyze only .log files\n");
    printf("  linex --directory /path/to/logs --recursive  # Analyze recursively\n");
    printf("  linex --corpus-analysis --file-list files.txt  # Analyze a corpus\n");
    printf("  linex --corpus-analysis --directory /path/logs  # Analyze directory as corpus\n");
}

void print_markdown_stats_json(const char *filename, MarkdownStats *stats) {
    printf("{\n");
    printf("  \"filename\": \"%s\",\n", filename);
    printf("  \"markdown_stats\": {\n");
    
    printf("    \"component_counts\": {\n");
    for (int i = 0; i < MD_COMPONENT_COUNT; i++) {
        printf("      \"%s\": %zu%s\n", 
               md_component_names[i], 
               stats->component_counts[i],
               (i < MD_COMPONENT_COUNT - 1) ? "," : "");
    }
    printf("    },\n");
    
    printf("    \"header_levels\": {\n");
    for (int i = 0; i < 6; i++) {
        printf("      \"h%d\": %zu%s\n", 
               i+1, 
               stats->header_levels[i],
               (i < 5) ? "," : "");
    }
    printf("    },\n");
    
    printf("    \"max_line_lengths\": {\n");
    for (int i = 0; i < MD_COMPONENT_COUNT; i++) {
        printf("      \"%s\": %zu%s\n", 
               md_component_names[i], 
               stats->max_component_length[i],
               (i < MD_COMPONENT_COUNT - 1) ? "," : "");
    }
    printf("    },\n");
    
    printf("    \"component_density\": %.1f,\n", stats->components_per_1000_lines);
    printf("    \"total_components\": %zu\n", stats->total_components);
    
    printf("  },\n");
    
    printf("  \"insights\": [\n");
    
    // Build insights array
    int insight_count = 0;
    
    // Header structure
    if (stats->component_counts[MD_HEADER] == 0) {
        printf("    \"%sNo headers found - document lacks structured sections\"%s\n", 
               insight_count == 0 ? "" : ",", "");
        insight_count++;
    } else if (stats->header_levels[0] == 0) {
        printf("    \"%sNo H1 headers found - document may be missing a main title\"%s\n", 
               insight_count == 0 ? "" : ",", "");
        insight_count++;
    }
    
    // Code density
    if (stats->component_counts[MD_CODE_BLOCK] > 0) {
        double code_percent = (double)stats->lines_per_component[MD_CODE_BLOCK] * 100 / 
                             (stats->total_components > 0 ? stats->total_components : 1);
        if (code_percent > 40) {
            printf("    \"%sHigh code density (%.1f%%) - document is code-heavy\"%s\n", 
                  insight_count == 0 ? "" : ",", code_percent, "");
            insight_count++;
        }
    }
    
    // List usage
    if (stats->component_counts[MD_LIST] > stats->component_counts[MD_PARAGRAPH] &&
        stats->component_counts[MD_LIST] > 5) {
        printf("    \"%sList-heavy document - consider more narrative text\"%s\n", 
               insight_count == 0 ? "" : ",", "");
        insight_count++;
    }
    
    // Long lines
    size_t max_line = 0;
    for (int i = 0; i < MD_COMPONENT_COUNT; i++) {
        if (stats->max_component_length[i] > max_line) {
            max_line = stats->max_component_length[i];
        }
    }
    
    if (max_line > 120) {
        printf("    \"%sVery long lines detected (%zu chars) - may affect readability\"%s\n", 
               insight_count == 0 ? "" : ",", max_line, "");
        insight_count++;
    }
    
    printf("  ]\n");
    printf("}\n");
}

void print_markdown_stats_text(const char *filename, MarkdownStats *stats) {
    printf("\nMARKDOWN STRUCTURE ANALYSIS FOR: %s\n", filename);
    printf("----------------------------------------\n");
    
    printf("Component counts:\n");
    for (int i = 0; i < MD_COMPONENT_COUNT; i++) {
        printf("  %-20s %zu\n", md_component_names[i], stats->component_counts[i]);
    }
    
    printf("\nHeader level distribution:\n");
    for (int i = 0; i < 6; i++) {
        printf("  H%d: %zu\n", i+1, stats->header_levels[i]);
    }
    
    printf("\nComponent density: %.1f components per 1000 lines\n", stats->components_per_1000_lines);
    
    printf("\nMax line lengths by component type:\n");
    for (int i = 0; i < MD_COMPONENT_COUNT; i++) {
        if (stats->max_component_length[i] > 0) {
            printf("  %-20s %zu characters\n", md_component_names[i], stats->max_component_length[i]);
        }
    }
    
    printf("\nDocument structure summary:\n");
    printf("  Total structural components: %zu\n", stats->total_components);
    
    // Provide insights based on the structure
    printf("\nMARKDOWN INSIGHTS:\n");
    printf("----------------------------------------\n");
    
    // Header structure
    if (stats->component_counts[MD_HEADER] == 0) {
        printf("! No headers found - document lacks structured sections\n");
    } else if (stats->header_levels[0] == 0) {
        printf("! No H1 headers found - document may be missing a main title\n");
    }
    
    // Code density
    if (stats->component_counts[MD_CODE_BLOCK] > 0) {
        double code_percent = (double)stats->lines_per_component[MD_CODE_BLOCK] * 100 / 
                             (stats->total_components > 0 ? stats->total_components : 1);
        if (code_percent > 40) {
            printf("! High code density (%.1f%%) - document is code-heavy\n", code_percent);
        }
    }
    
    // List usage
    if (stats->component_counts[MD_LIST] > stats->component_counts[MD_PARAGRAPH] &&
        stats->component_counts[MD_LIST] > 5) {
        printf("! List-heavy document - consider more narrative text\n");
    }
    
    // Long lines
    size_t max_line = 0;
    for (int i = 0; i < MD_COMPONENT_COUNT; i++) {
        if (stats->max_component_length[i] > max_line) {
            max_line = stats->max_component_length[i];
        }
    }
    
    if (max_line > 120) {
        printf("! Very long lines detected (%zu chars) - may affect readability\n", max_line);
    }
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
    printf("    \"extract_longest_line\": \"slice4 --start %lld --size %zu --file %s\",\n", 
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