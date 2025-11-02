# Test Control Hazards (Branches and Jumps)
# Control hazards cause IF stage flushes

.text
# Test 1: Simple taken branch
addi x2,x0,0
addi x1, x0, 10
addi x2, x0, 10
beq x1, x2, target1  # Branch taken
addi x3, x0, 99      # Should NOT execute (flushed)
target1:
addi x4, x0, 100     # Should execute
# Expected: x1=10, x2=10, x3=0, x4=100

# Test 2: Not-taken branch
addi x5, x0, 5
addi x6, x0, 10
bne x5, x6, target2  # Branch taken (5 != 10)
addi x7, x0, 88      # Should NOT execute
target2:
addi x8, x0, 77      # Should execute
# Expected: x5=5, x6=10, x7=0, x8=77

# Test 3: JAL (Jump and Link)
jal x9, func1        # Jump to func1, save return address
addi x10, x0, 111    # Should NOT execute
func1:
addi x11, x0, 222    # Should execute
jal x0, after_func1  # Jump to continue (skip JALR test complexity)
after_func1:
addi x12, x0, 12
# Expected: x9=return_addr, x10=0, x11=222, x12=12

# Test 4: Nested branches
addi x17, x0, 1
addi x18, x0, 2
blt x17, x18, nest1  # Taken (1 < 2)
addi x19, x0, 111
nest1:
addi x20, x0, 222
bge x18, x17, nest2  # Taken (2 >= 1)
addi x21, x0, 333
nest2:
addi x22, x0, 444
# Expected: x17=1, x18=2, x19=0, x20=222, x21=0, x22=444

# Test 5: Branch with data dependency
addi x23, x0, 100
addi x24, x23, 100   # Dependency (may stall in Mode 3)
beq x23, x24, end    # Not taken (100 != 200)
addi x25, x0, 555    # Should execute
end:
addi x26, x0, 666
# Expected: x23=100, x24=200, x25=555, x26=666
