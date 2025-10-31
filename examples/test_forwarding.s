# Test Forwarding Logic (Mode 4 Only)
# Validates EX-to-EX, MEM-to-EX, and store data forwarding

.data
store_test: .word 0, 0, 0, 0

.text
# Test 1: EX-to-EX forwarding (EX/MEM -> EX)
# Producer in EX/MEM, consumer in EX
addi x1, x0, 10      # Cycle N: x1 produced
add x2, x1, x0       # Cycle N+1: x1 forwarded from EX/MEM
# Expected: x2 = 10, no stalls

# Test 2: MEM-to-EX forwarding (MEM/WB -> EX)
# Producer in MEM/WB, consumer in EX
addi x3, x0, 20      # Cycle N: x3 produced
nop                  # Cycle N+1: gap
add x4, x3, x0       # Cycle N+2: x3 forwarded from MEM/WB
# Expected: x4 = 20, no stalls

# Test 3: Forwarding priority (EX/MEM takes priority over MEM/WB)
addi x5, x0, 30      # Older write to x5
addi x5, x0, 40      # Newer write to x5 (in EX/MEM when next instr in EX)
add x6, x5, x0       # Should get 40 from EX/MEM, not 30 from MEM/WB
# Expected: x6 = 40

# Test 4: Forwarding both operands
addi x7, x0, 50      # x7 = 50
addi x8, x0, 60      # x8 = 60
add x9, x7, x8       # Forward both x7 and x8
# Expected: x9 = 110, no stalls

# Test 5: Chain of forwarding
addi x10, x0, 1
addi x11, x10, 1     # Forward x10
addi x12, x11, 1     # Forward x11
addi x13, x12, 1     # Forward x12
# Expected: x10=1, x11=2, x12=3, x13=4, no stalls

# Test 6: Store data forwarding
lui x20, 0x10000     # Base address
addi x21, x0, 100    # x21 = 100
sw x21, 0(x20)       # Store x21 (should forward value)
addi x22, x0, 200
sw x22, 4(x20)       # Store x22
# Expected: Memory[0x10000000] = 100, Memory[0x10000004] = 200

# Test 7: Load result NOT forwarded from EX/MEM (data not ready)
lw x23, 0(x20)       # Load x23
add x24, x23, x0     # Must stall (load-use hazard)
# Expected: 1 stall cycle, x24 = 100

# Test 8: Load result CAN be forwarded from MEM/WB
lw x25, 4(x20)       # Load x25 = 200
nop                  # Gap allows load to complete
add x26, x25, x0     # Forward from MEM/WB, no stall
# Expected: x26 = 200, no stalls

# Test 9: Multiple producers, same register
addi x27, x0, 10
addi x27, x0, 20
addi x27, x0, 30
add x28, x27, x0     # Should get most recent value (30)
# Expected: x28 = 30

# Test 10: Forwarding with write to x0 (should be ignored)
addi x0, x0, 999     # Write to x0 (ignored)
add x29, x0, x0      # Should get 0, not 999
# Expected: x29 = 0
