# Undo/Redo Functionality Test Suite
# Tests state restoration across multiple cycles

.data
memory_test: .word 0, 0, 0, 0

.text
# Test 1: Simple register changes
addi x1, x0, 10      # x1 = 10
addi x2, x0, 20      # x2 = 20
addi x3, x0, 30      # x3 = 30
# After undo x3: x3 should be 0
# After undo x2: x2 should be 0
# After undo x1: x1 should be 0

# Test 2: Register overwrite
addi x4, x0, 100
addi x4, x0, 200     # Overwrite x4
addi x4, x0, 300     # Overwrite again
# After undo: x4 = 200
# After undo: x4 = 100
# After undo: x4 = 0

# Test 3: Memory changes
lui x10, 0x10000
addi x5, x0, 42
sw x5, 0(x10)        # Write 42 to memory
addi x6, x0, 99
sw x6, 0(x10)        # Overwrite with 99
# After undo: memory[0x10000000] = 42
# After undo: memory[0x10000000] = 0

# Test 4: Multiple register changes in one WB cycle
# (Pipeline may have multiple instructions completing)
addi x7, x0, 1
addi x8, x0, 2
addi x9, x0, 3
addi x10, x0, 4
# Undo should restore all registers correctly

# Test 5: Pipeline state restoration
addi x11, x0, 11
addi x12, x0, 12
addi x13, x0, 13
# Undo should restore PC and pipeline register contents

# Test 6: Undo after branch
addi x14, x0, 5
addi x15, x0, 5
beq x14, x15, target
addi x16, x0, 999    # Should not execute
target:
addi x17, x0, 777    # Should execute
# Undo should restore correct branch path

# Test 7: Redo functionality
addi x18, x0, 88
# Undo, then redo should restore x18 = 88

# Test 8: Complex dependency chain
addi x19, x0, 1
addi x20, x19, 1     # x20 = 2
addi x21, x20, 1     # x21 = 3
addi x22, x21, 1     # x22 = 4
# Undo should correctly restore dependency chain

# Test 9: Load/Store with undo
lui x25, 0x10000
addi x23, x0, 123
sw x23, 4(x25)       # Write 123
lw x24, 4(x25)       # Load back
# Undo load: x24 should revert
# Undo store: memory should revert

# Test 10: Multiple undos and redos
addi x26, x0, 10
addi x27, x0, 20
addi x28, x0, 30
# Undo 3 times, then redo 3 times should restore all
