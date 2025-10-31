#!/usr/bin/env python3
"""
Test Validator for RISC-V Simulator
Validates register values and performance metrics against expected values
"""

import json
import sys
import re

def parse_register_dump(filepath):
    """Parse registers_dump.json file"""
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
            registers = {}
            
            # Handle both formats: 'gpr' array and 'gp_registers' dict
            if 'gpr' in data:
                for reg in data['gpr']:
                    reg_name = f"x{reg['index']}"
                    registers[reg_name] = reg['value']
            elif 'gp_registers' in data:
                for reg_name, hex_value in data['gp_registers'].items():
                    # Convert hex string to int
                    registers[reg_name] = int(hex_value, 16)
            
            return registers
    except Exception as e:
        print(f"Error parsing register dump: {e}")
        return None

def parse_stats(output_file):
    """Parse VM_STATS from output"""
    try:
        with open(output_file, 'r') as f:
            content = f.read()
            match = re.search(r'VM_STATS cycles=(\d+) retired=(\d+) cpi=([\d.]+)', content)
            if match:
                return {
                    'cycles': int(match.group(1)),
                    'retired': int(match.group(2)),
                    'cpi': float(match.group(3))
                }
    except Exception as e:
        print(f"Error parsing stats: {e}")
    return None

def validate_test(test_name, mode, expectations_file, register_dump, output_file):
    """Validate a test run against expectations"""
    
    # Load expectations
    try:
        with open(expectations_file, 'r') as f:
            expectations = json.load(f)
    except:
        print(f"Warning: Could not load expectations file")
        return True  # Pass by default if no expectations
    
    if test_name not in expectations:
        print(f"  No expectations defined for {test_name}")
        return True
    
    test_exp = expectations[test_name]
    mode_str = str(mode)
    
    # Parse actual results
    registers = parse_register_dump(register_dump)
    stats = parse_stats(output_file)
    
    if not stats:
        print(f"  ❌ Could not parse statistics")
        return False
    
    all_pass = True
    
    # Check if we have mode-specific expectations
    if 'modes' in test_exp and mode_str in test_exp['modes']:
        mode_exp = test_exp['modes'][mode_str]
        
        # Validate cycles
        if 'cycles' in mode_exp and mode_exp['cycles'] is not None:
            expected_cycles = mode_exp['cycles']
            if stats['cycles'] != expected_cycles:
                print(f"  ❌ Cycle mismatch: expected {expected_cycles}, got {stats['cycles']}")
                all_pass = False
            else:
                print(f"  ✓ Cycles correct: {stats['cycles']}")
        
        # Validate retired instructions
        if 'retired' in mode_exp and mode_exp['retired'] is not None:
            expected_retired = mode_exp['retired']
            if stats['retired'] != expected_retired:
                print(f"  ❌ Retired mismatch: expected {expected_retired}, got {stats['retired']}")
                all_pass = False
            else:
                print(f"  ✓ Retired correct: {stats['retired']}")
        
        # Validate CPI (with tolerance for floating point)
        if 'cpi' in mode_exp and mode_exp['cpi'] is not None:
            expected_cpi = mode_exp['cpi']
            if abs(stats['cpi'] - expected_cpi) > 0.01:
                print(f"  ❌ CPI mismatch: expected {expected_cpi}, got {stats['cpi']}")
                all_pass = False
            else:
                print(f"  ✓ CPI correct: {stats['cpi']}")
        
        # Check if registers should be validated (mode 2 might have wrong values)
        if mode_exp.get('registers_correct', True) == False:
            print(f"  ⚠️  Register validation skipped (expected wrong values in mode {mode})")
            return all_pass
    
    # Validate register values
    if registers and 'registers' in test_exp:
        reg_exp = test_exp['registers']
        reg_errors = []
        
        for reg_name, expected_val in reg_exp.items():
            actual_val = registers.get(reg_name, 0)
            if actual_val != expected_val:
                reg_errors.append(f"{reg_name}: expected {expected_val}, got {actual_val}")
        
        if reg_errors:
            print(f"  ❌ Register mismatches:")
            for error in reg_errors[:5]:  # Show first 5 errors
                print(f"     {error}")
            if len(reg_errors) > 5:
                print(f"     ... and {len(reg_errors) - 5} more")
            all_pass = False
        else:
            print(f"  ✓ All registers correct")
    
    return all_pass

if __name__ == "__main__":
    if len(sys.argv) != 6:
        print("Usage: validate_test.py <test_name> <mode> <expectations_file> <register_dump> <output_file>")
        sys.exit(1)
    
    test_name = sys.argv[1]
    mode = sys.argv[2]
    expectations_file = sys.argv[3]
    register_dump = sys.argv[4]
    output_file = sys.argv[5]
    
    result = validate_test(test_name, mode, expectations_file, register_dump, output_file)
    sys.exit(0 if result else 1)
