#!/bin/bash

# Enhanced Test Suite for Linex
# --------------------------
# This script tests the core functionality of the linex utility
# with improved error reporting and recovery

set -e  # Exit on any error
TEST_DIR="./test_data"
RESULT_DIR="./test_results"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/bin/linex"

# Text colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Create test directories if they don't exist
mkdir -p "$TEST_DIR"
mkdir -p "$RESULT_DIR"

# ===== Test Settings =====
VERBOSE=1           # 0=minimal output, 1=normal, 2=detailed
FAIL_FAST=0         # 1=stop on first error, 0=continue tests after error
TESTS_FAILED=0      # Counter for failed tests
TESTS_PASSED=0      # Counter for passed tests

# ===== Helper Functions =====

function log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

function log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

function log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

function log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    
    # Display debug info about the failed test
    if [ -n "$2" ]; then
        echo -e "${YELLOW}[DEBUG]${NC} Test output content:"
        echo "-------------------------------------"
        cat "$2"
        echo "-------------------------------------"
        
        # Additional debugging for specific assertions
        if [ -n "$3" ]; then
            echo -e "${YELLOW}[DEBUG]${NC} Searching for: $3"
            echo "Grep result:"
            grep -n "$3" "$2" || echo "Pattern not found in output"
        fi
    fi
    
    # Check stderr if available
    if [ -n "$4" ] && [ -f "$4" ]; then
        echo -e "${YELLOW}[DEBUG]${NC} Error output content:"
        echo "-------------------------------------"
        cat "$4"
        echo "-------------------------------------"
    fi
    
    if [ "$FAIL_FAST" -eq 1 ]; then
        echo -e "${RED}[ABORT]${NC} Stopping tests due to failure"
        exit 1
    fi
}

function check_binary() {
    if [ ! -x "$BINARY" ]; then
        echo -e "${RED}[FATAL]${NC} Linex binary not found at $BINARY or not executable"
        echo "Make sure you've successfully compiled the project"
        exit 1
    fi
}

