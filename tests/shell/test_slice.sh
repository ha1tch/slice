#!/bin/bash
set -e

SLICE="$1"
TEST_FILE="test.txt"
ERR_FILE="err.txt"
DEBUG_LOG="debug.log"

if [[ ! -f "$SLICE" ]]; then
  echo "Error: $SLICE not found"
  exit 1
fi

if [[ ! -x "$SLICE" ]]; then
  echo "Making $SLICE executable..."
  chmod +x "$SLICE"
fi

run_test() {
  local name=$1
  echo "=== RUN   $name"
  if "$name"; then
    echo "--- PASS: $name"
  else
    echo "--- FAIL: $name"
    if [[ -s "$DEBUG_LOG" ]]; then
      echo "    Debug output:"
      cat "$DEBUG_LOG"
    fi
    exit 1
  fi
  rm -f "$TEST_FILE" "$ERR_FILE" "$DEBUG_LOG"
}

write_file() {
  echo -ne "$1" > "$TEST_FILE"
}

test_basic_slice() {
  write_file "Line 1\nLine 2\nLine 3\n"
  output=$("$SLICE" --start 7 --size 7 --file "$TEST_FILE" --debug 2>&1 | tee "$DEBUG_LOG")
  result=$(echo "$output" | tail -n 1)
  [[ "$result" == "Line 2" ]]
}

test_start_at_zero() {
  write_file "abcdefghij"
  output=$("$SLICE" --start 0 --size 5 --file "$TEST_FILE" --debug 2>&1 | tee "$DEBUG_LOG")
  result=$(echo "$output" | tail -n 1)
  [[ "$result" == "abcde" ]]
}

test_slice_at_eof() {
  write_file "1234567890"
  output=$("$SLICE" --start 8 --size 4 --file "$TEST_FILE" --debug 2>&1 | tee "$DEBUG_LOG")
  result=$(echo "$output" | tail -n 1 | tr -d '\n')
  if [[ "$result" != "90" ]]; then
    echo "    Expected: '90'"
    echo "    Got:      '$result'"
    echo "    Raw:      $(echo -n "$result" | xxd)"
    return 1
  fi
}

test_oversized_slice() {
  write_file "abcdefghij"
  output=$("$SLICE" --start 0 --size 100 --file "$TEST_FILE" --debug 2>&1 | tee "$DEBUG_LOG")
  result=$(echo "$output" | tail -n 1)
  [[ "$result" == "abcdefghij" ]]
}

test_missing_file() {
  ! "$SLICE" --start 0 --size 10 --file "nonexistent.txt" --debug > /dev/null 2> "$ERR_FILE"
  grep -qi "error" "$ERR_FILE"
}

test_missing_arguments() {
  ! "$SLICE" --start 0 --size 10 --debug > /dev/null 2> "$ERR_FILE"
  grep -qi "error" "$ERR_FILE"
}

run_test test_basic_slice
run_test test_start_at_zero
run_test test_slice_at_eof
run_test test_oversized_slice
run_test test_missing_file
run_test test_missing_arguments

rm -f "$TEST_FILE" "$ERR_FILE" "$DEBUG_LOG"

echo "PASS"
