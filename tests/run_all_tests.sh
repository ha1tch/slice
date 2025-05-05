#!/bin/bash
set -e

# Get the absolute path to the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Running all slice tool tests..."

# Go tests
echo "==> Go tests"
cd "$ROOT_DIR/go"
go test -v

# Python tests
echo "==> Python tests"
cd "$SCRIPT_DIR/python"
python3 test_slice.py

# Ruby tests
echo "==> Ruby tests"
cd "$SCRIPT_DIR/ruby"
ruby test_slice.rb

# Shell tests
echo "==> Shell tests"
cd "$SCRIPT_DIR/shell"
bash test_slice.sh "$ROOT_DIR/slice"

echo "All tests passed."
