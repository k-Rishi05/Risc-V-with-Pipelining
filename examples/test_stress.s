# Stress Test: Long Dependency Chains
# Tests pipeline performance under heavy data dependencies

.text
# Test 1: Maximum dependency chain (20 instructions)
addi x1, x0, 1
addi x2, x1, 1       # Depends on x1
addi x3, x2, 1       # Depends on x2
addi x4, x3, 1       # Depends on x3
addi x5, x4, 1       # Depends on x4
addi x6, x5, 1       # Depends on x5
addi x7, x6, 1       # Depends on x6
addi x8, x7, 1       # Depends on x7
addi x9, x8, 1       # Depends on x8
addi x10, x9, 1      # Depends on x9
addi x11, x10, 1     # Depends on x10
addi x12, x11, 1     # Depends on x11
addi x13, x12, 1     # Depends on x12
addi x14, x13, 1     # Depends on x14
addi x15, x14, 1     # Depends on x15
addi x16, x15, 1     # Depends on x16
addi x17, x16, 1     # Depends on x17
addi x18, x17, 1     # Depends on x18
addi x19, x18, 1     # Depends on x19
addi x20, x19, 1     # Depends on x20
# Expected: x20 = 20
# Mode 3: Many stalls (2 stalls per dependency)
# Mode 4: No stalls (all forwarded)

# Test 2: Interleaved independent chains (no dependencies)
addi x21, x0, 100
addi x22, x0, 200
addi x23, x0, 300
addi x24, x0, 400
addi x25, x0, 500
add x26, x21, x22    # x26 = 300
add x27, x23, x24    # x27 = 700
add x28, x25, x26    # x28 = 800
# Expected: Minimal stalls in all modes

# Test 3: Pyramid dependency
addi x1, x0, 1
addi x2, x0, 2
add x3, x1, x2       # x3 = 3
addi x4, x0, 4
add x5, x3, x4       # x5 = 7
addi x6, x0, 6
add x7, x5, x6       # x7 = 13
# Expected: x7 = 13

# Test 4: Worst-case alternating dependencies
addi x10, x0, 10
addi x11, x10, 1     # Dep on x10
addi x10, x11, 1     # Dep on x11, writes x10
addi x11, x10, 1     # Dep on x10, writes x11
addi x10, x11, 1     # Dep on x11, writes x10
addi x11, x10, 1     # Dep on x10, writes x11
# Expected: x10 = 15, x11 = 16
# Mode 3: Maximum stalls
# Mode 4: Forwarding helps significantly
