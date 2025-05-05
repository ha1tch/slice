#!/bin/bash
set -euo pipefail

# --- Configuration ---
IMPLEMENTATIONS=(
  "C-slice1:./c/slice1"
  "C-slice2:./c/slice2"
  "C-slice3:./c/slice3"
  "Go:./go/slice"
  "Python:python3 ./slice.py"
  "Ruby:ruby ./slice.rb"
  "Shell:./slice"
)

# Test scenarios - [name, start, size, description]
SCENARIOS=(
  "small_start:0:1048576:Read 1MB from beginning"
  "large_chunk:0:20971520:Read 20MB from beginning"
  "full_file:0:file_size:Read entire file"
)

# Input file sizes in MB
FILE_SIZES=(1 5 10 20 50 100)

# Number of iterations to run for more accurate timing
ITERATIONS=5

# Directory setup
INPUT_DIR="bench/sample_inputs"
RESULTS_DIR="bench/results"
mkdir -p "$INPUT_DIR" "$RESULTS_DIR"

# --- Helper Functions ---
# Print message with timestamp
log() {
  echo "[$(date +%H:%M:%S)] $1"
}

# Get high-precision timing using Python
# This ensures consistent timing across platforms
get_time_python() {
  local cmd="$1"
  local iterations="$2"
  python3 -c "
import subprocess
import time
import statistics

times = []
for i in range($iterations):
    start = time.perf_counter()
    result = subprocess.run('$cmd', shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    end = time.perf_counter()
    if result.returncode == 0:
        times.append(end - start)
    else:
        # Return error code as negative time - this signals an error
        print('-' + str(result.returncode))
        exit(0)

# Calculate statistics
if times:
    avg = statistics.mean(times)
    if len(times) > 1:
        stdev = statistics.stdev(times)
        min_time = min(times)
        max_time = max(times)
        print(f'{avg:.6f},{min_time:.6f},{max_time:.6f},{stdev:.6f}')
    else:
        # Only one successful run
        print(f'{avg:.6f},{avg:.6f},{avg:.6f},0.000000')
else:
    print('FAIL,FAIL,FAIL,FAIL')
"
}

# --- Main Benchmark Function ---
run_benchmarks() {
  local results_file="$RESULTS_DIR/benchmark_results.csv"
  
  # Initialize results file with headers
  echo "impl,scenario,file_size_MB,start_offset,slice_size,iterations,avg_time,min_time,max_time,stdev" > "$results_file"
  
  log "Running benchmarks with $ITERATIONS iterations per test..."
  for size in "${FILE_SIZES[@]}"; do
    padded=$(printf "%03d" "$size")
    input_file="$INPUT_DIR/input_${padded}MB.txt"
    
    if [[ ! -f "$input_file" ]]; then
      log "Warning: Input file $input_file not found. Skipping."
      continue
    fi
    
    file_size=$(stat -f%z "$input_file" 2>/dev/null || stat -c%s "$input_file" 2>/dev/null)
    log "File: ${input_file} (${size}MB, ${file_size} bytes)"
    
    # For each test scenario
    for scenario_def in "${SCENARIOS[@]}"; do
      IFS=":" read -r scenario_name start_offset size_param description <<< "$scenario_def"
      
      # Replace file_size with actual file size if needed
      if [[ "$size_param" == "file_size" ]]; then
        actual_size="$file_size"
      else
        actual_size="$size_param"
        
        # Skip scenarios that request more data than available
        if (( actual_size > file_size )); then
          log "  Skipping scenario $scenario_name - requested size $actual_size exceeds file size $file_size"
          continue
        fi
      fi
      
      log "  Scenario: $scenario_name ($description)"
      log "    Start: $start_offset, Size: $actual_size bytes"
      
      # For each implementation
      for entry in "${IMPLEMENTATIONS[@]}"; do
        IFS=":" read -r impl_name cmd <<< "$entry"
        
        # Check if executable exists
        executable=$(echo "$cmd" | awk '{print $1}')
        if [[ ! -x "$executable" && ! "$executable" =~ ^(python3|ruby)$ ]]; then
          log "    Skipping $impl_name - executable not found: $executable"
          echo "$impl_name,$scenario_name,$size,$start_offset,$actual_size,$ITERATIONS,MISSING,MISSING,MISSING,MISSING" >> "$results_file"
          continue
        fi
        
        log "    Testing $impl_name ($ITERATIONS iterations)..."
        
        # Command with proper quoting for Python subprocess
        full_cmd="${cmd} --start ${start_offset} --size ${actual_size} --file ${input_file}"
        
        # Get timing results
        result=$(get_time_python "$full_cmd" "$ITERATIONS")
        
        # Check for errors (negative return values)
        if [[ "$result" == -* ]]; then
          error_code="${result#-}"
          log "      Failed with error code $error_code"
          echo "$impl_name,$scenario_name,$size,$start_offset,$actual_size,$ITERATIONS,FAIL,FAIL,FAIL,FAIL" >> "$results_file"
          continue
        fi
        
        # Parse timing results
        IFS="," read -r avg_time min_time max_time stdev <<< "$result"
        
        # Convert to milliseconds for display
        avg_ms=$(echo "$avg_time * 1000" | bc)
        min_ms=$(echo "$min_time * 1000" | bc)
        max_ms=$(echo "$max_time * 1000" | bc)
        stdev_ms=$(echo "$stdev * 1000" | bc)
        
        log "      Time: ${avg_ms}ms (min: ${min_ms}ms, max: ${max_ms}ms, stdev: ${stdev_ms}ms)"
        
        # Record results
        echo "$impl_name,$scenario_name,$size,$start_offset,$actual_size,$ITERATIONS,$avg_time,$min_time,$max_time,$stdev" >> "$results_file"
      done
    done
  done
  
  log "Benchmarks complete. Results saved to $results_file"
}

# --- Generate Analysis Scripts ---
generate_analysis_scripts() {
  log "Generating analysis scripts..."
  
  # Create Python script for generating plots
  cat > "$RESULTS_DIR/analyze_results.py" << 'EOF'
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import numpy as np

# Load results
results_file = os.path.join("bench", "results", "benchmark_results.csv")
if not os.path.exists(results_file):
    print(f"Error: Results file not found: {results_file}")
    exit(1)

df = pd.read_csv(results_file)

# Clean data
df = df.replace("FAIL", float('nan'))
df = df.replace("MISSING", float('nan'))
df['avg_time'] = pd.to_numeric(df['avg_time'], errors='coerce')
df['min_time'] = pd.to_numeric(df['min_time'], errors='coerce')
df['max_time'] = pd.to_numeric(df['max_time'], errors='coerce')
df['stdev'] = pd.to_numeric(df['stdev'], errors='coerce')

# Filter out rows with nan values
df_clean = df.dropna(subset=['avg_time'])

# Calculate throughput
df_clean['throughput_MBps'] = (df_clean['slice_size'] / (1024 * 1024)) / df_clean['avg_time']

# Create output directory
os.makedirs(os.path.join("bench", "results", "plots"), exist_ok=True)

# Set plotting style
sns.set(style="whitegrid", palette="colorblind", font_scale=1.2)

# --- Plot 1: Execution time by file size for each implementation (scenario: full_file) ---
plt.figure(figsize=(12, 7))
full_file_df = df_clean[df_clean['scenario'] == 'full_file']
if not full_file_df.empty:
    ax = sns.lineplot(
        data=full_file_df,
        x="file_size_MB",
        y="avg_time",
        hue="impl",
        marker="o",
        linewidth=2.5
    )
    plt.title("Slice Performance - Reading Entire File", fontsize=16)
    plt.xlabel("File Size (MB)", fontsize=14)
    plt.ylabel("Execution Time (seconds)", fontsize=14)
    plt.yscale("log")  # Log scale for better visibility
    plt.xticks(sorted(df_clean['file_size_MB'].unique()))
    plt.legend(title="Implementation", fontsize=12, title_fontsize=13)
    plt.tight_layout()
    plt.savefig(os.path.join("bench", "results", "plots", "time_by_file_size.png"))
plt.close()

# --- Plot 2: Small slice performance (1MB reads) ---
plt.figure(figsize=(12, 7))
small_slice_df = df_clean[df_clean['scenario'] == 'small_start']
if not small_slice_df.empty:
    ax = sns.lineplot(
        data=small_slice_df,
        x="file_size_MB",
        y="avg_time",
        hue="impl",
        marker="o",
        linewidth=2.5
    )
    plt.title("Performance Reading 1MB Slice from Start", fontsize=16)
    plt.xlabel("Source File Size (MB)", fontsize=14)
    plt.ylabel("Execution Time (seconds)", fontsize=14)
    plt.yscale("log")
    plt.xticks(sorted(df_clean['file_size_MB'].unique()))
    plt.legend(title="Implementation", fontsize=12, title_fontsize=13)
    plt.tight_layout()
    plt.savefig(os.path.join("bench", "results", "plots", "small_slice_performance.png"))
plt.close()

# --- Plot 3: C implementations comparison ---
plt.figure(figsize=(14, 8))
c_impls_df = df_clean[df_clean['impl'].str.contains('C-slice')]
if not c_impls_df.empty:
    # Create a separate plot for each scenario
    for scenario, scenario_df in c_impls_df.groupby('scenario'):
        plt.figure(figsize=(14, 8))
        ax = sns.lineplot(
            data=scenario_df,
            x="file_size_MB", 
            y="avg_time",
            hue="impl",
            marker="o",
            linewidth=2.5
        )
        plt.title(f"C Implementation Comparison - {scenario}", fontsize=16)
        plt.xlabel("File Size (MB)", fontsize=14)
        plt.ylabel("Execution Time (seconds)", fontsize=14)
        plt.yscale("log")
        plt.legend(title="Implementation", fontsize=12, title_fontsize=13)
        plt.tight_layout()
        plt.savefig(os.path.join("bench", "results", "plots", f"c_implementation_{scenario}.png"))
        plt.close()
plt.close()

# --- Plot 4: Throughput comparison (MB/s) across all implementations for 20MB chunk ---
plt.figure(figsize=(14, 8))
large_chunk_df = df_clean[df_clean['scenario'] == 'large_chunk']
if not large_chunk_df.empty:
    large_chunk_df = large_chunk_df.sort_values('throughput_MBps', ascending=False)
    ax = sns.barplot(
        data=large_chunk_df,
        x="impl",
        y="throughput_MBps",
        hue="file_size_MB", 
        palette="viridis",
        errorbar=None
    )
    plt.title("Throughput - 20MB Chunk", fontsize=16)
    plt.xlabel("Implementation", fontsize=14)
    plt.ylabel("Throughput (MB/s)", fontsize=14)
    plt.legend(title="File Size (MB)", fontsize=12, title_fontsize=13)
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig(os.path.join("bench", "results", "plots", "throughput_comparison.png"))
plt.close()

# --- Create a summary table ---
print("\nPerformance Summary:")
print("-" * 80)

# Group by implementation and scenario, and calculate mean metrics
summary = df_clean.groupby(['impl', 'scenario']).agg({
    'avg_time': ['mean', 'min', 'max'],
    'stdev': 'mean',
    'throughput_MBps': ['mean', 'max']
}).reset_index()

# Save summary to CSV
summary.to_csv(os.path.join("bench", "results", "performance_summary.csv"))
print(f"Summary saved to {os.path.join('bench', 'results', 'performance_summary.csv')}")

print("\nC Implementation Comparison:")
print("-" * 80)
c_summary = df_clean[df_clean['impl'].str.contains('C-slice')].groupby(['impl', 'scenario']).agg({
    'avg_time': ['mean', 'min', 'max'],
    'throughput_MBps': ['mean', 'max']
}).reset_index()

print(c_summary)

print("\nAnalysis complete. Plots saved to bench/results/plots/")
EOF

  log "Analysis scripts generated"
}

# --- Main Script Execution ---
main() {
  log "Precision Slice Benchmark Suite"
  log "=============================="
  
  # Check for required tools
  for cmd in python3 bc; do
    if ! command -v "$cmd" &> /dev/null; then
      log "Error: Required command '$cmd' not found"
      exit 1
    fi
  done
  
  # Create results directory
  mkdir -p "$RESULTS_DIR/plots"
  
  run_benchmarks
  generate_analysis_scripts
  
  log "Running analysis scripts..."
  python3 "$RESULTS_DIR/analyze_results.py"
  
  log "Benchmark suite completed successfully"
}

# Run the main function
main