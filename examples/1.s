.text
# Test 1: Simple taken branch
addi x1, x0, 10
addi x2, x0, 10
beq x1, x2, target1  # Branch taken
addi x3, x0, 99      # Should NOT execute (flushed)
target1:
addi x4, x0, 100     # Should execute
# Expected: x1=10, x2=10, x3=0, x4=100