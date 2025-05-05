# slice – byte-slice extractor for text or binary files

A small command-line utility to extract a byte slice from any position in a file and optionally trim truncated lines.

---

## Features

* Extract a chunk from any byte offset
* Optionally remove **truncated** lines at the beginning and end
* POSIX-compatible (works on macOS and Linux)
* Requires only standard tools: `head`, `tail`, `wc`

---

## Example Usage

```bash
# Extract bytes 2048–4095
slice --start 2048 --size 2048 --file input.txt

# Remove truncated lines within the same slice
slice --start 2048 --size 2048 --file input.txt --full-lines-only

# Extract the first 1024 bytes of a file
slice --start 0 --size 1024 --file input.txt

# Extract the last 1024 bytes of a file (using file size knowledge)
size=$(wc -c < input.txt)
offset=$((size - 1024))
slice --start "$offset" --size 1024 --file input.txt

# Chunk a file into 4KB segments in a loop
for ((i = 0; i < $(wc -c < input.txt); i += 4096)); do
  slice --start $i --size 4096 --file input.txt > chunk_$i.txt
done

# Chunk a file with overlapping segments (e.g., for RAG use)
overlap=512
chunk_size=2048
file_size=$(wc -c < input.txt)

for ((i = 0; i < file_size; i += chunk_size - overlap)); do
  slice --start $i --size $chunk_size --file input.txt --full-lines-only > chunk_${i}.txt
done
```

---

## Options

| Option              | Description                                      |
| ------------------- | ------------------------------------------------ |
| `--start <offset>`  | Byte offset to start (0-based)                   |
| `--size <bytes>`    | Number of bytes to read                          |
| `--file <filename>` | File to extract from                             |
| `--full-lines-only` | Remove truncated lines at start/end of the slice |
| `--help`            | Show usage information                           |

---

## Installation

1. Copy `slice` to `~/bin/slice`
2. Make it executable:

   ```bash
   chmod +x ~/bin/slice
   ```
3. Ensure `~/bin` is in your `PATH`:

   ```bash
   export PATH="$HOME/bin:$PATH"
   ```

---

## Compatibility

Tested on:

* macOS (BSD utilities)
* Linux (GNU coreutils)

No `awk`, `sed`, or nonstandard dependencies.

---

## Man Page

You can create a manual page entry for `slice` by writing a `slice.1` file:

```man
.TH SLICE 1 "May 2025" "slice 1.0" "User Commands"
.SH NAME
slice \- extract byte slices from a file with optional line trimming
.SH SYNOPSIS
.B slice
[\-\-start OFFSET] [\-\-size BYTES] [\-\-file FILE] [\-\-full-lines-only]
.SH DESCRIPTION
.B slice
is a command-line utility that extracts a specified byte range from a file. It can optionally remove truncated lines at the beginning and end of the slice.
.SH OPTIONS
.TP
.B \-\-start OFFSET
Byte offset to begin reading (0-based).
.TP
.B \-\-size BYTES
Number of bytes to read.
.TP
.B \-\-file FILE
File to extract from.
.TP
.B \-\-full-lines-only
Remove truncated lines at the start and end of the slice.
.TP
.B \-\-help
Display usage information.
.SH EXAMPLES
.TP
Extract 2048 bytes starting from byte 4096:
.B
slice --start 4096 --size 2048 --file input.txt
.TP
Extract overlapping chunks for RAG use:
.B
slice --start 0 --size 2048 --file input.txt --full-lines-only
.SH AUTHOR
haitch@duck.com
.SH LICENSE
MIT License. See project README for details.
```

To install:

```bash
sudo mkdir -p /usr/local/share/man/man1
sudo cp slice.1 /usr/local/share/man/man1/
sudo mandb  # optional: update man page index
```

Then use it with:

```bash
man slice
```

---

## License

MIT License

Copyright (c)2025 haitch@duck.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
