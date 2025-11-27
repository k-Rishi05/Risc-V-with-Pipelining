#!/usr/bin/env python3
"""
Branch Predictor History Length Analysis
Tests Perceptron and Gshare with varying history lengths
Generates graphs showing mispredictions vs history size
"""

import subprocess
import re
import matplotlib.pyplot as plt
import json
import os
from datetime import datetime

# Configuration
TEST_PROGRAM = 'examples/quicksort.s'
HISTORY_SIZES = [1, 2, 4, 8, 16, 32]
CONFIG_FILE = 'include/config.h'

def backup_config():
    """Backup original config file"""
    with open(CONFIG_FILE, 'r') as f:
        return f.read()

def restore_config(original_content):
    """Restore original config file"""
    with open(CONFIG_FILE, 'w') as f:
        f.write(original_content)

def update_history_length(predictor, history_size):
    """Update history length in config.h"""
    with open(CONFIG_FILE, 'r') as f:
        content = f.read()
    
    if predictor == 'perceptron':
        # Update perceptron_history_length
        content = re.sub(
            r'uint32_t perceptron_history_length = \d+;',
            f'uint32_t perceptron_history_length = {history_size};',
            content
        )
    elif predictor == 'gshare':
        # Update gshare_history_length
        content = re.sub(
            r'uint32_t gshare_history_length = \d+;',
            f'uint32_t gshare_history_length = {history_size};',
            content
        )
    
    with open(CONFIG_FILE, 'w') as f:
        f.write(content)

def rebuild_project():
    """Rebuild the project"""
    print("  Rebuilding project...", end=' ', flush=True)
    result = subprocess.run(
        ['make', '-C', 'build', '-j4'],
        capture_output=True,
        text=True
    )
    if result.returncode == 0:
        print("✓")
        return True
    else:
        print("✗")
        print(f"Build error: {result.stderr}")
        return False

def run_benchmark(test_file):
    """Run benchmark and extract statistics for both predictors"""
    result = subprocess.run(
        ['./dynamic_bp.sh', test_file],
        capture_output=True,
        text=True,
        timeout=60
    )
    
    # Strip ANSI codes
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    output = ansi_escape.sub('', result.stdout)
    
    # Parse both mode 8 (Perceptron) and mode 9 (Gshare)
    results = {}
    lines = output.split('\n')
    
    for line in lines:
        # Match mode 8 (Perceptron)
        match_perc = re.search(r'^8\s+\S.*?\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s*$', line)
        if match_perc:
            results['perceptron'] = {
                'cycles': int(match_perc.group(1)),
                'instructions': int(match_perc.group(2)),
                'cpi': float(match_perc.group(3)),
                'ipc': float(match_perc.group(4)),
                'mispredictions': int(match_perc.group(5))
            }
        
        # Match mode 9 (Gshare)
        match_gsh = re.search(r'^9\s+\S.*?\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s*$', line)
        if match_gsh:
            results['gshare'] = {
                'cycles': int(match_gsh.group(1)),
                'instructions': int(match_gsh.group(2)),
                'cpi': float(match_gsh.group(3)),
                'ipc': float(match_gsh.group(4)),
                'mispredictions': int(match_gsh.group(5))
            }
    
    return results

def collect_data():
    """Collect data for all history sizes for both predictors"""
    perceptron_results = []
    gshare_results = []
    
    print(f"\n{'='*70}")
    print(f"Testing PERCEPTRON and GSHARE predictors")
    print(f"{'='*70}")
    
    for h_size in HISTORY_SIZES:
        print(f"\nHistory size: {h_size}")
        
        # Update both history lengths in config
        update_history_length('perceptron', h_size)
        update_history_length('gshare', h_size)
        
        # Rebuild once
        if not rebuild_project():
            print(f"  Skipping h={h_size} due to build failure")
            continue
        
        # Run benchmark once, get both results
        print(f"  Running benchmark...", end=' ', flush=True)
        stats = run_benchmark(TEST_PROGRAM)
        
        if 'perceptron' in stats and 'gshare' in stats:
            print("✓")
            
            # Store Perceptron data
            perceptron_results.append({
                'history_size': h_size,
                'cycles': stats['perceptron']['cycles'],
                'instructions': stats['perceptron']['instructions'],
                'cpi': stats['perceptron']['cpi'],
                'mispredictions': stats['perceptron']['mispredictions']
            })
            
            # Store Gshare data
            gshare_results.append({
                'history_size': h_size,
                'cycles': stats['gshare']['cycles'],
                'instructions': stats['gshare']['instructions'],
                'cpi': stats['gshare']['cpi'],
                'mispredictions': stats['gshare']['mispredictions']
            })
            
            print(f"  Perceptron - Mispred: {stats['perceptron']['mispredictions']}, "
                  f"CPI: {stats['perceptron']['cpi']:.3f}")
            print(f"  Gshare     - Mispred: {stats['gshare']['mispredictions']}, "
                  f"CPI: {stats['gshare']['cpi']:.3f}")
        else:
            print("✗")
            print(f"  Failed to get stats for h={h_size}")
    
    return perceptron_results, gshare_results

