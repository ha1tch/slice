#ifndef OUTPUT_H
#define OUTPUT_H

#include "linestats.h"
#include "corpus.h"
#include "markdown.h"

// Output format options
typedef enum {
    OUTPUT_TEXT = 0,
    OUTPUT_JSON
} OutputFormat;

// Operation modes
typedef enum {
    MODE_SINGLE_FILE = 0,
    MODE_CORPUS_ANALYSIS
} OperationMode;

// Display text format statistics
void print_stats_text(const char *filename, LineStats *stats, int debug);

// Display JSON format statistics
void print_stats_json(const char *filename, LineStats *stats, int debug);

// Display corpus statistics in text format
void print_corpus_stats_text(CorpusStats *stats, int debug);

// Display corpus statistics in JSON format
void print_corpus_stats_json(CorpusStats *stats, int debug);

// Display markdown statistics in text format
void print_markdown_stats_text(const char *filename, MarkdownStats *stats);

// Display markdown statistics in JSON format
void print_markdown_stats_json(const char *filename, MarkdownStats *stats);

// Display help information
void show_help();

#endif /* OUTPUT_H */