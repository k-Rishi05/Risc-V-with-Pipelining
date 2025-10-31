# Test RAW (Read After Write) Data Hazards
# This test validates hazard detection and forwarding logic

.data
.text

# Test 1: EX hazard (back-to-back dependency)
# Mode 2: Wrong result (no hazard detection)
# Mode 3: 2 stalls, correct result
# Mode 4: No stalls (forwarding from EX/MEM), correct result
addi x1, x0, 10      # x1 = 10
addi x2, x1, 5       # x2 should be 15 (depends on x1)

# Test 2: MEM hazard (1 instruction gap)
# Mode 3: 1 stall
# Mode 4: No stalls (forwarding from MEM/WB)
addi x3, x0, 20      # x3 = 20
nop
addi x4, x3, 10      # x4 should be 30

# Test 3: No hazard (2 instruction gap)
# All modes: No stalls needed
addi x5, x0, 30      # x5 = 30
nop
nop
addi x6, x5, 5       # x6 should be 35

# Test 4: Multiple dependencies
# Mode 3: Many stalls
# Mode 4: Forwarding eliminates stalls
addi x7, x0, 100
addi x8, x7, 1       # Depends on x7
addi x9, x8, 1       # Depends on x8
addi x10, x9, 1      # Depends on x9
# Expected: x7=100, x8=101, x9=102, x10=103

# Test 5: Both operands have dependencies
addi x11, x0, 50
addi x12, x0, 60
add x13, x11, x12    # Both operands dependent
# Expected: x13 = 110
