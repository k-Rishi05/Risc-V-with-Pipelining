#!/bin/bash
# Automated Test Runner for RV5S RISC-V Simulator
# Tests all modes and validates correctness

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test files
TEST_FILES=(
    "examples/test_raw_hazard.s"
    "examples/test_load_use.s"
    "examples/test_control_hazard.s"
    "examples/test_forwarding.s"
    "examples/test_edge_cases.s"
    "examples/test_undo_redo.s"
    "examples/test_stress.s"
)

# Modes to test
MODES=(2 3 4)
MODE_NAMES=("PIPE_NO_HAZ" "PIPE_STALL" "PIPE_FWD")

# Build the simulator
echo "Building simulator..."
cmake --build build -j 4
if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful${NC}\n"

# Run tests
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

for test_file in "${TEST_FILES[@]}"; do
    if [ ! -f "$test_file" ]; then
        echo -e "${YELLOW}Skipping $test_file (not found)${NC}"
        continue
    fi
    
    test_name=$(basename "$test_file" .s)
    echo -e "\n${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Testing: $test_name${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    for i in "${!MODES[@]}"; do
        mode="${MODES[$i]}"
        mode_name="${MODE_NAMES[$i]}"
        
        echo -e "\n${YELLOW}Running in Mode $mode ($mode_name)...${NC}"
        
        # Create input commands
        cat > /tmp/test_input.txt << EOF
modify_config e m $mode
load $test_file
run
quit
EOF
        
        # Run simulator
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        timeout 30s ./build/vm --start-vm < /tmp/test_input.txt > /tmp/test_output_${test_name}_${mode}.txt 2>&1
        
        if [ $? -eq 0 ]; then
            # Check for errors in output
            if grep -q "ERROR\|SEGFAULT\|Assertion" /tmp/test_output_${test_name}_${mode}.txt; then
                echo -e "${RED}✗ FAILED (errors detected)${NC}"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            elif grep -q "VM_PROGRAM_END" /tmp/test_output_${test_name}_${mode}.txt; then
                echo -e "${GREEN}✓ PASSED${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
                
                # Extract and display statistics (BSD grep compatible)
                cycles=$(grep "VM_STATS" /tmp/test_output_${test_name}_${mode}.txt | sed -n 's/.*cycles=\([0-9]*\).*/\1/p')
                retired=$(grep "VM_STATS" /tmp/test_output_${test_name}_${mode}.txt | sed -n 's/.*retired=\([0-9]*\).*/\1/p')
                cpi=$(grep "VM_STATS" /tmp/test_output_${test_name}_${mode}.txt | sed -n 's/.*cpi=\([0-9.]*\).*/\1/p')
                echo "  Cycles: $cycles, Retired: $retired, CPI: $cpi"
            else
                echo -e "${RED}✗ FAILED (incomplete execution)${NC}"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        else
            echo -e "${RED}✗ FAILED (timeout or crash)${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    done
done

# Summary
echo -e "\n${YELLOW}========================================${NC}"
echo -e "${YELLOW}Test Summary${NC}"
echo -e "${YELLOW}========================================${NC}"
echo -e "Total Tests: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
echo -e "${RED}Failed: $FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed! ✓${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed. Check output files in /tmp/${NC}"
    exit 1
fi