def plot_results(perceptron_data, gshare_data, output_dir='graphs'):
    """Generate mispredictions comparison graph"""
    os.makedirs(output_dir, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Extract data
    perc_h = [d['history_size'] for d in perceptron_data]
    perc_mispred = [d['mispredictions'] for d in perceptron_data]
    
    gshare_h = [d['history_size'] for d in gshare_data]
    gshare_mispred = [d['mispredictions'] for d in gshare_data]
    
    # Create single figure
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot Mispredictions vs History Size
    ax.plot(perc_h, perc_mispred, 'o-', linewidth=2.5, markersize=10, 
            color='#9b59b6', label='Perceptron')
    ax.plot(gshare_h, gshare_mispred, 's-', linewidth=2.5, markersize=10, 
            color='#f39c12', label='Gshare')
    
    ax.set_xlabel('History Length (bits)', fontsize=13, fontweight='bold')
    ax.set_ylabel('Branch Mispredictions', fontsize=13, fontweight='bold')
    ax.set_title('Branch Mispredictions vs History Length\n(Matrix Multiplication 20x20)', 
                fontsize=15, fontweight='bold', pad=20)
    ax.legend(fontsize=12, loc='best')
    ax.grid(True, alpha=0.3, linestyle='--')
    ax.set_xscale('log', base=2)
    
    # Add value labels
    for h, m in zip(perc_h, perc_mispred):
        ax.annotate(f'{m}', (h, m), textcoords="offset points", 
                   xytext=(0,10), ha='center', fontsize=10, fontweight='bold',
                   color='#9b59b6')
    for h, m in zip(gshare_h, gshare_mispred):
        ax.annotate(f'{m}', (h, m), textcoords="offset points", 
                   xytext=(0,-15), ha='center', fontsize=10, fontweight='bold',
                   color='#f39c12')
    
    # Add timestamp
    timestamp_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fig.text(0.99, 0.01, f'Generated: {timestamp_str}', 
            ha='right', fontsize=8, style='italic', color='gray')
    
    # Save
    filename = f'{output_dir}/mispredictions_vs_history_{timestamp}.png'
    plt.tight_layout()
    plt.savefig(filename, dpi=300, bbox_inches='tight')
    print(f"\n✓ Graph saved: {filename}")
    plt.close()
    
    # Save data to JSON
    data_file = f'{output_dir}/history_analysis_{timestamp}.json'
    with open(data_file, 'w') as f:
        json.dump({
            'perceptron': perceptron_data,
            'gshare': gshare_data,
            'test_program': TEST_PROGRAM
        }, f, indent=2)
    print(f"✓ Data saved: {data_file}")

def print_summary(perceptron_data, gshare_data):
    """Print summary table"""
    print("\n" + "="*70)
    print("SUMMARY: Mispredictions by History Length")
    print("="*70)
    print(f"{'History':>8} | {'Perceptron':>12} | {'Gshare':>12} | {'Winner':>10}")
    print("-"*70)
    
    for i in range(len(HISTORY_SIZES)):
        h = HISTORY_SIZES[i]
        perc = next((d['mispredictions'] for d in perceptron_data if d['history_size'] == h), None)
        gsh = next((d['mispredictions'] for d in gshare_data if d['history_size'] == h), None)
        
        if perc is not None and gsh is not None:
            winner = "Perceptron" if perc < gsh else "Gshare" if gsh < perc else "Tie"
            print(f"{h:>8} | {perc:>12} | {gsh:>12} | {winner:>10}")
    
    print("="*70)

def main():
    print("="*70)
    print("Branch Predictor History Length Analysis")
    print("Test Program: Matrix Multiplication (20x20)")
    print("History Sizes: " + ", ".join(map(str, HISTORY_SIZES)))
    print("="*70)
    
    # Backup original config
    print("\nBacking up config file...")
    original_config = backup_config()
    
    try:
        # Collect data for both predictors in one pass
        perceptron_data, gshare_data = collect_data()
        
        if not perceptron_data or not gshare_data:
            print("\nError: Failed to collect sufficient data")
            return
        
        # Generate graphs
        print("\n" + "="*70)
        print("Generating graphs...")
        print("="*70)
        plot_results(perceptron_data, gshare_data)
        
        # Print summary
        print_summary(perceptron_data, gshare_data)
        
        print("\n" + "="*70)
        print("Analysis complete!")
        print("="*70)
        
    finally:
        # Restore original config
        print("\nRestoring original config file...")
        restore_config(original_config)
        print("✓ Config restored")
        
        # Rebuild with original config
        print("Rebuilding with original config...", end=' ', flush=True)
        subprocess.run(['make', '-C', 'build', '-j4'], 
                      capture_output=True, text=True)
        print("✓")

if __name__ == "__main__":
    main()
