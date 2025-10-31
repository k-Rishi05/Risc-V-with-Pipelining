# Test Load-Use Hazards
# Load-use hazards require stalling even with forwarding
# because data from memory is not available until MEM stage completes

.data
.word 42, 100, 200, 300

.text
# Setup base address
lui x10, 0x10000     # x10 = 0x10000000

# Test 1: Classic load-use hazard
# Mode 3: 1 stall required
# Mode 4: 1 stall required (unavoidable - data not ready)
lw x1, 0(x10)        # Load x1 = 42
add x2, x1, x0       # Use x1 immediately - MUST STALL
# Expected: x1 = 42, x2 = 42

# Test 2: Load-use with NOP in between (no stall needed)
lw x3, 4(x10)        # Load x3 = 100
nop                  # Data available by now
add x4, x3, x0       # No stall needed
# Expected: x3 = 100, x4 = 100

# Test 3: Load followed by another load (no dependency)
lw x5, 8(x10)        # Load x5 = 200
lw x6, 12(x10)       # Load x6 = 300 (no dependency on x5)
# Expected: No stalls, x5 = 200, x6 = 300

# Test 4: Load-use in both operands
lw x7, 0(x10)        # Load x7 = 42
lw x8, 4(x10)        # Load x8 = 100
add x9, x7, x8       # Both operands from loads
# Expected: x7 = 42, x8 = 100, x9 = 142
# Mode 3/4: Stall only if loads are too close

# Test 5: Load-ALU-ALU chain
lw x11, 0(x10)       # Load x11 = 42
addi x12, x11, 8     # Must stall (load-use)
addi x13, x12, 10    # Can forward from x12
# Expected: x11 = 42, x12 = 50, x13 = 60

# Test 6: Store after load (store data forwarding)
lw x14, 0(x10)       # Load x14 = 42
sw x14, 16(x10)      # Store x14 to memory
# Expected: Memory at offset 16 = 42
