// markdown.c - completely rewritten for robustness
#include "markdown.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Names for each markdown component type
const char* md_component_names[] = {
    "Headers",
    "Lists",
    "Code Blocks",
    "Blockquotes",
    "Tables",
    "Links",
    "Images",
    "Horizontal Rules",
    "Paragraphs",
    "Blank Lines"
};

void init_markdown_stats(MarkdownStats *stats) {
    if (!stats) return;

    // Clear all values
    memset(stats, 0, sizeof(MarkdownStats));
}

int is_header_line(const char *line, size_t length, int *level) {
    if (!line || !level || length == 0) return 0;
    
    *level = 0;
    size_t i = 0;
    
    // Skip leading whitespace
    while (i < length && (line[i] == ' ' || line[i] == '\t'))
        i++;
    
    // Count consecutive # characters
    while (i < length && line[i] == '#') {
        (*level)++;
        i++;
        
        // Limit header level to 6 (standard markdown)
        if (*level > 6) return 0;
    }
    
    // Valid header must have space after # and level between 1-6
    return (*level >= 1 && *level <= 6 && i < length && (line[i] == ' ' || line[i] == '\t'));
}

int is_list_line(const char *line, size_t length) {
    if (!line || length < 2) return 0;
    
    size_t i = 0;
    
    // Skip leading whitespace
    while (i < length && (line[i] == ' ' || line[i] == '\t'))
        i++;
    
    if (i >= length) return 0;
    
    // Unordered list markers: -, *, +
    if (line[i] == '-' || line[i] == '*' || line[i] == '+') {
        i++;
        // Must be followed by space
        return (i < length && (line[i] == ' ' || line[i] == '\t'));
    }
    
    // Ordered list markers: 1., 2., etc.
    if (isdigit(line[i])) {
        while (i < length && isdigit(line[i]))
            i++;
        
        // Must be followed by . and space
        return (i+1 < length && line[i] == '.' && (line[i+1] == ' ' || line[i+1] == '\t'));
    }
    
    return 0;
}

int is_blockquote_line(const char *line, size_t length) {
    if (!line || length < 2) return 0;
    
    size_t i = 0;
    
    // Skip leading whitespace
    while (i < length && (line[i] == ' ' || line[i] == '\t'))
        i++;
    
    if (i >= length) return 0;
    
    // Blockquote marker: >
    return (line[i] == '>' && (i+1 >= length || line[i+1] == ' ' || line[i+1] == '\t'));
}

int is_horizontal_rule_line(const char *line, size_t length) {
    if (!line || length < 3) return 0;
    
    size_t i = 0;
    
    // Skip leading whitespace
    while (i < length && (line[i] == ' ' || line[i] == '\t'))
        i++;
    
    if (i >= length) return 0;
    
    // Check for at least 3 consecutive -, *, or =
    char marker = 0;
    if (line[i] == '-' || line[i] == '*' || line[i] == '=') {
        marker = line[i];
    } else {
        return 0;
    }
    
    int count = 0;
    while (i < length && (line[i] == marker || line[i] == ' ' || line[i] == '\t')) {
        if (line[i] == marker) count++;
        i++;
    }
    
    // Must be at least 3 markers and no other characters on the line
    return (count >= 3 && i >= length);
}

int is_table_line(const char *line, size_t length) {
    if (!line || length < 3) return 0;
    
    // Table lines contain at least one pipe character |
    for (size_t i = 0; i < length; i++) {
        if (line[i] == '|') return 1;
    }
    
    return 0;
}

int is_code_block_delimiter(const char *line, size_t length) {
    if (!line || length < 3) return 0;
    
    size_t i = 0;
    
    // Skip leading whitespace
    while (i < length && (line[i] == ' ' || line[i] == '\t'))
        i++;
    
    if (i >= length) return 0;
    
    // Check for 3 or more backticks or tildes
    char marker = 0;
    if (line[i] == '`' || line[i] == '~') {
        marker = line[i];
    } else {
        return 0;
    }
    
    int count = 0;
    while (i < length && line[i] == marker) {
        count++;
        i++;
        if (count >= 100) break; // Avoid potential infinite loop
    }
    
    return count >= 3;
}

int contains_link(const char *line, size_t length) {
    if (!line || length < 4) return 0; // Minimum size for [a](b)
    
    // Basic pattern for markdown link: [text](url)
    int in_brackets = 0;
    
    for (size_t i = 0; i < length - 1; i++) {
        if (line[i] == '[' && !in_brackets) {
            in_brackets = 1;
        } else if (line[i] == ']' && in_brackets && i+1 < length && line[i+1] == '(') {
            return 1;
        }
    }
    
    return 0;
}

int contains_image(const char *line, size_t length) {
    if (!line || length < 5) return 0; // Minimum size for ![a](b)
    
    // Basic pattern for markdown image: ![alt](url)
    for (size_t i = 0; i < length - 2; i++) {
        if (i+1 < length && line[i] == '!' && line[i+1] == '[') {
            return 1;
        }
    }
    
    return 0;
}

int is_blank_line(const char *line, size_t length) {
    if (!line) return 1; // NULL line considered blank
    
    for (size_t i = 0; i < length; i++) {
        if (line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') {
            return 0;
        }
    }
    return 1;
}

