#!/usr/bin/env python3
"""
Single Program Branch Predictor Visualization
Runs dynamic_bp.sh on ONE program and generates a comparison bar chart
Usage: ./graph_bp.py <file.s>
"""

import subprocess
import re
import matplotlib.pyplot as plt
import numpy as np
from datetime import datetime
import sys
import os

PREDICTORS = ['Static', '1-Bit', '2-Bit', 'Perceptron', 'Gshare']
PREDICTOR_COLORS = ['#e74c3c', '#3498db', '#2ecc71', '#9b59b6', '#f39c12']

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
        
        output = result.stdout
        
        # Strip ANSI color codes
        ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
        output = ansi_escape.sub('', output)
        
        # Parse results
        results = {
            'cycles': [],
            'instructions': [],
            'cpi': [],
            'ipc': [],
            'mispredictions': []
        }
        
        lines = output.split('\n')
        for line in lines:
            # Match data lines (modes 5-9)
            # Format: "5     Static BP (Always Not-Taken)     42087   34084  1.2348    0.81      7999"
            match = re.search(r'^([5-9])\s+\S.*?\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s*$', line)
            if match:
                results['cycles'].append(int(match.group(2)))
                results['instructions'].append(int(match.group(3)))
                results['cpi'].append(float(match.group(4)))
                results['ipc'].append(float(match.group(5)))
                results['mispredictions'].append(int(match.group(6)))
        
        if len(results['mispredictions']) != 5:
            print(f"Error: Expected 5 results, got {len(results['mispredictions'])}")
            print("Debug: Parsed lines:")
            for line in lines:
                if re.search(r'^[5-9]', line):
                    print(f"  {repr(line)}")
            return None
        
        return results
        
    except Exception as e:
        print(f"Error running benchmark: {e}")
        import traceback
        traceback.print_exc()
        return None

def create_bar_chart(data, program_name, output_file):
    """Create a simple bar chart showing mispredictions only"""
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    x = np.arange(len(PREDICTORS))
    width = 0.6
    
    # Single chart: Mispredictions with blue bars
    bars = ax.bar(x, data['mispredictions'], width, 
                  color='#3498db', edgecolor='black', linewidth=1.5)
    
    ax.set_xlabel('Branch Predictor', fontsize=13, fontweight='bold')
    ax.set_ylabel('Number of Mispredictions', fontsize=13, fontweight='bold')
    ax.set_title(f'Branch Mispredictions Comparison - {program_name}', 
                fontsize=15, fontweight='bold', pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(PREDICTORS, fontsize=12)
    ax.grid(axis='y', alpha=0.3, linestyle='--')
    
    # Add value labels on top of bars
    for bar in bars:
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{int(height)}',
                ha='center', va='bottom', fontsize=11, fontweight='bold')
    
    # Highlight best (lowest mispredictions)
    best_idx = data['mispredictions'].index(min(data['mispredictions']))
    bars[best_idx].set_color('#2ecc71')  # Green for best
    bars[best_idx].set_edgecolor('gold')
    bars[best_idx].set_linewidth(3)
    
    # Add note at bottom
    note = f"Best: {PREDICTORS[best_idx]} with {data['mispredictions'][best_idx]} mispredictions"
    ax.text(0.5, -0.15, note, transform=ax.transAxes,
           ha='center', fontsize=11, style='italic', 
           color='darkgreen', fontweight='bold',
           bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.3))
    
    # Timestamp
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fig.text(0.99, 0.01, f'Generated: {timestamp}',
            ha='right', fontsize=8, style='italic', color='gray')
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Graph saved: {output_file}")
    plt.close()

def main():
    if len(sys.argv) != 2:
        print("Usage: ./graph_bp.py <file.s>")
        print("Example: ./graph_bp.py examples/matrix_mult.s")
        sys.exit(1)
    
    asm_file = sys.argv[1]
    
    # Check if file exists
    if not os.path.exists(asm_file):
        print(f"Error: File '{asm_file}' not found!")
        sys.exit(1)
    
    # Check if dynamic_bp.sh exists
    if not os.path.exists('dynamic_bp.sh'):
        print("Error: dynamic_bp.sh not found in current directory")
        sys.exit(1)
    
    print("=" * 80)
    print("RISC-V Branch Predictor Performance Graph Generator")
    print("=" * 80)
    print()
    
    # Run benchmark
    results = run_benchmark(asm_file)
    
    if not results:
        print("Failed to get benchmark results")
        sys.exit(1)
    
    # Create output filename
    base_name = os.path.basename(asm_file)
    program_name = os.path.splitext(base_name)[0]
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_file = f"graph_{program_name}_{timestamp}.png"
    
    # Generate graph
    print("\nGenerating graph...")
    create_bar_chart(results, program_name, output_file)
    
    # Print summary
    print("\n" + "=" * 80)
    print("Performance Summary:")
    print("-" * 80)
    
    best_mispred_idx = results['mispredictions'].index(min(results['mispredictions']))
    best_cpi_idx = results['cpi'].index(min(results['cpi']))
    
    print(f"Best Mispredictions: {PREDICTORS[best_mispred_idx]} ({results['mispredictions'][best_mispred_idx]})")
    print(f"Best CPI:            {PREDICTORS[best_cpi_idx]} ({results['cpi'][best_cpi_idx]:.3f})")
    print(f"Instructions:        {results['instructions'][0]}")
    
    print("\nAll Predictors:")
    for i, predictor in enumerate(PREDICTORS):
        print(f"  {predictor:12} - Mispred: {results['mispredictions'][i]:4}, CPI: {results['cpi'][i]:.3f}, Cycles: {results['cycles'][i]}")
    
    print("=" * 80)

if __name__ == "__main__":
    main()
