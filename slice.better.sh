#!/bin/bash

show_help() {
  cat <<EOF
Usage: slice --start <offset> --size <bytes> --file <filename> [--full-lines-only] [--debug]

Extracts a slice of bytes from a file and optionally trims truncated lines.

Options:
  --start <offset>        Byte offset to start reading (0-based)
  --size <bytes>          Number of bytes to read
  --file <filename>       File to read from
  --full-lines-only       Remove truncated lines at start/end of slice
  --debug                 Print internal debug info
  --help                  Show this help message

Examples:
  slice --start 2048 --size 1024 --file input.txt
  slice --start 4096 --size 2048 --file input.txt --full-lines-only
EOF
}

# Parse args with getopt for better handling
if ! PARSED=$(getopt -o "" --long start:,size:,file:,full-lines-only,debug,help -n "$(basename "$0")" -- "$@"); then
  echo "Invalid arguments." >&2
  show_help
  exit 1
fi
eval set -- "$PARSED"

# Defaults
start=-1
size=-1
file=""
full_lines_only=false
debug=false

# Process options
while true; do
  case "$1" in
    --start) start="$2"; shift 2 ;;
    --size) size="$2"; shift 2 ;;
    --file) file="$2"; shift 2 ;;
    --full-lines-only) full_lines_only=true; shift ;;
    --debug) debug=true; shift ;;
    --help) show_help; exit 0 ;;
    --) shift; break ;;
    *) echo "Programming error"; exit 3 ;;
  esac
done

# Validate input
if [[ "$start" -lt 0 || "$size" -le 0 || -z "$file" ]]; then
  echo "Error: --start must be >= 0, --size must be > 0, and --file is required." >&2
  show_help
  exit 1
fi

if [[ ! -f "$file" ]]; then
  echo "Error: file does not exist: $file" >&2
  exit 1
fi

$debug && {
  echo "DEBUG: start=$start"
  echo "DEBUG: size=$size"
  echo "DEBUG: file=$file"
  echo "DEBUG: full_lines_only=$full_lines_only"
}

# Use better dd options for performance
if $debug; then
  echo "DEBUG: extracting with: dd if='$file' bs=1 skip=$start count=$size"
  raw_slice=$(dd if="$file" bs=1 skip="$start" count="$size" status=none)
  echo "DEBUG: raw_slice bytes:"
  echo "$raw_slice" | xxd
else
  # For non-debug mode, use a more efficient block size
  # First determine if we need to extract partial blocks
  bs=8192  # Use a more efficient block size
  skip_blocks=$((start / bs))
  skip_bytes=$((start % bs))
  
  if [ "$skip_bytes" -eq 0 ]; then
    # We're aligned to block boundaries
    raw_slice=$(dd if="$file" bs="$bs" skip="$skip_blocks" count="$((size / bs + 1))" status=none | head -c "$size")
  else
    # We need a temporary file to handle unaligned reads efficiently
    raw_slice=$(dd if="$file" bs="$bs" skip="$skip_blocks" count="$((size / bs + 2))" status=none | 
                tail -c "+$((skip_bytes + 1))" | head -c "$size")
  fi
fi

# Line trimming logic - use sed instead of echo | tail combinations
if $full_lines_only; then
  $debug && echo "DEBUG: trimming full lines only..."

  # Create a temporary file for processing
  tmpfile=$(mktemp)
  printf '%s' "$raw_slice" > "$tmpfile"
  
  # Remove the first line if start > 0 (likely truncated)
  if [[ "$start" -gt 0 ]]; then
    sed -i '1d' "$tmpfile"
    $debug && { echo "DEBUG: after trimming first line:"; cat "$tmpfile" | xxd; }
  fi

  # Remove the last line if not ending with newline
  if [[ -s "$tmpfile" && "$(tail -c 1 "$tmpfile" | xxd -p)" != "0a" ]]; then
    # More efficient approach to remove last line
    sed -i -e '$d' "$tmpfile"
    $debug && { echo "DEBUG: after trimming last line:"; cat "$tmpfile" | xxd; }
  fi
  
  # Get the result and clean up
  raw_slice=$(cat "$tmpfile")
  rm "$tmpfile"
fi

# Output result
printf '%s' "$raw_slice"