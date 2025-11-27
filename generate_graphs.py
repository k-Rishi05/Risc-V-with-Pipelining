#!/usr/bin/env python3
"""
Branch Predictor Performance Visualization
Runs dynamic_bp.sh on test programs and generates comparison graphs
"""

import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime
import sys
import os

# Configuration
TEST_PROGRAMS = [
    'examples/matrix_mult.s',
    'examples/binary_search.s', 
    'examples/quicksort.s'
]

PROGRAM_NAMES = [
    'Matrix Multiplication',
    'Binary Search',
    'Quicksort Partition'
]

PREDICTORS = [
    'Static',
    '1-Bit',
    '2-Bit',
    'Perceptron',
    'Gshare'
]

PREDICTOR_COLORS = [
    '#e74c3c',  # Red - Static
    '#3498db',  # Blue - 1-Bit
    '#2ecc71',  # Green - 2-Bit
    '#9b59b6',  # Purple - Perceptron
    '#f39c12'   # Orange - Gshare
]

def run_benchmark(asm_file):
    """Run dynamic_bp.sh and parse results"""
    print(f"Running benchmark: {asm_file}...")
    
    try:
        result = subprocess.run(
            ['./dynamic_bp.sh', asm_file],
            capture_output=True,
            text=True,
            timeout=60
        )
        
        if result.returncode != 0:
            print(f"Warning: Script returned code {result.returncode}")
        
        output = result.stdout
        
        # Parse results - look for the table rows
        results = {
            'cycles': [],
            'instructions': [],
            'cpi': [],
            'ipc': [],
            'mispredictions': []
        }
        
        # Extract data from each predictor line (modes 5-9)
        lines = output.split('\n')
        for line in lines:
            # Match lines like: "5  Static BP (Always Not-Taken)  63188   56926   1.11    0.90      6258"
            match = re.search(r'^[5-9]\s+\S.*?\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)', line)
            if match:
                results['cycles'].append(int(match.group(1)))
                results['instructions'].append(int(match.group(2)))
                results['cpi'].append(float(match.group(3)))
                results['ipc'].append(float(match.group(4)))
                results['mispredictions'].append(int(match.group(5)))
        
        if len(results['mispredictions']) != 5:
            print(f"Warning: Expected 5 results, got {len(results['mispredictions'])}")
            print(f"Output:\n{output}")
        
        return results
        
    except subprocess.TimeoutExpired:
        print(f"Error: Benchmark timed out for {asm_file}")
        return None
    except Exception as e:
        print(f"Error running benchmark: {e}")
        return None

def create_grouped_bar_chart(data, metric, ylabel, title, filename):
    """Create a grouped bar chart"""
    
    fig, ax = plt.subplots(figsize=(12, 7))
    
    x = np.arange(len(PROGRAM_NAMES))
    width = 0.15  # Width of each bar
    
    # Plot bars for each predictor
    for i, predictor in enumerate(PREDICTORS):
        values = [data[prog][metric][i] for prog in range(len(PROGRAM_NAMES))]
        offset = width * (i - 2)  # Center the groups
        bars = ax.bar(x + offset, values, width, 
                     label=predictor, 
                     color=PREDICTOR_COLORS[i],
                     edgecolor='black',
                     linewidth=0.5)
        
        # Add value labels on top of bars
        for bar in bars:
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{height:.0f}' if metric == 'mispredictions' else f'{height:.2f}',
                   ha='center', va='bottom', fontsize=8, fontweight='bold')
    
    # Customize chart
    ax.set_xlabel('Test Programs', fontsize=12, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=12, fontweight='bold')
    ax.set_title(title, fontsize=14, fontweight='bold', pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(PROGRAM_NAMES, fontsize=11)
    ax.legend(title='Predictor Type', fontsize=10, title_fontsize=11, 
             loc='upper right', framealpha=0.9)
    ax.grid(axis='y', alpha=0.3, linestyle='--')
    
    # Add timestamp
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fig.text(0.99, 0.01, f'Generated: {timestamp}', 
            ha='right', fontsize=8, style='italic', color='gray')
    
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {filename}")
    plt.close()

def create_combined_chart(data, filename):
    """Create a figure with both mispredictions and CPI"""
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7))
    
    x = np.arange(len(PROGRAM_NAMES))
    width = 0.15
    
    # Plot 1: Mispredictions
    for i, predictor in enumerate(PREDICTORS):
        values = [data[prog]['mispredictions'][i] for prog in range(len(PROGRAM_NAMES))]
        offset = width * (i - 2)
        bars = ax1.bar(x + offset, values, width, 
                      label=predictor, 
                      color=PREDICTOR_COLORS[i],
                      edgecolor='black',
                      linewidth=0.5)
        
        for bar in bars:
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height,
                    f'{height:.0f}',
                    ha='center', va='bottom', fontsize=7, fontweight='bold')
    
    ax1.set_xlabel('Test Programs', fontsize=11, fontweight='bold')
    ax1.set_ylabel('Mispredictions', fontsize=11, fontweight='bold')
    ax1.set_title('Branch Mispredictions Comparison', fontsize=13, fontweight='bold')
    ax1.set_xticks(x)
    ax1.set_xticklabels(PROGRAM_NAMES, fontsize=10)
    ax1.legend(fontsize=9, loc='upper right', framealpha=0.9)
    ax1.grid(axis='y', alpha=0.3, linestyle='--')
    
    # Plot 2: CPI
    for i, predictor in enumerate(PREDICTORS):
        values = [data[prog]['cpi'][i] for prog in range(len(PROGRAM_NAMES))]
        offset = width * (i - 2)
        bars = ax2.bar(x + offset, values, width, 
                      label=predictor, 
                      color=PREDICTOR_COLORS[i],
                      edgecolor='black',
                      linewidth=0.5)
        
        for bar in bars:
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height,
                    f'{height:.2f}',
                    ha='center', va='bottom', fontsize=7, fontweight='bold')
    
    ax2.set_xlabel('Test Programs', fontsize=11, fontweight='bold')
    ax2.set_ylabel('CPI (Cycles Per Instruction)', fontsize=11, fontweight='bold')
    ax2.set_title('CPI Comparison (Lower is Better)', fontsize=13, fontweight='bold')
    ax2.set_xticks(x)
    ax2.set_xticklabels(PROGRAM_NAMES, fontsize=10)
    ax2.legend(fontsize=9, loc='upper right', framealpha=0.9)
    ax2.grid(axis='y', alpha=0.3, linestyle='--')
    
    # Add overall title
    fig.suptitle('RISC-V Branch Predictor Performance Analysis', 
                fontsize=16, fontweight='bold', y=0.98)
    
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fig.text(0.99, 0.01, f'Generated: {timestamp}', 
            ha='right', fontsize=8, style='italic', color='gray')
    
    plt.tight_layout(rect=[0, 0.02, 1, 0.96])
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {filename}")
    plt.close()