int analyze_markdown_file(const char *filename, MarkdownStats *md_stats, int debug) {
    if (!filename || !md_stats) {
        fprintf(stderr, "Error: null parameters passed to analyze_markdown_file\n");
        return 1;
    }

    // Initialize stats before doing anything
    init_markdown_stats(md_stats);

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: cannot open file '%s': %s\n", filename, strerror(errno));
        return 1;
    }
    
    if (debug) {
        fprintf(stderr, "[DEBUG] Analyzing markdown structure for: %s\n", filename);
    }
    
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    
    int in_code_block = 0;
    int current_component = MD_PARAGRAPH;
    size_t component_line_count = 0;
    size_t total_lines = 0;
    
    // Process each line
    while ((read = getline(&line, &len, file)) != -1) {
        if (!line) {
            if (debug) {
                fprintf(stderr, "[DEBUG] getline returned -1 with null line\n");
            }
            break;
        }

        total_lines++;
        
        // Safety check for read length
        if (read <= 0) {
            // Empty line, treat as blank
            md_stats->component_counts[MD_BLANK]++;
            continue;
        }
        
        // Remove trailing newline
        if (read > 0 && (line[read-1] == '\n' || line[read-1] == '\r')) {
            line[--read] = '\0';
        }
        
        // Check for code block delimiters
        if (is_code_block_delimiter(line, read)) {
            if (in_code_block) {
                // End of code block
                in_code_block = 0;
                md_stats->component_counts[MD_CODE_BLOCK]++;
                if (component_line_count > 0) {
                    md_stats->lines_per_component[MD_CODE_BLOCK] += component_line_count;
                }
                component_line_count = 0;
                current_component = MD_PARAGRAPH;  // Reset after code block ends
            } else {
                // Start of new code block
                in_code_block = 1;
                current_component = MD_CODE_BLOCK;
                component_line_count = 1;
            }
            continue;
        }
        
        // If inside code block, just count lines
        if (in_code_block) {
            component_line_count++;
            continue;
        }
        
        // Identify line type safely
        int header_level = 0;
        MarkdownComponentType line_type = MD_PARAGRAPH;  // Default
        
        if (is_blank_line(line, read)) {
            line_type = MD_BLANK;
            md_stats->component_counts[MD_BLANK]++;
        } else if (is_header_line(line, read, &header_level)) {
            line_type = MD_HEADER;
            md_stats->component_counts[MD_HEADER]++;
            // Ensure we don't go out of bounds
            if (header_level >= 1 && header_level <= 6) {
                md_stats->header_levels[header_level-1]++;
            }
        } else if (is_list_line(line, read)) {
            line_type = MD_LIST;
            if (current_component != MD_LIST) {
                md_stats->component_counts[MD_LIST]++;
                current_component = MD_LIST;
                component_line_count = 1;
            } else {
                component_line_count++;
            }
        } else if (is_blockquote_line(line, read)) {
            line_type = MD_BLOCKQUOTE;
            if (current_component != MD_BLOCKQUOTE) {
                md_stats->component_counts[MD_BLOCKQUOTE]++;
                current_component = MD_BLOCKQUOTE;
                component_line_count = 1;
            } else {
                component_line_count++;
            }
        } else if (is_horizontal_rule_line(line, read)) {
            line_type = MD_HORIZONTAL;
            md_stats->component_counts[MD_HORIZONTAL]++;
        } else if (is_table_line(line, read)) {
            line_type = MD_TABLE;
            if (current_component != MD_TABLE) {
                md_stats->component_counts[MD_TABLE]++;
                current_component = MD_TABLE;
                component_line_count = 1;
            } else {
                component_line_count++;
            }
        } else {
            // Default is paragraph
            line_type = MD_PARAGRAPH;
            if (current_component != MD_PARAGRAPH) {
                md_stats->component_counts[MD_PARAGRAPH]++;
                current_component = MD_PARAGRAPH;
                component_line_count = 1;
            } else {
                component_line_count++;
            }
        }
        
        // Validate line_type before using it as an index
        if (line_type >= 0 && line_type < MD_COMPONENT_COUNT) {
            // Track line length for max component length
            if (read > md_stats->max_component_length[line_type]) {
                md_stats->max_component_length[line_type] = read;
            }
        } else {
            if (debug) {
                fprintf(stderr, "[DEBUG] Invalid line_type value: %d\n", line_type);
            }
        }
        
        // Count links and images inside any component
        if (contains_link(line, read)) {
            md_stats->component_counts[MD_LINK]++;
        }
        
        if (contains_image(line, read)) {
            md_stats->component_counts[MD_IMAGE]++;
        }
    }
    
    // Finish the last component if we're in one
    if (in_code_block) {
        md_stats->component_counts[MD_CODE_BLOCK]++;
        if (component_line_count > 0) {
            md_stats->lines_per_component[MD_CODE_BLOCK] += component_line_count;
        }
    } else if (component_line_count > 0 && 
               current_component >= 0 && current_component < MD_COMPONENT_COUNT) {
        md_stats->lines_per_component[current_component] += component_line_count;
    }
    
    // Calculate total components (excluding blank lines)
    md_stats->total_components = 0;
    if (MD_BLANK > 0 && MD_BLANK < MD_COMPONENT_COUNT) {
        for (int i = 0; i < MD_BLANK; i++) {
            md_stats->total_components += md_stats->component_counts[i];
        }
    }
    
    // Calculate component density
    if (total_lines > 0) {
        md_stats->components_per_1000_lines = (double)md_stats->total_components * 1000 / total_lines;
    }
    
    if (debug) {
        fprintf(stderr, "[DEBUG] Markdown analysis complete: %zu components in %zu lines\n", 
                md_stats->total_components, total_lines);
    }
    
    if (line) {
        free(line);
        line = NULL;
    }
    
    fclose(file);
    return 0;
}