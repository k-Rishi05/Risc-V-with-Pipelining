#!/bin/bash

# dynamic_bp.sh - Run all dynamic branch prediction modes and display statistics
# Usage: ./dynamic_bp.sh <file.s>

set -e  # Exit on error (disable for this script to handle errors gracefully)
set +e

# Check if file argument is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <file.s>"
    echo "Example: $0 examples/branch_test.s"
    exit 1
fi

ASM_FILE="$1"

# Convert to absolute path
if [[ "$ASM_FILE" != /* ]]; then
    ASM_FILE="$(pwd)/$ASM_FILE"
fi

# Check if file exists
if [ ! -f "$ASM_FILE" ]; then
    echo "Error: File '$ASM_FILE' not found!"
    exit 1
fi

BUILD_DIR="build"
VM_STATE_DIR="$BUILD_DIR/vm_state"

# Build the project if needed
echo "Building RISC-V VM..."
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. > /dev/null 2>&1
    cd ..
fi

cd "$BUILD_DIR"
make -j4 > /dev/null 2>&1
BUILD_STATUS=$?
cd ..

if [ $BUILD_STATUS -ne 0 ]; then
    echo "Error: Build failed. Please check your code."
    exit 1
fi

if [ ! -f "build/vm" ]; then
    echo "Error: VM executable not found after build."
    exit 1
fi

echo "✓ Build successful!"
echo ""

# Terminal colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Function to extract statistics from output or JSON
extract_stats() {
    local output_file="$1"
    local json_file="$2"
    
    # First try to extract from VM_STATS line in output (more reliable)
    if [ -f "$output_file" ]; then
        local stats_line=$(grep "VM_STATS" "$output_file" 2>/dev/null | tail -1)
        if [ ! -z "$stats_line" ]; then
            # Extract using sed (BSD compatible)
            local cycles=$(echo "$stats_line" | sed -n 's/.*cycles=\([0-9]*\).*/\1/p')
            local instructions=$(echo "$stats_line" | sed -n 's/.*retired=\([0-9]*\).*/\1/p')
            local cpi=$(echo "$stats_line" | sed -n 's/.*cpi=\([0-9.]*\).*/\1/p')
            local ipc=$(echo "$stats_line" | sed -n 's/.*ipc=\([0-9.]*\).*/\1/p')
            local mispredictions=$(echo "$stats_line" | sed -n 's/.*mispred=\([0-9]*\).*/\1/p')
            
            # Default values if extraction failed
            cycles=${cycles:-0}
            instructions=${instructions:-0}
            cpi=${cpi:-0.00}
            ipc=${ipc:-0.00}
            mispredictions=${mispredictions:-0}
            
            echo "$cycles|$instructions|$cpi|$ipc|$mispredictions"
            return
        fi
    fi
    
    # Fallback to JSON file
    if [ -f "$json_file" ]; then
        local cycles=$(grep -o '"cycle_count": [0-9]*' "$json_file" | grep -o '[0-9]*' | tail -1)
        local instructions=$(grep -o '"instructions_retired": [0-9]*' "$json_file" | grep -o '[0-9]*' | tail -1)
        local cpi=$(grep -o '"cpi": [0-9.eE+-]*' "$json_file" | grep -o '[0-9.eE+-]*' | tail -1)
        local ipc=$(grep -o '"ipc": [0-9.eE+-]*' "$json_file" | grep -o '[0-9.eE+-]*' | tail -1)
        local mispredictions=$(grep -o '"branch_mispredictions": [0-9]*' "$json_file" | grep -o '[0-9]*' | tail -1)
        
        cycles=${cycles:-0}
        instructions=${instructions:-0}
        cpi=${cpi:-0.00}
        ipc=${ipc:-0.00}
        mispredictions=${mispredictions:-0}
        
        echo "$cycles|$instructions|$cpi|$ipc|$mispredictions"
        return
    fi
    
    # No data found
    echo "0|0|0.00|0.00|0"
}

# Function to run VM in a specific mode
run_mode() {
    local mode_num=$1
    local mode_name=$2
    local temp_output="/tmp/vm_output_$$_${mode_num}.txt"
    
    # Print status message to stderr so it doesn't interfere with return value
    echo -e "${CYAN}Running MODE $mode_num - $mode_name...${NC}" >&2
    
    # Change to build directory to run VM
    cd "$BUILD_DIR"
    
    # Run the VM with timeout to prevent hanging
    timeout 30s ./vm --vm-mode "$mode_num" --run "$ASM_FILE" > "$temp_output" 2>&1
    local run_status=$?
    
    cd ..
    
    # Check if execution was successful
    if [ $run_status -eq 124 ]; then
        echo -e "${RED}Timeout running mode $mode_num${NC}" >&2
        rm -f "$temp_output"
        echo "0|0|0.00|0.00|0"
        return 1
    elif [ $run_status -ne 0 ]; then
        echo -e "${RED}Error running mode $mode_num (exit code: $run_status)${NC}" >&2
        rm -f "$temp_output"
        echo "0|0|0.00|0.00|0"
        return 1
    fi
    
    # Extract statistics from output and JSON
    local stats=$(extract_stats "$temp_output" "$VM_STATE_DIR/vm_state_dump.json")
    
    # Cleanup
    rm -f "$temp_output"
    
    echo "$stats"
}

# Print header
echo -e "\n${BOLD}╔════════════════════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║         RISC-V SIMULATOR - DYNAMIC BRANCH PREDICTION COMPARISON                ║${NC}"
echo -e "${BOLD}╠════════════════════════════════════════════════════════════════════════════════╣${NC}"
echo -e "${BOLD}║ Program: $(printf '%-68s' "$ASM_FILE")║${NC}"
echo -e "${BOLD}╚════════════════════════════════════════════════════════════════════════════════╝${NC}\n"

# Array to store results
declare -a MODES=("5" "6" "7" "8" "9")
declare -a MODE_NAMES=("Static BP (Always Not-Taken)" "1-Bit Dynamic BP" "2-Bit Dynamic BP" "Perceptron BP" "Gshare BP")
declare -a RESULTS

echo -e "${YELLOW}Executing all branch prediction modes... This may take a moment.${NC}\n"

# Run all modes and collect results
for i in "${!MODES[@]}"; do
    mode_num="${MODES[$i]}"
    mode_name="${MODE_NAMES[$i]}"
    
    result=$(run_mode "$mode_num" "$mode_name")
    RESULTS[$i]="$result"
    
    # Small delay between runs to ensure file writes complete
    sleep 0.2
done

# Print results table
echo -e "\n${BOLD}════════════════════════════════════════════════════════════════════════════════${NC}"
echo -e "${BOLD}MODE  PREDICTOR                      CYCLES  INSTR   CPI    IPC      MISPRED${NC}"
echo -e "${BOLD}────────────────────────────────────────────────────────────────────────────────${NC}"

for i in "${!MODES[@]}"; do
    mode_num="${MODES[$i]}"
    mode_name="${MODE_NAMES[$i]}"
    stats="${RESULTS[$i]}"
    
    # Parse stats
    IFS='|' read -r cycles instructions cpi ipc mispredictions <<< "$stats"
    
    # Color code based on performance (lower CPI is better)
    if (( $(echo "$cpi < 1.5" | bc -l 2>/dev/null || echo 0) )); then
        color=$GREEN
    elif (( $(echo "$cpi < 2.0" | bc -l 2>/dev/null || echo 0) )); then
        color=$YELLOW
    else
        color=$RED
    fi
    
    # Format IPC to 2 decimal places
    ipc_formatted=$(printf "%.2f" "$ipc")
    
    # Print row with color - cleaner formatting
    printf "${BLUE}%-4s${NC}  %-30s  ${color}%6s${NC}  %6s  ${color}%5s${NC}  %6s   ${MAGENTA}%7s${NC}\n" \
        "$mode_num" "$mode_name" "$cycles" "$instructions" "$cpi" "$ipc_formatted" "$mispredictions"
done

echo -e "${BOLD}════════════════════════════════════════════════════════════════════════════════${NC}\n"

# Find best performing mode (lowest CPI)
best_mode=""
best_cpi=999999.0
for i in "${!MODES[@]}"; do
    stats="${RESULTS[$i]}"
    IFS='|' read -r cycles instructions cpi ipc mispredictions <<< "$stats"
    
    if (( $(echo "$cpi < $best_cpi" | bc -l 2>/dev/null || echo 0) )); then
        best_cpi=$cpi
        best_mode="${MODES[$i]}"
    fi
done

if [ -n "$best_mode" ]; then
    echo -e "${GREEN}${BOLD}✓ Best Performance: MODE $best_mode with CPI = $best_cpi${NC}\n"
fi

# Display legend
echo -e "${BOLD}Legend:${NC}"
echo -e "  ${GREEN}Green${NC}  = Excellent performance (CPI < 1.5)"
echo -e "  ${YELLOW}Yellow${NC} = Good performance (1.5 ≤ CPI < 2.0)"
echo -e "  ${RED}Red${NC}    = Needs improvement (CPI ≥ 2.0)"
echo -e ""
echo -e "${BOLD}Notes:${NC}"
echo -e "  • Lower CPI (Cycles Per Instruction) is better"
echo -e "  • Higher IPC (Instructions Per Cycle) is better"
echo -e "  • Fewer mispredictions indicate better branch prediction accuracy"
echo -e ""