def main():
    print("=" * 80)
    print("RISC-V Branch Predictor Performance Graph Generator")
    print("=" * 80)
    print()
    
    # Check if matplotlib is available
    try:
        import matplotlib
        matplotlib.use('Agg')  # Use non-interactive backend
    except ImportError:
        print("Error: matplotlib is required. Install with: pip3 install matplotlib")
        sys.exit(1)
    
    # Check if dynamic_bp.sh exists
    if not os.path.exists('dynamic_bp.sh'):
        print("Error: dynamic_bp.sh not found in current directory")
        sys.exit(1)
    
    # Make sure dynamic_bp.sh is executable
    os.chmod('dynamic_bp.sh', 0o755)
    
    # Collect data from all benchmarks
    all_data = []
    
    for i, test_prog in enumerate(TEST_PROGRAMS):
        if not os.path.exists(test_prog):
            print(f"Warning: Test file {test_prog} not found, skipping...")
            # Use dummy data
            all_data.append({
                'cycles': [0] * 5,
                'instructions': [0] * 5,
                'cpi': [0] * 5,
                'ipc': [0] * 5,
                'mispredictions': [0] * 5
            })
            continue
        
        results = run_benchmark(test_prog)
        if results:
            all_data.append(results)
        else:
            print(f"Failed to get results for {test_prog}")
            sys.exit(1)
    
    print()
    print("=" * 80)
    print("Generating graphs...")
    print("=" * 80)
    
    # Generate output directory
    output_dir = "graphs"
    os.makedirs(output_dir, exist_ok=True)
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Create individual graphs
    create_grouped_bar_chart(
        all_data, 
        'mispredictions',
        'Branch Mispredictions',
        'Branch Predictor Mispredictions Comparison',
        f'{output_dir}/mispredictions_{timestamp}.png'
    )
    
    create_grouped_bar_chart(
        all_data,
        'cpi',
        'CPI (Cycles Per Instruction)',
        'Branch Predictor CPI Comparison (Lower is Better)',
        f'{output_dir}/cpi_{timestamp}.png'
    )
    
    # Create combined chart
    create_combined_chart(
        all_data,
        f'{output_dir}/combined_{timestamp}.png'
    )
    
    print()
    print("=" * 80)
    print(f"✓ All graphs generated successfully in '{output_dir}/' directory")
    print("=" * 80)
    
    # Print summary
    print("\nPerformance Summary:")
    print("-" * 80)
    for i, prog_name in enumerate(PROGRAM_NAMES):
        print(f"\n{prog_name}:")
        mispred = all_data[i]['mispredictions']
        best_idx = mispred.index(min(mispred))
        print(f"  Best Predictor: {PREDICTORS[best_idx]} ({mispred[best_idx]} mispredictions)")
        
        cpi = all_data[i]['cpi']
        best_cpi_idx = cpi.index(min(cpi))
        print(f"  Best CPI: {PREDICTORS[best_cpi_idx]} ({cpi[best_cpi_idx]:.2f})")

if __name__ == "__main__":
    main()
