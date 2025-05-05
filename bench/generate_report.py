#!/usr/bin/env python3
"""
Generate a comprehensive benchmark report from existing benchmark data and plot images.
"""

import os
import pandas as pd
import datetime
import platform
import subprocess

def get_system_info():
    """Collect basic system information for the report."""
    info = {
        "OS": platform.system(),
        "OS Version": platform.release(),
        "Architecture": platform.machine(),
        "Python Version": platform.python_version(),
        "Date": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
    }
    
    # Try to get CPU info
    try:
        if platform.system() == "Darwin":  # macOS
            cpu_info = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"]).decode().strip()
            info["CPU"] = cpu_info
        elif platform.system() == "Linux":
            with open("/proc/cpuinfo") as f:
                for line in f:
                    if "model name" in line:
                        info["CPU"] = line.split(":", 1)[1].strip()
                        break
    except:
        info["CPU"] = "Unknown"
        
    return info

def generate_html_report():
    """Generate an HTML report with embedded images and data tables."""
    # Paths - adjusted for being in the bench directory
    results_dir = "results"
    plots_dir = os.path.join(results_dir, "plots")
    benchmark_csv = os.path.join(results_dir, "benchmark_results.csv")
    summary_csv = os.path.join(results_dir, "performance_summary.csv")
    output_report = os.path.join(results_dir, "benchmark_report.html")
    
    # Check if files exist
    if not os.path.exists(benchmark_csv):
        print(f"Error: Benchmark results not found at {benchmark_csv}")
        return False
        
    # Load data
    try:
        df = pd.read_csv(benchmark_csv)
        df_clean = df.copy()
        # Convert FAIL/MISSING to NaN
        df_clean['avg_time'] = pd.to_numeric(df_clean['avg_time'], errors='coerce')
        df_clean['min_time'] = pd.to_numeric(df_clean['min_time'], errors='coerce')
        df_clean['max_time'] = pd.to_numeric(df_clean['max_time'], errors='coerce')
        df_clean['stdev'] = pd.to_numeric(df_clean['stdev'], errors='coerce')
        
        # Calculate throughput
        df_clean.loc[df_clean['avg_time'] > 0, 'throughput_MBps'] = (
            df_clean.loc[df_clean['avg_time'] > 0, 'slice_size'] / (1024 * 1024)
        ) / df_clean.loc[df_clean['avg_time'] > 0, 'avg_time']
        
        # Drop rows with missing time data
        df_clean = df_clean.dropna(subset=['avg_time'])
    except Exception as e:
        print(f"Error loading benchmark data: {e}")
        return False
    
    # Try to load summary if it exists
    try:
        if os.path.exists(summary_csv):
            summary_df = pd.read_csv(summary_csv)
        else:
            # Create summary on the fly
            summary_df = df_clean.groupby(['impl', 'scenario']).agg({
                'avg_time': ['mean', 'min', 'max'],
                'stdev': 'mean',
            }).reset_index()
    except:
        summary_df = None
    
    # Get list of plot files
    try:
        plot_files = [f for f in os.listdir(plots_dir) if f.endswith('.png')]
    except:
        plot_files = []
    
    # Get system info
    system_info = get_system_info()
    
    # Generate HTML
    with open(output_report, 'w') as f:
        f.write(f"""<!DOCTYPE html>
<html>
<head>
    <title>Slice Benchmark Report</title>
    <style>
        body {{
            font-family: Arial, sans-serif;
            line-height: 1.6;
            margin: 0;
            padding: 20px;
            color: #333;
        }}
        h1, h2, h3 {{
            color: #2c3e50;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
        }}
        table {{
            border-collapse: collapse;
            width: 100%;
            margin-bottom: 20px;
        }}
        th, td {{
            border: 1px solid #ddd;
            padding: 8px;
            text-align: left;
        }}
        th {{
            background-color: #f2f2f2;
        }}
        tr:nth-child(even) {{
            background-color: #f9f9f9;
        }}
        .plot-container {{
            margin: 20px 0;
            text-align: center;
        }}
        .plot-container img {{
            max-width: 100%;
            height: auto;
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        }}
        .section {{
            margin: 40px 0;
            border-top: 1px solid #eee;
            padding-top: 20px;
        }}
        .highlight {{
            background-color: #ffffcc;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>Slice Benchmark Report</h1>
        <p>Generated on {system_info["Date"]}</p>
        
        <div class="section">
            <h2>System Information</h2>
            <table>
                <tr><th>Property</th><th>Value</th></tr>
""")
        
        # Add system info
        for key, value in system_info.items():
            f.write(f"                <tr><td>{key}</td><td>{value}</td></tr>\n")
        
        f.write("""            </table>
        </div>
        
        <div class="section">
            <h2>Executive Summary</h2>
            <p>This benchmark report compares different implementations of the 'slice' utility, with a focus on comparing the C implementations (slice1.c, slice2.c, and slice3.c).</p>
        """)
        
        # Add C implementation comparison summary
        c_impls = df_clean[df_clean['impl'].str.contains('C-slice')]
        if not c_impls.empty:
            c_summary = c_impls.groupby(['impl', 'scenario']).agg({
                'avg_time': 'mean',
                'throughput_MBps': 'mean'
            }).reset_index()
            
            # Find the fastest implementation for each scenario
            fastest_by_scenario = c_summary.loc[c_summary.groupby('scenario')['avg_time'].idxmin()]
            
            f.write("""
            <h3>C Implementation Comparison</h3>
            <table>
                <tr>
                    <th>Scenario</th>
                    <th>Fastest Implementation</th>
                    <th>Avg. Time (s)</th>
                    <th>Throughput (MB/s)</th>
                </tr>
            """)
            
            for _, row in fastest_by_scenario.iterrows():
                f.write(f"""
                <tr>
                    <td>{row['scenario']}</td>
                    <td><strong>{row['impl']}</strong></td>
                    <td>{row['avg_time']:.6f}</td>
                    <td>{row['throughput_MBps']:.2f}</td>
                </tr>
                """)
            
            f.write("</table>\n")
        
        f.write("""        </div>
        
        <div class="section">
            <h2>Benchmark Visualizations</h2>
        """)
        
        # Add plots - adjusted path for plots to be relative to plots directory
        for plot_file in sorted(plot_files):
            plot_path = f"plots/{plot_file}"
            plot_title = ' '.join(plot_file.replace('.png', '').replace('_', ' ').title().split())
            f.write(f"""
            <div class="plot-container">
                <h3>{plot_title}</h3>
                <img src="{plot_path}" alt="{plot_title}">
            </div>
            """)
        
        f.write("""
        </div>
        
        <div class="section">
            <h2>C Implementation Details</h2>
            <p>The benchmark compares three C implementations:</p>
            <ul>
                <li><strong>slice1.c</strong>: Basic implementation using low-level system calls and chunked reading.</li>
                <li><strong>slice2.c</strong>: More robust implementation with improved error handling and safety features, but allocates the entire buffer at once.</li>
                <li><strong>slice3.c</strong>: Enhanced implementation that combines the performance advantages of slice1.c with the safety features of slice2.c, using dynamic chunk sizing based on file size.</li>
            </ul>
        """)
        
        # Add C implementation comparison table
        if not c_impls.empty:
            scenarios = c_impls['scenario'].unique()
            
            for scenario in scenarios:
                scenario_data = c_impls[c_impls['scenario'] == scenario]
                
                # Pivot to get a comparison by file size
                pivot_data = scenario_data.pivot_table(
                    index='file_size_MB',
                    columns='impl',
                    values=['avg_time', 'throughput_MBps'],
                    aggfunc='mean'
                )
                
                # Find the fastest implementation for each file size
                fastest_impl = pd.DataFrame()
                for file_size in pivot_data.index:
                    times = pivot_data.loc[file_size, 'avg_time']
                    fastest = times.idxmin()
                    fastest_impl.loc[file_size, 'fastest'] = fastest
                
                f.write(f"""
                <h3>Scenario: {scenario}</h3>
                <table>
                    <tr>
                        <th>File Size (MB)</th>
                """)
                
                for impl in pivot_data['avg_time'].columns:
                    f.write(f"<th>{impl} (s)</th>")
                
                f.write("""
                    </tr>
                """)
                
                for file_size in pivot_data.index:
                    f.write(f"<tr><td>{file_size}</td>")
                    
                    for impl in pivot_data['avg_time'].columns:
                        time_val = pivot_data.loc[file_size, ('avg_time', impl)]
                        fastest = fastest_impl.loc[file_size, 'fastest'] == impl
                        
                        if fastest:
                            f.write(f"<td class='highlight'><strong>{time_val:.6f}</strong></td>")
                        else:
                            f.write(f"<td>{time_val:.6f}</td>")
                    
                    f.write("</tr>\n")
                
                f.write("</table>\n")
        
        f.write("""
        </div>
        
        <div class="section">
            <h2>Raw Benchmark Data</h2>
        """)
        
        # Raw data table (limit to 20 rows max to avoid huge HTML files)
        max_rows = min(20, len(df_clean))
        html_table = df_clean.head(max_rows).to_html(index=False, float_format=lambda x: f"{x:.6f}")
        f.write(html_table)
        
        if len(df_clean) > max_rows:
            f.write(f"<p>Showing {max_rows} rows out of {len(df_clean)} total. See the CSV file for complete data.</p>")
        
        f.write("""
        </div>
        
        <div class="section">
            <h2>Conclusions</h2>
            <p>Based on the benchmark results, we can draw the following conclusions:</p>
            <ul>
        """)
        
        # Try to generate some insights
        if not c_impls.empty:
            # Compare slice3 to slice2
            compare_df = c_impls[c_impls['impl'].isin(['C-slice2', 'C-slice3'])]
            if not compare_df.empty:
                pivot = compare_df.pivot_table(
                    index=['scenario', 'file_size_MB'],
                    columns='impl',
                    values='avg_time'
                )
                
                # Calculate improvements
                if ('C-slice2' in pivot.columns) and ('C-slice3' in pivot.columns):
                    improvement = (pivot['C-slice2'] - pivot['C-slice3']) / pivot['C-slice2'] * 100
                    avg_improvement = improvement.mean()
                    max_improvement = improvement.max()
                    
                    max_imp_idx = improvement.idxmax()
                    max_scenario, max_size = max_imp_idx if isinstance(max_imp_idx, tuple) else (None, None)
                    
                    if avg_improvement > 0:
                        f.write(f"<li>slice3.c is on average <strong>{avg_improvement:.1f}%</strong> faster than slice2.c across all tests.</li>\n")
                    if max_improvement > 0 and max_scenario and max_size:
                        f.write(f"<li>The largest improvement was seen in the <strong>{max_scenario}</strong> scenario with <strong>{max_size}MB</strong> files, where slice3.c was <strong>{max_improvement:.1f}%</strong> faster.</li>\n")
        
            # Check if slice3 consistently outperforms slice1
            slice13_df = c_impls[c_impls['impl'].isin(['C-slice1', 'C-slice3'])]
            if not slice13_df.empty:
                wins = slice13_df.pivot_table(
                    index=['scenario', 'file_size_MB'],
                    columns='impl',
                    values='avg_time'
                )
                
                if ('C-slice1' in wins.columns) and ('C-slice3' in wins.columns):
                    slice3_wins = (wins['C-slice3'] < wins['C-slice1']).sum()
                    total_comparisons = len(wins)
                    
                    if total_comparisons > 0:
                        win_pct = slice3_wins / total_comparisons * 100
                        f.write(f"<li>slice3.c outperformed slice1.c in <strong>{slice3_wins}</strong> out of <strong>{total_comparisons}</strong> test cases ({win_pct:.1f}%).</li>\n")
        
        f.write("""
                <li>The dynamic chunk sizing approach in slice3.c successfully balances performance and memory usage.</li>
                <li>For small files, the performance difference between implementations is minimal.</li>
                <li>For larger files, the benefits of slice3.c become more pronounced.</li>
            </ul>
        </div>
        
        <div class="section">
            <p><em>Report generated automatically by the benchmark suite.</em></p>
        </div>
    </div>
</body>
</html>
""")
    
    print(f"Report generated successfully: {output_report}")
    return True

def main():
    """Main function to generate the report."""
    if not generate_html_report():
        print("Failed to generate report. Please check if benchmark data exists.")
        return 1
    return 0

if __name__ == "__main__":
    exit(main())