# slice

**`slice`** is a minimal, reliable tool for extracting byte ranges from text files, with support for trimming incomplete lines. It is ideal for clean, line-aware chunking of large text files — such as for indexing, archival, or content analysis.

---

## Key Features

- Byte-range slicing: `--start`, `--size`
- Line integrity control: `--full-lines-only`
- Overlap-friendly chunking for contextual continuity
- Simple CLI interface (Shell, C, Go, Python, Ruby)
- Full test coverage across all versions

---

## Versions

| Language | File              | Notes                            |
|----------|-------------------|----------------------------------|
| C        | `c/slice.c`       | Primary tested version           |
| Shell    | `slice`           | Portable POSIX reference         |
| Go       | `go/slice.go`     | Self-contained CLI               |
| Python   | `slice.py`        | Easy integration, testable       |
| Ruby     | `slice.rb`        | Minimal version                  |

The C version is the most thoroughly tested, including edge cases and realistic content simulation.

---

## Basic Usage

```bash
slice --start 2048 --size 1024 --file input.txt
```

Outputs 1024 bytes starting at byte offset 2048 from `input.txt`.

To ensure no line is cut off:

```bash
slice --start 2048 --size 1024 --file input.txt --full-lines-only
```

This trims any partial lines at the start or end of the slice.

---

## Overlapping Chunking Example

```bash
slice --start 0    --size 2048 --file input.txt --full-lines-only > chunk_000.txt
slice --start 1024 --size 2048 --file input.txt --full-lines-only > chunk_001.txt
slice --start 2048 --size 2048 --file input.txt --full-lines-only > chunk_002.txt
```

Each chunk overlaps the previous by 1024 bytes to preserve contextual continuity at boundaries.

---

## Debug Mode

Use `--debug` to inspect internal logic and byte counts:

```bash
slice --start 0 --size 512 --file input.txt --full-lines-only --debug
```

---

## Test Suite

Run all tests:

```bash
./tests/run_all_tests.sh
```

Includes:
- Byte slicing
- Line trimming
- Edge conditions
- Overlapping chunks
- Full document reconstruction

---
## Benchmarks
Benchmarks can be generated using benchmarks.sh.
Be aware that:
- The shell script benchmark is very slow, you may want to disable it.
- Before running benchmarks.sh you must run the prepare.sh script in ./bench, it creates sample input files and it ensures that the required python packages are installed.
- The generated benchmark input files are about 160 Mb in size. They are in ./bench/sample_inputs,  you may want to delete them after running the benchmarks.

If you don't have the time and inclination to run the benchmarks yourself, I included a set of results running on a Macbook Pro M1 (the original, late 2020 model). 

You can see them here:
![Benchmarks report captured on 2025-05-05](https://ha1tch.github.io/slice/benchmark_report.html)

Some highlights from the aforementioned benchmark results:
### Time vs. File Size

![Time by File Size](https://raw.githubusercontent.com/ha1tch/slice/refs/heads/main/bench/results/plots/time_by_file_size.png)

### Throughput Comparison

![Throughput Comparison](https://raw.githubusercontent.com/ha1tch/slice/refs/heads/main/bench/results/plots/throughput_comparison.png)



## Structured Content Testing

The C version includes realistic simulation of structured text processing:

- Generates a `fakedown_input.md` file with:
  - Markdown-style headings and block elements
  - Long, scrambled Latin-style paragraphs
- Slices file into overlapping chunks
- Deduplicates and reconstructs the document
- Verifies that all content is preserved

---

## Test Artifacts

- `tests/c/fakedown_input.md` — synthetic structured input
- `tests/c/rag_chunks/` — overlapping slices
- Reconstruction and diff verification files

---

## License
```
MIT License

Copyright (c) 2025 haitch@duck.com
https://github.com/ha1tch/slice

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
---

## See Also

- `slice.1` — manual page
- `tests/c/test_slice.sh` — full C test suite with fakedown generation
