# test_slice.py - Python CLI tests for the slice tool with progress output
import subprocess
import os
import sys

SLICE_CMD = ["python3", "../../slice.py"]
TEST_FILE = "test.txt"

def run_slice(*args):
    return subprocess.run(
        SLICE_CMD + list(args),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

def write_file(content):
    with open(TEST_FILE, "wb") as f:
        f.write(content)

def cleanup():
    if os.path.exists(TEST_FILE):
        os.remove(TEST_FILE)

def run_test(name, func):
    print(f"=== RUN   {name}")
    try:
        func()
        print(f"--- PASS: {name}")
    except AssertionError as e:
        print(f"--- FAIL: {name}")
        print(f"    {e}")
        return False
    except Exception as e:
        print(f"--- FAIL: {name}")
        print(f"    Unexpected error: {e}")
        return False
    return True

def test_basic_slice():
    write_file(b"Line 1\nLine 2\nLine 3\n")
    result = run_slice("--start", "7", "--size", "7", "--file", TEST_FILE)
    assert result.stdout == b"Line 2\n", f"Expected 'Line 2\\n', got: {result.stdout}"

def test_start_at_zero():
    write_file(b"abcdefghij")
    result = run_slice("--start", "0", "--size", "5", "--file", TEST_FILE)
    assert result.stdout == b"abcde", f"Expected 'abcde', got: {result.stdout}"

def test_slice_at_eof():
    write_file(b"1234567890")
    result = run_slice("--start", "8", "--size", "4", "--file", TEST_FILE)
    assert result.stdout == b"90", f"Expected '90', got: {result.stdout}"

def test_oversized_slice():
    write_file(b"abcdefghij")
    result = run_slice("--start", "0", "--size", "100", "--file", TEST_FILE)
    assert result.stdout == b"abcdefghij", f"Expected full file content, got: {result.stdout}"

def test_missing_file():
    result = run_slice("--start", "0", "--size", "10", "--file", "nonexistent.txt")
    assert result.returncode != 0, f"Expected non-zero exit code, got {result.returncode}"
    assert b"error" in result.stderr.lower(), f"Expected 'error' in stderr, got: {result.stderr.decode()}"

def test_missing_arguments():
    result = run_slice("--start", "0", "--size", "10")
    assert result.returncode != 0, f"Expected non-zero exit code, got {result.returncode}"
    assert b"error" in result.stderr.lower(), f"Expected 'error' in stderr, got: {result.stderr.decode()}"

if __name__ == "__main__":
    tests = [
        ("TestBasicSlice", test_basic_slice),
        ("TestStartAtZero", test_start_at_zero),
        ("TestSliceAtEOF", test_slice_at_eof),
        ("TestOversizedSlice", test_oversized_slice),
        ("TestMissingFile", test_missing_file),
        ("TestMissingArguments", test_missing_arguments),
    ]

    all_passed = True
    for name, func in tests:
        cleanup()
        if not run_test(name, func):
            all_passed = False

    cleanup()

    if not all_passed:
        print("FAIL")
        sys.exit(1)

    print("PASS")
