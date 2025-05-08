#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "utils.h"
#include "linestats.h"
#include "markdown.h"
#include "directory.h"
#include "corpus.h"
#include "output.h"

int main(int argc, char *argv[]) {
    const char *filename = NULL;
    const char *directory = NULL;
    const char *file_list = NULL;
    const char *extension = NULL;
    const char *config_output = ".linexrc";
    int debug = 0;
    int exit_code = 0;
    int recursive = 0;
    int analyze_markdown = 0;
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
        } else if (!strcmp(argv[i], "--markdown")) {
            analyze_markdown = 1;
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
        
        if (analyze_markdown) {
            fprintf(stderr, "Warning: --markdown is ignored in corpus analysis mode\n");
            analyze_markdown = 0;
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
            
            // Perform markdown analysis if requested
            if (analyze_markdown) {
                MarkdownStats md_stats;
                init_markdown_stats(&md_stats);
                
                if (analyze_markdown_file(filename, &md_stats, debug) == 0) {
                    // Output results in requested format
                    switch (format) {
                        case OUTPUT_JSON:
                            print_markdown_stats_json(filename, &md_stats);
                            break;
                        case OUTPUT_TEXT:
                        default:
                            print_markdown_stats_text(filename, &md_stats);
                            break;
                    }
                } else {
                    fprintf(stderr, "Warning: Failed to analyze markdown structure\n");
                }
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
                
                // Perform markdown analysis if requested
                if (analyze_markdown) {
                    MarkdownStats md_stats;
                    init_markdown_stats(&md_stats);
                    
                    if (analyze_markdown_file(filenames[i], &md_stats, debug) == 0) {
                        // Output results in requested format
                        switch (format) {
                            case OUTPUT_JSON:
                                print_markdown_stats_json(filenames[i], &md_stats);
                                break;
                            case OUTPUT_TEXT:
                            default:
                                print_markdown_stats_text(filenames[i], &md_stats);
                                break;
                        }
                    } else {
                        fprintf(stderr, "Warning: Failed to analyze markdown structure for %s\n", filenames[i]);
                    }
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