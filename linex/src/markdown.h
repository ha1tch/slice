#ifndef MARKDOWN_H
#define MARKDOWN_H

#include <stddef.h>

// Maximum number of markdown component types
#define MD_COMPONENT_COUNT 10

// Enum for markdown component types
typedef enum {
    MD_HEADER = 0,
    MD_LIST,
    MD_CODE_BLOCK,
    MD_BLOCKQUOTE,
    MD_TABLE,
    MD_LINK,
    MD_IMAGE,
    MD_HORIZONTAL,
    MD_PARAGRAPH,
    MD_BLANK,
} MarkdownComponentType;

// Struct to hold markdown statistics
typedef struct {
    size_t component_counts[MD_COMPONENT_COUNT];   // Count of each component type
    size_t lines_per_component[MD_COMPONENT_COUNT]; // Lines per component type
    size_t header_levels[6];                       // Count of each header level (H1-H6)
    size_t max_component_length[MD_COMPONENT_COUNT]; // Max length of each component
    double components_per_1000_lines;              // Density of components
    size_t total_components;                       // Total components (excl. blank lines)
} MarkdownStats;

// External reference to component names array
extern const char* md_component_names[];

// Initialize markdown stats structure
void init_markdown_stats(MarkdownStats *stats);

// Analyze a markdown file and collect statistics
int analyze_markdown_file(const char *filename, MarkdownStats *stats, int debug);

// Helper functions for markdown component identification
int is_header_line(const char *line, size_t length, int *level);
int is_list_line(const char *line, size_t length);
int is_blockquote_line(const char *line, size_t length);
int is_horizontal_rule_line(const char *line, size_t length);
int is_table_line(const char *line, size_t length);
int is_code_block_delimiter(const char *line, size_t length);
int contains_link(const char *line, size_t length);
int contains_image(const char *line, size_t length);
int is_blank_line(const char *line, size_t length);

#endif /* MARKDOWN_H */// markdown.h
#ifndef MARKDOWN_H
#define MARKDOWN_H

#include <stddef.h>

// Maximum number of markdown component types
#define MD_COMPONENT_COUNT 10

// Enum for markdown component types
typedef enum {
    MD_HEADER = 0,
    MD_LIST,
    MD_CODE_BLOCK,
    MD_BLOCKQUOTE,
    MD_TABLE,
    MD_LINK,
    MD_IMAGE,
    MD_HORIZONTAL,
    MD_PARAGRAPH,
    MD_BLANK
} MarkdownComponentType;

// Struct to hold markdown statistics
typedef struct {
    size_t component_counts[MD_COMPONENT_COUNT];   // Count of each component type
    size_t lines_per_component[MD_COMPONENT_COUNT]; // Lines per component type
    size_t header_levels[6];                       // Count of each header level (H1-H6)
    size_t max_component_length[MD_COMPONENT_COUNT]; // Max length of each component
    double components_per_1000_lines;              // Density of components
    size_t total_components;                       // Total components (excl. blank lines)
} MarkdownStats;

// External reference to component names array
extern const char* md_component_names[];

// Initialize markdown stats structure
void init_markdown_stats(MarkdownStats *stats);

// Analyze a markdown file and collect statistics
int analyze_markdown_file(const char *filename, MarkdownStats *stats, int debug);

// Helper functions for markdown component identification
int is_header_line(const char *line, size_t length, int *level);
int is_list_line(const char *line, size_t length);
int is_blockquote_line(const char *line, size_t length);
int is_horizontal_rule_line(const char *line, size_t length);
int is_table_line(const char *line, size_t length);
int is_code_block_delimiter(const char *line, size_t length);
int contains_link(const char *line, size_t length);
int contains_image(const char *line, size_t length);
int is_blank_line(const char *line, size_t length);

#endif /* MARKDOWN_H */