package main

import (
	"bufio"
	"bytes"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
)

func main() {
	start := flag.Int("start", -1, "Byte offset to start reading (0-based)")
	size := flag.Int("size", -1, "Number of bytes to read")
	file := flag.String("file", "", "File to extract from")
	fullLinesOnly := flag.Bool("full-lines-only", false, "Remove truncated lines at start/end")
	help := flag.Bool("help", false, "Show help message")

	flag.Parse()

	if *help {
		printUsage()
		return
	}

	if *start < 0 || *size <= 0 || *file == "" {
		fmt.Fprintln(os.Stderr, "Error: --start must be >= 0, --size must be > 0, and --file is required.")
		printUsage()
		os.Exit(1)
	}

	if _, err := os.Stat(*file); errors.Is(err, os.ErrNotExist) {
		fmt.Fprintf(os.Stderr, "Error: file does not exist: %s\n", *file)
		os.Exit(1)
	}

	data, err := readSlice(*file, *start, *size)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file: %v\n", err)
		os.Exit(1)
	}

	if *fullLinesOnly {
		data = trimTruncatedLines(data, *start)
	}

	_, err = os.Stdout.Write(data)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error writing output: %v\n", err)
		os.Exit(1)
	}
}

func printUsage() {
	fmt.Println("Usage: slice --start <offset> --size <bytes> --file <filename> [--full-lines-only]")
	flag.PrintDefaults()
}

func readSlice(filename string, start int, size int) ([]byte, error) {
	f, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	_, err = f.Seek(int64(start), io.SeekStart)
	if err != nil {
		return nil, err
	}

	buf := make([]byte, size)
	readBytes, err := io.ReadFull(io.LimitReader(f, int64(size)), buf)
	if err != nil && !errors.Is(err, io.EOF) && !errors.Is(err, io.ErrUnexpectedEOF) {
		return nil, err
	}
	return buf[:readBytes], nil
}

func trimTruncatedLines(data []byte, start int) []byte {
	scanner := bufio.NewScanner(bytes.NewReader(data))
	lines := [][]byte{}

	for scanner.Scan() {
		line := append([]byte(nil), scanner.Bytes()...)
		lines = append(lines, line)
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintf(os.Stderr, "Error scanning lines: %v\n", err)
		return data
	}

	if start > 0 && len(lines) > 0 {
		lines = lines[1:]
	}

	if len(data) > 0 && data[len(data)-1] != '\n' && len(lines) > 0 {
		lines = lines[:len(lines)-1]
	}

	return bytes.Join(lines, []byte("\n"))
}
