import argparse
import os
import sys

def read_slice(file_path, start, size):
    try:
        with open(file_path, 'rb') as f:
            f.seek(start)
            return f.read(size)
    except Exception as e:
        print(f"Error reading file: {e}", file=sys.stderr)
        sys.exit(1)

def trim_truncated_lines(data, start):
    lines = data.splitlines(keepends=True)

    # Remove first line if start > 0
    if start > 0 and lines:
        lines = lines[1:]

    # Remove last line if not ending in newline
    if lines and not lines[-1].endswith(b'\n'):
        lines = lines[:-1]

    return b''.join(lines)

def main():
    parser = argparse.ArgumentParser(description='Extract a byte slice from a file.')
    parser.add_argument('--start', type=int, required=True, help='Byte offset to start (0-based)')
    parser.add_argument('--size', type=int, required=True, help='Number of bytes to read')
    parser.add_argument('--file', required=True, help='File to extract from')
    parser.add_argument('--full-lines-only', action='store_true', help='Remove truncated lines at start/end')

    args = parser.parse_args()

    if args.start < 0:
        parser.error("--start must be >= 0")
    if args.size <= 0:
        parser.error("--size must be > 0")
    if not os.path.isfile(args.file):
        parser.error(f"File not found or not a regular file: {args.file}")

    try:
        data = read_slice(args.file, args.start, args.size)
        if args.full_lines_only:
            data = trim_truncated_lines(data, args.start)
        sys.stdout.buffer.write(data)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()