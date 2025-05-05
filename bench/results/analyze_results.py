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