function setup_test_files() {
    log_info "Setting up test files..."
    
    # Create a file with regular lines
    cat > "$TEST_DIR/regular.txt" << EOL
This is a normal text file.
It has several lines of reasonable length.
Nothing fancy here, just testing basic functionality.
Line lengths are all under 100 characters, which is typical for most text files.
EOL
    
    # Create a file with a very long line
    echo "This is a file with a normal first line." > "$TEST_DIR/longline.txt"
    yes "very_long_" | head -1000 | tr -d '\n' >> "$TEST_DIR/longline.txt"
    echo -e "\nThis is a normal last line." >> "$TEST_DIR/longline.txt"
    
    # Create a basic markdown file
    cat > "$TEST_DIR/sample.md" << EOL
# Sample Markdown Document

This is a sample markdown document for testing linex.

## Section 1

- Item 1
- Item 2
- Item 3

## Section 2

\`\`\`
Some code block
Multiple lines
Of code
\`\`\`

> This is a blockquote
> It spans multiple lines

### Subsection

Here's a [link](https://example.com) and an ![image](image.jpg).

---

| Column 1 | Column 2 |
|----------|----------|
| Cell 1   | Cell 2   |
| Cell 3   | Cell 4   |

EOL
    
    # Create a file with problematic markdown content
    cat > "$TEST_DIR/edge_case.md" << EOL
# Edge Case Document

This tests some edge cases:

\`\`\`
Unclosed code block without ending ticks

> Nested blockquote
> > With multiple levels
> > > Of depth

- [ ] Task list
- [x] Completed task

| Header | With | Many | Columns |
|--------|------|------|---------|
| And | just | one | row |

EOL
    
    # Create a directory with multiple files for testing directory mode
    mkdir -p "$TEST_DIR/multi"
    echo "File 1 content" > "$TEST_DIR/multi/file1.txt"
    echo "File 2 content" > "$TEST_DIR/multi/file2.txt"
    echo "File 3 content" > "$TEST_DIR/multi/file3.log"
    
    # Create a subdirectory for testing recursive mode
    mkdir -p "$TEST_DIR/multi/subdir"
    echo "Subdir file content" > "$TEST_DIR/multi/subdir/file4.txt"
    
    # Create a file list for corpus testing
    cat > "$TEST_DIR/filelist.txt" << EOL
$TEST_DIR/regular.txt
$TEST_DIR/longline.txt
$TEST_DIR/sample.md
EOL
    
    log_success "Test files created successfully"
    
    # Debug info about created files
    if [ "$VERBOSE" -ge 1 ]; then
        log_info "Test files content information:"
        find "$TEST_DIR" -type f -exec ls -la {} \; | awk '{print $5 " bytes: " $9}'
    fi
}

function run_command_with_timeout() {
    local cmd="$1"
    local output_file="$2"
    local error_file="$3"
    local timeout_seconds=10
    
    # Create a temporary file for exit status
    local exit_status_file=$(mktemp)
    
    # Run command with timeout
    if [ "$VERBOSE" -ge 2 ]; then
        log_info "Running command: $cmd"
    fi
    
    # Execute command with timeout and capture exit status
    timeout $timeout_seconds bash -c "$cmd > $output_file 2> $error_file; echo \$? > $exit_status_file" || {
        local timeout_status=$?
        if [ $timeout_status -eq 124 ]; then
            echo "Command timed out after $timeout_seconds seconds" >> $error_file
            echo "255" > $exit_status_file  # Use 255 to indicate timeout
        fi
    }
    
    # Read exit status
    local status=$(cat $exit_status_file)
    rm $exit_status_file
    
    if [ "$VERBOSE" -ge 1 ]; then
        log_info "Command exit status: $status"
    fi
    
    return $status
}

function cleanup() {
    log_info "Cleaning up test files..."
    rm -rf "$TEST_DIR"
    rm -rf "$RESULT_DIR"
    rm -f .linexrc
    log_success "Cleanup complete"
}

# ===== Test Cases =====

function test_single_file() {
    log_info "Testing single file analysis..."
    
    # Capture both stdout and stderr
    local output="$RESULT_DIR/regular.txt.out"
    local error="$RESULT_DIR/regular.txt.err"
    
    run_command_with_timeout "$BINARY --file $TEST_DIR/regular.txt" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Verify output contains expected sections
    grep -q "LINE LENGTH ANALYSIS FOR:" "$output" || 
        log_error "Missing analysis header" "$output" "LINE LENGTH ANALYSIS FOR:" "$error"
    
    grep -q "Total lines:" "$output" || 
        log_error "Missing line count" "$output" "Total lines:" "$error"
    
    grep -q "HISTOGRAM OF LINE LENGTHS:" "$output" || 
        log_error "Missing histogram" "$output" "HISTOGRAM OF LINE LENGTHS:" "$error"
    
    log_success "Single file analysis passed"
}

function test_long_line() {
    log_info "Testing file with long line..."
    
    local output="$RESULT_DIR/longline.txt.out"
    local error="$RESULT_DIR/longline.txt.err"
    
    run_command_with_timeout "$BINARY --file $TEST_DIR/longline.txt" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # First, show summary of the output file for debugging
    if [ "$VERBOSE" -ge 2 ]; then
        log_info "Output file size: $(wc -c < "$output") bytes"
        log_info "First 100 bytes of output:"
        head -c 100 "$output" | hexdump -C
    fi
    
    # Check for long line indication (different possible patterns)
    if ! (grep -q "CAUTION: File contains very long lines" "$output" || 
          grep -q "WARNING: File contains extremely long lines" "$output" ||
          grep -q "Lines over 100KB:" "$output"); then
        
        # Show the output in case of failure
        if [ "$VERBOSE" -ge 1 ]; then
            log_warning "Checking for any warning or caution messages:"
            grep -i "warning\|caution\|long lines" "$output" || echo "No warning/caution found"
            
            # Show statistics to see if long line was detected
            log_warning "Lines over 100KB statistic:"
            grep -A 5 "DISTRIBUTION" "$output" || echo "Distribution section not found"
        fi
        
        log_error "Missing long line warning" "$output" "" "$error"
        return
    fi
    
    log_success "Long line detection passed"
}

function test_markdown_analysis() {
    log_info "Testing markdown analysis..."
    
    local output="$RESULT_DIR/sample.md.out"
    local error="$RESULT_DIR/sample.md.err"
    
    run_command_with_timeout "$BINARY --file $TEST_DIR/sample.md --markdown" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Verify output contains markdown analysis
    grep -q "MARKDOWN STRUCTURE ANALYSIS FOR:" "$output" || 
        log_error "Missing markdown analysis" "$output" "MARKDOWN STRUCTURE ANALYSIS FOR:" "$error"
        
    grep -q "Component counts:" "$output" || 
        log_error "Missing component counts" "$output" "Component counts:" "$error"
        
    grep -q "Headers" "$output" || 
        log_error "Missing header count" "$output" "Headers" "$error"
    
    log_success "Markdown analysis passed"
}

function test_markdown_edge_cases() {
    log_info "Testing markdown analysis with edge cases..."
    
    local output="$RESULT_DIR/edge_case.md.out"
    local error="$RESULT_DIR/edge_case.md.err"
    
    run_command_with_timeout "$BINARY --file $TEST_DIR/edge_case.md --markdown" "$output" "$error"
    local status=$?
    
    # We're testing robustness, so even if it fails, we want to know what happened
    if [ $status -ne 0 ]; then
        log_warning "Edge case test resulted in non-zero exit code $status (might be expected)"
        if [ -s "$error" ]; then
            log_info "Error output for edge case:"
            cat "$error"
        fi
    fi
    
    # Verify we got either a valid analysis or a graceful error
    if [ -s "$output" ]; then
        if grep -q "MARKDOWN STRUCTURE ANALYSIS FOR:" "$output"; then
            log_success "Markdown edge case handled successfully"
        else
            log_error "Output present but does not contain markdown analysis" "$output" "MARKDOWN STRUCTURE ANALYSIS" "$error"
        fi
    elif [ -s "$error" ] && grep -q "Error" "$error"; then
        log_success "Markdown edge case resulted in graceful error"
    else
        log_error "Markdown edge case test failed with no useful output" "$output" "" "$error"
    fi
}

function test_json_output() {
    log_info "Testing JSON output..."
    
    local output="$RESULT_DIR/regular.txt.json"
    local error="$RESULT_DIR/regular.txt.json.err"
    
    run_command_with_timeout "$BINARY --file $TEST_DIR/regular.txt --json" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Basic JSON validation (checks if the output starts with { and ends with })
    head -c 1 "$output" | grep -q "{" || 
        log_error "Invalid JSON format (missing opening brace)" "$output" "{" "$error"
        
    tail -c 2 "$output" | grep -q "}" || 
        log_error "Invalid JSON format (missing closing brace)" "$output" "}" "$error"
    
    # Check for essential JSON fields
    grep -q "\"filename\":" "$output" || 
        log_error "Missing filename field in JSON" "$output" "\"filename\":" "$error"
    
    log_success "JSON output passed"
}

function test_directory_mode() {
    log_info "Testing directory mode..."
    
    local output="$RESULT_DIR/directory.out"
    local error="$RESULT_DIR/directory.err"
    
    run_command_with_timeout "$BINARY --directory $TEST_DIR/multi" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Verify output contains analysis for multiple files
    grep -q "LINE LENGTH ANALYSIS FOR:" "$output" || 
        log_error "Missing directory analysis" "$output" "LINE LENGTH ANALYSIS FOR:" "$error"
        
    # The separator might differ based on implementation
    if ! (grep -q "========================================" "$output" || 
          grep -q "----------------------------------------" "$output"); then
        log_error "Missing file separator" "$output" "" "$error"
        return
    fi
    
    log_success "Directory mode passed"
}

function test_directory_with_extension() {
    log_info "Testing directory mode with extension filter..."
    
    local output="$RESULT_DIR/directory_ext.out"
    local error="$RESULT_DIR/directory_ext.err"
    
    run_command_with_timeout "$BINARY --directory $TEST_DIR/multi --extension .log" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Output overview for debugging
    if [ "$VERBOSE" -ge 2 ]; then
        log_info "Directory with extension test output overview:"
        head -20 "$output"
    fi
    
    # Verify output contains only log files
    grep -q "file3.log" "$output" || 
        log_error "Missing .log file analysis" "$output" "file3.log" "$error"
        
    if grep -q "file1.txt" "$output"; then
        log_error ".txt file should not be included" "$output" "file1.txt" "$error"
        return
    fi
    
    log_success "Directory with extension filter passed"
}

function test_recursive_directory() {
    log_info "Testing recursive directory scan..."
    
    local output="$RESULT_DIR/recursive.out"
    local error="$RESULT_DIR/recursive.err"
    
    run_command_with_timeout "$BINARY --directory $TEST_DIR/multi --recursive" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Show files in directory to verify test setup
    if [ "$VERBOSE" -ge 2 ]; then
        log_info "Files in test directory:"
        find "$TEST_DIR/multi" -type f | sort
    fi
    
    # Verify output contains files from subdirectory
    if ! grep -q "subdir/file4.txt" "$output" && ! grep -q "file4.txt" "$output"; then
        if [ "$VERBOSE" -ge 1 ]; then
            log_info "Checking for any subdirectory files:"
            grep -i "subdir\|file4" "$output" || echo "No subdirectory files found"
        fi
        log_error "Missing file from subdirectory" "$output" "subdir/file4.txt" "$error"
        return
    fi
    
    log_success "Recursive directory scan passed"
}

function test_corpus_analysis() {
    log_info "Testing corpus analysis..."
    
    local output="$RESULT_DIR/corpus.out"
    local error="$RESULT_DIR/corpus.err"
    
    run_command_with_timeout "$BINARY --corpus-analysis --file-list $TEST_DIR/filelist.txt" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Verify output contains corpus analysis
    grep -q "CORPUS LINE LENGTH ANALYSIS" "$output" || 
        log_error "Missing corpus analysis" "$output" "CORPUS LINE LENGTH ANALYSIS" "$error"
        
    grep -q "Files analyzed:" "$output" || 
        log_error "Missing file count" "$output" "Files analyzed:" "$error"
    
    # Check if the config file was generated
    if [ ! -f ".linexrc" ]; then
        if [ "$VERBOSE" -ge 1 ]; then
            log_warning "Checking for any config files:"
            ls -la ./*rc 2>/dev/null || echo "No config files found"
        fi
        log_error "Config file .linexrc not generated" "$output" "" "$error"
        return
    fi
    
    log_success "Corpus analysis passed"
}

function test_sample_mode() {
    log_info "Testing sample mode..."
    
    local output="$RESULT_DIR/sample.out"
    local error="$RESULT_DIR/sample.err"
    
    # Use a fixed seed for reproducibility
    run_command_with_timeout "$BINARY --corpus-analysis --directory $TEST_DIR --sample 2 --seed 42" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Show stderr for debugging
    if [ "$VERBOSE" -ge 2 ] && [ -s "$error" ]; then
        log_info "Sample mode stderr output:"
        cat "$error"
    fi
    
    # Verify output contains sampling info
    grep -q "CORPUS LINE LENGTH ANALYSIS" "$output" || 
        log_error "Missing corpus analysis in sample mode" "$output" "CORPUS LINE LENGTH ANALYSIS" "$error"
    
    log_success "Sample mode passed"
}

function test_debug_mode() {
    log_info "Testing debug mode..."
    
    local output="$RESULT_DIR/debug.out"
    local error="$RESULT_DIR/debug.err"
    
    run_command_with_timeout "$BINARY --file $TEST_DIR/regular.txt --debug" "$output" "$error"
    local status=$?
    
    # Check for successful execution
    if [ $status -ne 0 ]; then
        log_error "Command failed with status $status" "$output" "" "$error"
        return
    fi
    
    # Verify output contains debug information - check both stdout and stderr
    if ! grep -q "DEBUG INFORMATION:" "$output" && ! grep -q "DEBUG INFORMATION:" "$error"; then
        if ! grep -q "\[DEBUG\]" "$output" && ! grep -q "\[DEBUG\]" "$error"; then
            log_error "Missing debug information" "$output" "DEBUG or [DEBUG]" "$error"
            return
        fi
    fi
    
    log_success "Debug mode passed"
}

# ===== Main Test Execution =====

function run_all_tests() {
    log_info "Starting linex test suite..."
    
    # Check if the binary exists
    check_binary
    
    # Setup test environment
    setup_test_files
    
    # Run individual tests
    test_single_file
    test_long_line
    test_markdown_analysis
    test_markdown_edge_cases
    test_json_output
    test_directory_mode
    test_directory_with_extension
    test_recursive_directory
    test_corpus_analysis
    test_sample_mode
    test_debug_mode
    
    # Show test summary
    echo "-------------------------------------------------"
    echo "TEST SUMMARY"
    echo "-------------------------------------------------"
    echo "Tests passed: $TESTS_PASSED"
    echo "Tests failed: $TESTS_FAILED"
    echo "Total tests:  $((TESTS_PASSED + TESTS_FAILED))"
    echo "-------------------------------------------------"
    
    if [ "$TESTS_FAILED" -gt 0 ]; then
        echo -e "${RED}OVERALL RESULT: FAILED${NC}"
        FINAL_STATUS=1
    else
        echo -e "${GREEN}OVERALL RESULT: PASSED${NC}"
        FINAL_STATUS=0
    fi
    echo "-------------------------------------------------"
    
    # Cleanup after tests
    if [ "$1" != "--no-cleanup" ]; then
        cleanup
    fi
    
    return $FINAL_STATUS
}

# Parse command line arguments
ARGS=()
for arg in "$@"; do
    case $arg in
        --verbose)
            VERBOSE=2
            ;;
        --quiet)
            VERBOSE=0
            ;;
        --fail-fast)
            FAIL_FAST=1
            ;;
        *)
            ARGS+=("$arg")
            ;;
    esac
done

# Run the tests with remaining arguments
run_all_tests "${ARGS[@]}"
exit $?