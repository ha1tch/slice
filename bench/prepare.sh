#!/bin/bash
set -euo pipefail

INPUT_DIR="bench/sample_inputs"
REQUIREMENTS_FILE="bench/requirements.txt"

mkdir -p "$INPUT_DIR"

echo "Generating benchmark input files with Python..."
for size in 1 2 5 10 20 50 100; do
  padded=$(printf "%03d" "$size")
  file="$INPUT_DIR/input_${padded}MB.txt"
  echo "  $file ..."
  python3 -c "
with open('$file', 'w') as f:
    line = 'Benchmarking line of sample text for slice.\\n'
    repeats = ($size * 1024 * 1024) // len(line)
    f.writelines([line] * repeats)
" || { echo "Failed to generate $file"; exit 1; }
done

echo "All input files generated."

echo "Installing Python dependencies..."
pip install -r "$REQUIREMENTS_FILE"

echo "Preparation complete."
