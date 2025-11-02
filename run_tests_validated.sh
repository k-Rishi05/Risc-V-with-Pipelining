#!/bin/bash

# Enhanced Test Runner with Validation
# Tests the RISC-V simulator across multiple modes and validates results

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
EXAMPLES_DIR="examples"
EXPECTATIONS_FILE="test_expectations.json"
VALIDATOR_SCRIPT="validate_test.py"
TEMP_DIR="/tmp/riscv_tests_$$"

# Test modes to run
MODES=(2 3 4)
MODE_NAMES=("PIPE_NO_HAZ" "PIPE_STALL" "PIPE_FWD")

# Test files (must have entries in test_expectations.json)
TEST_FILES=(
    "test_raw_hazard"
    "test_load_use"
    "test_forwarding"
    "test_control_hazard"
)

# Statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Create temp directory
mkdir -p "$TEMP_DIR"

# Cleanup function
cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

# Print header
print_header() {
    echo -e "${BLUE}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║     RISC-V Simulator - Automated Test Suite with Validation   ║${NC}"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

# Print section
print_section() {
    echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

# Check if file exists
check_file() {
    if [ ! -f "$1" ]; then
        echo -e "${RED}✗ File not found: $1${NC}"
        return 1
    fi
    return 0
}

# Build the project
build_project() {
    print_section "Building Project"
    
    if [ ! -d "$BUILD_DIR" ]; then
        echo -e "${YELLOW}Build directory not found. Creating...${NC}"
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake .. || { echo -e "${RED}✗ CMake failed${NC}"; exit 1; }
        cd ..
    fi
    
    echo "Building..."
    cd "$BUILD_DIR"
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2) > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Build successful${NC}"
        cd ..
        return 0
    else
        echo -e "${RED}✗ Build failed${NC}"
        cd ..
        exit 1
    fi
}

# Run a single test
run_test() {
    local test_name=$1
    local mode=$2
    local mode_name=$3
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    local test_file="${EXAMPLES_DIR}/${test_name}.s"
    local output_file="${TEMP_DIR}/${test_name}_mode${mode}.txt"
    local vm_state_dir="${BUILD_DIR}/vm_state"
    
    # Check if test file exists
    if [ ! -f "$test_file" ]; then
        echo -e "  ${YELLOW}⚠ Test file not found, skipping${NC}"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return
    fi
    
    # Run the simulator
    cd "$BUILD_DIR"
    timeout 30s ./vm --vm-mode "$mode" --run "../$test_file" > "$output_file" 2>&1
    local run_status=$?
    cd ..
    
    if [ $run_status -ne 0 ]; then
        echo -e "  ${RED}✗ Simulator failed or timeout${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return
    fi
    
    # Extract basic stats
    local stats=$(grep "VM_STATS" "$output_file" 2>/dev/null)
    if [ -z "$stats" ]; then
        echo -e "  ${RED}✗ No statistics found${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return
    fi
    
    # Extract cycles, retired, cpi using sed (BSD compatible)
    local cycles=$(echo "$stats" | sed -n 's/.*cycles=\([0-9]*\).*/\1/p')
    local retired=$(echo "$stats" | sed -n 's/.*retired=\([0-9]*\).*/\1/p')
    local cpi=$(echo "$stats" | sed -n 's/.*cpi=\([0-9.]*\).*/\1/p')
    
    echo -e "  ${BLUE}Stats:${NC} cycles=$cycles, retired=$retired, cpi=$cpi"
    
    # Run validation if validator script exists
    if [ -f "$VALIDATOR_SCRIPT" ]; then
        local validation_output=$(python3 "$VALIDATOR_SCRIPT" \
            "$test_name" "$mode" "$EXPECTATIONS_FILE" \
            "${vm_state_dir}/registers_dump.json" "$output_file" 2>&1)
        local validation_status=$?
        
        if [ $validation_status -eq 0 ]; then
            # Count check marks in validation output (handle both single and multi-byte characters)
            local checks=$(echo "$validation_output" | grep -o "✓" | wc -l | tr -d ' ')
            if [ ! -z "$checks" ] && [ "$checks" -gt 0 ] 2>/dev/null; then
                echo -e "  ${GREEN}✓ Validation passed ($checks checks)${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            else
                echo -e "  ${YELLOW}⚠ Validation completed (no checks defined)${NC}"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            fi
            # Show validation details (indent each line)
            echo "$validation_output" | sed 's/^/    /'
        else
            echo -e "  ${RED}✗ Validation failed${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            # Show validation errors (indent each line)
            echo "$validation_output" | sed 's/^/    /'
        fi
    else
        echo -e "  ${YELLOW}⚠ No validation script found${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    fi
}

# Main execution
main() {
    print_header
    
    # Check prerequisites
    print_section "Checking Prerequisites"
    
    check_file "CMakeLists.txt" || check_file "CMakeLists.txt" || exit 1
    echo -e "${GREEN}✓ CMakeLists.txt found${NC}"
    
    if [ -f "$EXPECTATIONS_FILE" ]; then
        echo -e "${GREEN}✓ Test expectations file found${NC}"
    else
        echo -e "${YELLOW}⚠ Test expectations file not found (validation will be limited)${NC}"
    fi
    
    if [ -f "$VALIDATOR_SCRIPT" ]; then
        echo -e "${GREEN}✓ Validation script found${NC}"
    else
        echo -e "${YELLOW}⚠ Validation script not found (skipping validation)${NC}"
    fi
    
    # Build project
    build_project
    
    # Run tests
    print_section "Running Tests"
    
    for test_name in "${TEST_FILES[@]}"; do
        echo -e "\n${YELLOW}Testing: ${test_name}.s${NC}"
        
        for i in "${!MODES[@]}"; do
            mode="${MODES[$i]}"
            mode_name="${MODE_NAMES[$i]}"
            
            echo -e "\n  ${BLUE}Mode $mode ($mode_name):${NC}"
            run_test "$test_name" "$mode" "$mode_name"
        done
    done
    
    # Print summary
    print_section "Test Summary"
    
    echo ""
    echo -e "  Total Tests:   ${BLUE}$TOTAL_TESTS${NC}"
    echo -e "  Passed:        ${GREEN}$PASSED_TESTS${NC}"
    echo -e "  Failed:        ${RED}$FAILED_TESTS${NC}"
    echo -e "  Skipped:       ${YELLOW}$SKIPPED_TESTS${NC}"
    echo ""
    
    local success_rate=0
    if [ $TOTAL_TESTS -gt 0 ]; then
        success_rate=$((PASSED_TESTS * 100 / TOTAL_TESTS))
    fi
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║  ✓ All Tests Passed! ($success_rate%)            ║${NC}"
        echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
        exit 0
    else
        echo -e "${RED}╔════════════════════════════════════════╗${NC}"
        echo -e "${RED}║  ✗ Some Tests Failed ($success_rate% passed)      ║${NC}"
        echo -e "${RED}╚════════════════════════════════════════╝${NC}"
        exit 1
    fi
}

# Run main
main "$@"
