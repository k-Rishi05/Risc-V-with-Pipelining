# Edge Cases and Corner Cases Test Suite
# Tests boundary conditions, special cases, and error handling

.data
test_mem: .word 0x12345678, 0xABCDEF00

.text
# Test 1: Write to x0 (should be ignored)
addi x0, x0, 999     # Try to write to x0
addi x1, x0, 100     # x1 should be 100, not 100+999
add x0, x1, x1       # Try to write 200 to x0
addi x2, x0, 50      # x2 should be 50, x0 still 0
# Expected: x0=0, x1=100, x2=50

# Test 2: Self-dependency (register reads and writes itself)
addi x3, x0, 10      # x3 = 10
addi x3, x3, 5       # x3 = x3 + 5 = 15
addi x3, x3, 3       # x3 = x3 + 3 = 18
# Expected: x3 = 18

# Test 3: All operands same register
addi x4, x0, 7
add x4, x4, x4       # x4 = x4 + x4 = 14
add x4, x4, x4       # x4 = x4 + x4 = 28
# Expected: x4 = 28

# Test 4: Maximum and minimum immediates
addi x5, x0, 2047    # Max positive 12-bit immediate
addi x6, x0, -2048   # Min negative 12-bit immediate
lui x7, 0xFFFFF      # Max 20-bit immediate (sign-extended)
# Expected: x5=2047, x6=-2048, x7=0xFFFFF000

# Test 5: Zero immediate
addi x8, x0, 0       # Add zero
ori x9, x0, 0        # OR with zero
andi x10, x0, 0      # AND with zero
# Expected: x8=0, x9=0, x10=0

# Test 6: Negative immediate arithmetic
addi x11, x0, 100
addi x12, x11, -50   # 100 - 50 = 50
addi x13, x12, -25   # 50 - 25 = 25
# Expected: x11=100, x12=50, x13=25

# Test 7: Load/Store byte alignment
lui x20, 0x10000
lw x14, 0(x20)       # Load word (aligned)
lb x15, 0(x20)       # Load byte
lh x16, 0(x20)       # Load halfword
# Expected: x14=0x12345678, x15=0x78 (sign-extended), x16=0x5678 (sign-extended)

# Test 8: Unsigned loads
lbu x17, 0(x20)      # Load byte unsigned
lhu x18, 0(x20)      # Load halfword unsigned
lwu x19, 0(x20)      # Load word unsigned
# Expected: x17=0x78, x18=0x5678, x19=0x12345678

# Test 9: Store then load back (aliasing test)
lui x21, 0x10000
addi x22, x0, 42
sw x22, 8(x21)       # Store 42
lw x23, 8(x21)       # Load back
# Expected: x23 = 42

# Test 10: Multiple stores to same location
sw x22, 12(x21)      # Store 42
addi x24, x0, 99
sw x24, 12(x21)      # Overwrite with 99
lw x25, 12(x21)      # Load back
# Expected: x25 = 99

# Test 11: All R-type operations
addi x26, x0, 15     # x26 = 0b1111
addi x27, x0, 10     # x27 = 0b1010
and x28, x26, x27    # x28 = 0b1010 = 10
or x29, x26, x27     # x29 = 0b1111 = 15
xor x30, x26, x27    # x30 = 0b0101 = 5
# Expected: x28=10, x29=15, x30=5

# Test 12: Shift operations
addi x31, x0, 8      # x31 = 8
slli x1, x31, 2      # Shift left by 2: 8 << 2 = 32
srli x2, x31, 1      # Shift right logical by 1: 8 >> 1 = 4
# Expected: x1=32, x2=4

# Test 13: Comparison operations
addi x3, x0, 5
addi x4, x0, 10
slt x5, x3, x4       # 5 < 10 -> 1
slt x6, x4, x3       # 10 < 5 -> 0
sltu x7, x3, x4      # 5 < 10 (unsigned) -> 1
# Expected: x5=1, x6=0, x7=1

# Test 14: Register file exhaustion (use all 31 GPRs)
addi x1, x0, 1
addi x2, x0, 2
addi x3, x0, 3
# ... (continue for all registers)
addi x31, x0, 31
add x1, x1, x31      # x1 = 1 + 31 = 32
# Expected: All registers have correct values

# Test 15: Back-to-back branches
addi x8, x0, 1
addi x9, x0, 1
beq x8, x9, label1   # Taken
nop
label1:
beq x8, x9, label2   # Taken again
nop
label2:
addi x10, x0, 100
# Expected: x10 = 100
