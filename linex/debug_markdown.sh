#!/bin/bash
# Simple debug script for markdown segfaults

set -e
TEST_DIR="./test_debug"
BINARY="./bin/linex"

# Text colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

mkdir -p "$TEST_DIR"

# Create a very simple markdown file
cat > "$TEST_DIR/simple.md" << EOL
# Simple Header

Just a paragraph.
EOL

# Create a slightly more complex markdown file
cat > "$TEST_DIR/medium.md" << EOL
# Medium Markdown

## Subheader

- Item 1
- Item 2

Some paragraph text.
EOL

echo -e "${BLUE}[INFO]${NC} Testing with very simple markdown file..."
$BINARY --file "$TEST_DIR/simple.md" --markdown --debug

echo -e "${BLUE}[INFO]${NC} Testing with medium complexity markdown file..."
$BINARY --file "$TEST_DIR/medium.md" --markdown --debug

echo -e "${BLUE}[INFO]${NC} Tests completed."