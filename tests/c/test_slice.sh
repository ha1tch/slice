#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SLICE_SRC_DIR="$SCRIPT_DIR/../../c"
SLICE_BIN="$SLICE_SRC_DIR/slice"
TEST_FILE="$SCRIPT_DIR/test_input.txt"
OUT_FILE="$SCRIPT_DIR/output.txt"
EXPECT_FILE="$SCRIPT_DIR/expected.txt"
DEBUG_FILE="$SCRIPT_DIR/debug.txt"
RAG_FILE="$SCRIPT_DIR/fakedown_input.md"
KEEP_FAKE_FILE=1  # Keep the generated fakedown file for inspection

debug() {
  echo "DEBUG: $*"
}

compile_c_slice() {
  debug "Compiling C slice binary..."
  (cd "$SLICE_SRC_DIR" && make -s)
  if [[ ! -x "$SLICE_BIN" ]]; then
    echo "ERROR: $SLICE_BIN not found or not executable"
    exit 1
  fi
}

write_input() {
  cat <<EOF > "$TEST_FILE"
Line 1
Line 2
Line 3
EOF
}

run_test() {
  local name=$1
  local start=$2
  local size=$3
  local expected=$4
  local extra_flags=$5

  echo "=== RUN   $name"

  printf "%s" "$expected" > "$EXPECT_FILE"
  "$SLICE_BIN" --start "$start" --size "$size" --file "$TEST_FILE" $extra_flags > "$OUT_FILE" 2> "$DEBUG_FILE"

  if cmp -s "$OUT_FILE" "$EXPECT_FILE"; then
    echo "--- PASS: $name"
  else
    echo "--- FAIL: $name"
    echo "    Expected bytes:"
    xxd "$EXPECT_FILE"
    echo "    Got bytes:"
    xxd "$OUT_FILE"
    echo "    Debug output:"
    cat "$DEBUG_FILE"
    exit 1
  fi

  rm -f "$OUT_FILE" "$EXPECT_FILE" "$DEBUG_FILE"
}

run_test_expect_error() {
  local name=$1
  shift
  echo "=== RUN   $name"
  if "$SLICE_BIN" "$@" > "$OUT_FILE" 2>&1; then
    echo "--- FAIL: $name (expected error, got success)"
    cat "$OUT_FILE"
    exit 1
  else
    echo "--- PASS: $name"
  fi
  rm -f "$OUT_FILE"
}

# === Setup and core tests
debug "Working directory: $SCRIPT_DIR"
compile_c_slice
write_input

run_test test_basic_slice 7 7 $'Line 2\n'
run_test test_start_at_zero 0 7 $'Line 1\n'
run_test test_slice_at_eof 14 7 $'Line 3\n'
run_test test_oversized_slice 0 100 $'Line 1\nLine 2\nLine 3\n'
run_test test_start_exactly_at_eof 21 5 ""
run_test test_start_beyond_eof 30 10 ""
run_test test_partial_read_near_eof 19 5 $'3\n'
run_test test_first_byte_only 0 1 "L"
run_test test_last_byte_only 20 1 $'\n'
run_test test_full_file 0 21 $'Line 1\nLine 2\nLine 3\n'
run_test_expect_error test_size_zero --start 0 --size 0 --file "$TEST_FILE"
run_test_expect_error test_negative_start --start -1 --size 5 --file "$TEST_FILE"
run_test_expect_error test_negative_size --start 0 --size -5 --file "$TEST_FILE"

# === Fakedown generator
latin_roots=("lorem" "ipsum" "dolor" "sit" "amet" "elit" "sed" "do" "eiusmod" "tempor" "incididunt" "labore" "et" "magna" "aliqua" "ut" "enim" "ad" "minim" "veniam")

generate_random_word() {
  local root=${latin_roots[$((RANDOM % ${#latin_roots[@]}))]}
  local sublen=$((RANDOM % ${#root} + 1))
  echo "$root" | fold -w1 | shuf | tr -d '\n' | cut -c1-"$sublen"
}

generate_paragraph() {
  local wc=$((RANDOM % 250 + 50))
  local p=""
  for _ in $(seq 1 $wc); do
    p+=$(generate_random_word) 
    p+=" "
  done
  echo "$p"
}

generate_fakedown() {
  > "$RAG_FILE"
  for i in $(seq 1 $((RANDOM % 20 + 50))); do
    r=$((RANDOM % 20))
    case $r in
      0) echo "# $(generate_random_word | tr '[:lower:]' '[:upper:]') Section Title" >> "$RAG_FILE" ;;
      1) echo "## $(generate_random_word | tr '[:lower:]' '[:upper:]') Subsection" >> "$RAG_FILE" ;;
      2) echo "### $(generate_random_word | tr '[:lower:]' '[:upper:]') Sub-subsection" >> "$RAG_FILE" ;;
      3|4)
        for j in $(seq 1 $((RANDOM % 4 + 2))); do
          echo "- $(generate_random_word) $(generate_paragraph | fold -s -w 60 | head -n1)" >> "$RAG_FILE"
        done ;;
      5)
        echo "> $(generate_paragraph | fold -s -w 70 | head -n1)" >> "$RAG_FILE" ;;
      *)
        generate_paragraph | fmt -w $((RANDOM % 40 + 80)) >> "$RAG_FILE" ;;
    esac
    echo >> "$RAG_FILE"
  done
}

# === RAG-style chunking overlap test
echo "=== RUN   test_rag_chunking_overlap"
CHUNK_SIZE=512
OVERLAP=128
TMPDIR="$SCRIPT_DIR/rag_chunks"
mkdir -p "$TMPDIR"
generate_fakedown
FILE_SIZE=$(wc -c < "$RAG_FILE")
CHUNK_OUTPUT="$SCRIPT_DIR/rag_merged.txt"
: > "$CHUNK_OUTPUT"

i=0
while true; do
  START=$((i * (CHUNK_SIZE - OVERLAP)))
  [ $START -ge $FILE_SIZE ] && break
  "$SLICE_BIN" --start "$START" --size "$CHUNK_SIZE" --file "$RAG_FILE" --full-lines-only > "$TMPDIR/chunk_$i.txt"
  cat "$TMPDIR/chunk_$i.txt" >> "$CHUNK_OUTPUT"
  i=$((i + 1))
done

# === Round-trip reconstruction test
echo "=== RUN   test_rag_roundtrip_reconstruction"
DEDUPED="$SCRIPT_DIR/rag_deduped.txt"
RECONSTRUCTED="$SCRIPT_DIR/rag_original_cleaned.txt"

sort "$CHUNK_OUTPUT" | uniq > "$DEDUPED"
sort "$RAG_FILE" | uniq > "$RECONSTRUCTED"

if diff -u "$RECONSTRUCTED" "$DEDUPED" > "$SCRIPT_DIR/rag_diff.txt"; then
  echo "--- PASS: test_rag_roundtrip_reconstruction"
else
  echo "--- FAIL: test_rag_roundtrip_reconstruction"
  cat "$SCRIPT_DIR/rag_diff.txt"
  exit 1
fi

# === Cleanup
rm -f "$CHUNK_OUTPUT" "$DEDUPED" "$RECONSTRUCTED"
rm -rf "$TMPDIR" "$SCRIPT_DIR/rag_diff.txt"
rm -f "$TEST_FILE"
echo "PASS"
