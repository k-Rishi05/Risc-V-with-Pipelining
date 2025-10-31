# Comprehensive Test Cases for RV5S RISC-V Simulator

## Understanding the Simulator Configuration

### Pipeline Modes
1. **Mode 1 (SINGLE_CYCLE)**: Single-stage VM (rvss_vm)
2. **Mode 2 (PIPE_NO_HAZ)**: 5-stage pipeline, no hazard detection
3. **Mode 3 (PIPE_STALL)**: 5-stage pipeline with stall-only hazard detection
4. **Mode 4 (PIPE_FWD)**: 5-stage pipeline with forwarding
5. **Mode 5 (PIPE_STATIC_BP)**: Pipeline with static branch prediction
6. **Mode 6 (PIPE_DYN1_BP)**: Pipeline with 1-bit dynamic branch prediction

### Key Features to Test
- **Pipeline Stages**: IF, ID, EX, MEM, WB
- **Hazard Detection**: Data hazards (RAW), Control hazards (branches/jumps)
- **Forwarding**: EX-to-EX, MEM-to-EX forwarding
- **Stalling**: Load-use hazards, data dependency stalls
- **Undo/Redo**: State restoration per cycle
- **Bubble Tracking**: Identifying NOPs vs real instructions

---

## Test Suite

### 1. Basic Instruction Execution Tests

#### Test 1.1: R-Type Instructions
```assembly
# Test all R-type ALU operations
.text
addi x1, x0, 10      # x1 = 10
addi x2, x0, 5       # x2 = 5
add x3, x1, x2       # x3 = 15
sub x4, x1, x2       # x4 = 5
and x5, x1, x2       # x5 = 0
or x6, x1, x2        # x6 = 15
xor x7, x1, x2       # x7 = 15
sll x8, x1, x2       # x8 = 320
srl x9, x1, x2       # x9 = 0
sra x10, x1, x2      # x10 = 0
slt x11, x2, x1      # x11 = 1
sltu x12, x2, x1     # x12 = 1
```

**Expected Results (Mode 2)**:
- Cycles: 17 (5 fill + 12 instructions)
- CPI: ~1.42
- All registers match expected values

**Expected Results (Mode 3 - Stall)**:
- Additional stalls for back-to-back dependencies
- CPI: ~2.5-3.0

**Expected Results (Mode 4 - Forward)**:
- Forwarding eliminates most stalls
- CPI: ~1.5

---

#### Test 1.2: I-Type Instructions
```assembly
.text
addi x1, x0, 100
addi x2, x1, -50     # x2 = 50
slti x3, x1, 101     # x3 = 1
sltiu x4, x1, 101    # x4 = 1
xori x5, x1, 0xFF    # x5 = 155
ori x6, x1, 0x0F     # x6 = 111
andi x7, x1, 0x0F    # x7 = 4
slli x8, x1, 2       # x8 = 400
srli x9, x1, 2       # x9 = 25
srai x10, x1, 2      # x10 = 25
```

---

#### Test 1.3: Load/Store Instructions
```assembly
.data
test_data: .word 0x12345678
.text
lui x1, 0x10000      # Base address
addi x1, x1, 0       # x1 = 0x10000000
lw x2, 0(x1)         # Load word
sw x2, 4(x1)         # Store word
lh x3, 0(x1)         # Load halfword
sh x3, 8(x1)         # Store halfword
lb x4, 0(x1)         # Load byte
sb x4, 12(x1)        # Store byte
lbu x5, 0(x1)        # Load byte unsigned
lhu x6, 0(x1)        # Load halfword unsigned
lwu x7, 0(x1)        # Load word unsigned
```

**Critical Test (Mode 3)**: Load-use hazard
```assembly
lw x1, 0(x2)         # Load x1
add x3, x1, x4       # Use x1 immediately - MUST STALL
```

**Expected**: 1 stall cycle in Mode 3, 1 stall in Mode 4 (unavoidable)

---

### 2. Hazard Detection Tests

#### Test 2.1: RAW Data Hazards (Read After Write)
```assembly
# EX hazard (result needed 2 cycles after producer)
.text
addi x1, x0, 10      # Cycle 1: x1 produced in WB at cycle 6
addi x2, x1, 5       # Cycle 2: needs x1 in EX at cycle 7 (1 stall in Mode 3)
```

**Expected (Mode 2)**: Incorrect result (no hazard detection)
**Expected (Mode 3)**: 2 stalls, correct result
**Expected (Mode 4)**: No stalls (forwarding), correct result

#### Test 2.2: MEM Hazard
```assembly
.text
addi x1, x0, 20      # Cycle 1: x1 in WB at cycle 6
nop                  # Cycle 2
addi x2, x1, 10      # Cycle 3: needs x1 in EX at cycle 8 (1 stall in Mode 3)
```

**Expected (Mode 3)**: 1 stall
**Expected (Mode 4)**: No stalls (MEM-to-EX forwarding)

#### Test 2.3: Load-Use Hazard (Unavoidable)
```assembly
.text
lui x10, 0x10000
lw x1, 0(x10)        # Load x1
add x2, x1, x3       # Use x1 - MUST STALL 1 cycle
```

**Expected (Mode 3)**: 1 stall
**Expected (Mode 4)**: 1 stall (unavoidable - data not ready)

#### Test 2.4: Multiple Dependencies
```assembly
.text
addi x1, x0, 10
addi x2, x1, 5       # Depends on x1
addi x3, x2, 3       # Depends on x2
addi x4, x3, 1       # Depends on x3
```

**Expected (Mode 3)**: 2+2+2 = 6 stalls
**Expected (Mode 4)**: 0 stalls (all forwarded)

---

### 3. Control Hazard Tests

#### Test 3.1: Branch Hazards
```assembly
.text
    addi x1, x0, 10
    addi x2, x0, 10
    beq x1, x2, target   # Branch taken
    addi x3, x0, 99      # Should NOT execute
target:
    addi x4, x0, 100     # Should execute
```

**Expected (Mode 2)**: Wrong path executed (no hazard handling)
**Expected (Mode 3/4)**: 1 IF flush, correct path

#### Test 3.2: JAL/JALR
```assembly
.text
    jal x1, func         # Jump and link
    addi x2, x0, 99      # Should NOT execute
func:
    addi x3, x0, 100     # Should execute
    jalr x0, 0(x1)       # Return
```

**Expected**: Return address correctly stored, IF flush on jumps

#### Test 3.3: Nested Branches
```assembly
.text
    addi x1, x0, 5
    addi x2, x0, 10
    blt x1, x2, label1   # Taken
    addi x3, x0, 1
label1:
    addi x4, x0, 2
    bge x2, x1, label2   # Taken
    addi x5, x0, 3
label2:
    addi x6, x0, 4
```

---

### 4. Forwarding Tests (Mode 4 Only)

#### Test 4.1: EX-to-EX Forwarding
```assembly
.text
addi x1, x0, 10      # x1 in EX/MEM at cycle 4
add x2, x1, x0       # x1 needed in EX at cycle 5 - FORWARD from EX/MEM
```

**Expected**: No stalls, x2 = 10

#### Test 4.2: MEM-to-EX Forwarding
```assembly
.text
addi x1, x0, 20      # x1 in MEM/WB at cycle 5
nop
add x2, x1, x0       # x1 needed in EX at cycle 6 - FORWARD from MEM/WB
```

**Expected**: No stalls, x2 = 20

#### Test 4.3: Store Data Forwarding
```assembly
.data
.align 2
test: .word 0
.text
lui x10, 0x10000
addi x1, x0, 42
sw x1, 0(x10)        # Store x1 - should forward x1 value
```

**Expected**: Correct value stored

#### Test 4.4: Forwarding Priority (EX over MEM)
```assembly
.text
addi x1, x0, 10      # Older producer
addi x1, x0, 20      # Newer producer (in EX/MEM)
add x2, x1, x0       # Should get 20 (from EX/MEM, not MEM/WB)
```

**Expected**: x2 = 20 (EX/MEM takes priority)

---

### 5. Edge Cases and Corner Cases

#### Test 5.1: Write to x0 (Should be Ignored)
```assembly
.text
addi x0, x0, 100     # Should NOT change x0
addi x1, x0, 50      # x1 should be 50, not 150
```

**Expected**: x0 = 0, x1 = 50

#### Test 5.2: Self-Dependency
```assembly
.text
addi x1, x0, 10
addi x1, x1, 5       # x1 = x1 + 5
addi x1, x1, 3       # x1 = x1 + 3
```

**Expected (Mode 4)**: x1 = 18 (forwarding)
**Expected (Mode 3)**: x1 = 18 (with stalls)

#### Test 5.3: Load from Uninitialized Memory
```assembly
.text
lui x1, 0x20000      # Uninitialized region
lw x2, 0(x1)         # Should return 0 or error
```

**Expected**: x2 = 0 (or handle gracefully)

#### Test 5.4: Store/Load Aliasing
```assembly
.data
.align 2
mem: .word 0
.text
lui x10, 0x10000
addi x1, x0, 100
sw x1, 0(x10)        # Store 100
lw x2, 0(x10)        # Load should get 100
```

**Expected**: x2 = 100

#### Test 5.5: Branch to Invalid Address
```assembly
.text
    lui x1, 0xFFFF
    jalr x0, 0(x1)   # Jump to invalid address
```

**Expected**: Error or graceful termination

#### Test 5.6: Maximum Immediate Values
```assembly
.text
addi x1, x0, 2047    # Max positive 12-bit immediate
addi x2, x0, -2048   # Min negative 12-bit immediate
lui x3, 0xFFFFF      # Max 20-bit immediate
```

---

### 6. Pipeline-Specific Tests

#### Test 6.1: Pipeline Fill (Cold Start)
```assembly
.text
addi x1, x0, 1
addi x2, x0, 2
addi x3, x0, 3
addi x4, x0, 4
addi x5, x0, 5
```

**Expected**:
- Cycle 1: Only IF valid
- Cycle 2: IF, ID valid
- Cycle 3: IF, ID, EX valid
- Cycle 4: IF, ID, EX, MEM valid
- Cycle 5: All stages valid (pipeline full)

#### Test 6.2: Pipeline Drain (End of Program)
```assembly
.text
addi x1, x0, 10
# End of program
```

**Expected**: Pipeline drains over 4 cycles after last fetch

#### Test 6.3: Bubble Propagation
```assembly
.text
addi x1, x0, 10
addi x2, x1, 5       # Causes stall in Mode 3
addi x3, x2, 3       # Should see bubble propagate
```

**Expected (Mode 3)**:
- Stall inserts bubble (is_bubble=true) in EX
- Bubble propagates through MEM, WB
- Bubble NOT counted as retired instruction

---

### 7. Undo/Redo Tests

#### Test 7.1: Single Step Undo
```assembly
.text
addi x1, x0, 10
addi x2, x0, 20
```

**Steps**:
1. Run to completion
2. Undo once
3. Check x2 = 0 (or previous value)
4. Undo again
5. Check x1 = 0

#### Test 7.2: Multiple Register Changes in One Cycle
```assembly
.text
# Cycle N might have WB writing x1 while ID reads multiple regs
addi x1, x0, 10
addi x2, x0, 20
add x3, x1, x2
```

**Test**: Undo should restore all register changes from that cycle

#### Test 7.3: Memory Change Undo
```assembly
.data
test: .word 0
.text
lui x1, 0x10000
addi x2, x0, 42
sw x2, 0(x1)
```

**Test**: Undo should restore memory to previous value

#### Test 7.4: Pipeline State Undo
```assembly
.text
addi x1, x0, 10
addi x2, x0, 20
```

**Test**: Undo should restore pipeline registers (IF/ID, ID/EX, etc.)

---

### 8. Stress Tests

#### Test 8.1: Long Dependency Chain
```assembly
.text
addi x1, x0, 1
addi x2, x1, 1
addi x3, x2, 1
addi x4, x3, 1
addi x5, x4, 1
addi x6, x5, 1
addi x7, x6, 1
addi x8, x7, 1
addi x9, x8, 1
addi x10, x9, 1
# ... continue for 20+ instructions
```

**Expected (Mode 3)**: Many stalls
**Expected (Mode 4)**: Minimal stalls (forwarding)

#### Test 8.2: Interleaved Independent Instructions
```assembly
.text
addi x1, x0, 10
addi x2, x0, 20
addi x3, x0, 30
addi x4, x0, 40
add x5, x1, x2
add x6, x3, x4
```

**Expected**: Instructions x3, x4 can execute while x5 stalls

#### Test 8.3: Alternating Loads and Computes
```assembly
.data
arr: .word 1, 2, 3, 4, 5
.text
lui x10, 0x10000
lw x1, 0(x10)
addi x2, x0, 100
lw x3, 4(x10)
add x4, x1, x2
lw x5, 8(x10)
add x6, x3, x2
```

**Expected**: Load-use stalls interspersed with forwarding

---

### 9. Boundary and Overflow Tests

#### Test 9.1: Register File Boundaries
```assembly
.text
addi x31, x0, 100    # Last GPR
addi x1, x31, 50     # Use x31
```

#### Test 9.2: Arithmetic Overflow
```assembly
.text
lui x1, 0x7FFFF
addi x1, x1, 0x7FF   # Max positive 64-bit
addi x2, x1, 1       # Overflow
```

#### Test 9.3: Memory Alignment
```assembly
.data
unaligned: .byte 1, 2, 3, 4
.text
lui x1, 0x10000
lw x2, 1(x1)         # Unaligned load - should error or handle
```

---

### 10. Configuration Tests

#### Test 10.1: Mode Switching
```assembly
.text
addi x1, x0, 10
```

**Run in all 4 modes (2, 3, 4)**, verify:
- Mode 2: No hazard detection
- Mode 3: Stalls present
- Mode 4: Forwarding works

#### Test 10.2: Instruction Execution Limit
Set `instruction_execution_limit = 5` in config

```assembly
.text
addi x1, x0, 1
addi x2, x0, 2
addi x3, x0, 3
addi x4, x0, 4
addi x5, x0, 5
addi x6, x0, 6       # Should NOT execute
```

---

## Validation Checklist

For each test case, verify:
- [ ] Correct register values
- [ ] Correct memory values
- [ ] Correct cycle count
- [ ] Correct retired instruction count
- [ ] Correct CPI/IPC
- [ ] Pipeline state visualization matches expectations
- [ ] Undo/redo restores correct state
- [ ] No crashes or undefined behavior

## Critical Bugs to Watch For

1. **Forwarding Priority**: EX/MEM should take priority over MEM/WB
2. **Load-Use Hazard**: Must stall even with forwarding
3. **Bubble Counting**: Bubbles should NOT be counted as retired instructions
4. **x0 Protection**: Writes to x0 must be ignored at WB
5. **Branch Flush**: IF flush must discard exactly one instruction
6. **Stall Counter**: Must decrement correctly over multiple cycles
7. **Store Data Forwarding**: Store instructions must use forwarded rs2 value
8. **Pipeline Drain**: Last instructions must complete correctly

## Performance Expectations

| Mode | No Hazards | With Hazards | Load-Use |
|------|------------|--------------|----------|
| Mode 2 | CPI ≈ 1.4 | Incorrect | Incorrect |
| Mode 3 | CPI ≈ 1.4 | CPI ≈ 2-3 | CPI ≈ 1.5 |
| Mode 4 | CPI ≈ 1.4 | CPI ≈ 1.5 | CPI ≈ 1.5 |

---

## Test Execution Commands

```bash
# Build
cmake --build build -j 4

# Run test with specific mode
./build/vm --start-vm
> modify_config e m 3  # Set to Mode 3 (stall)
> load examples/test_case_X.s
> run

# Check results
> get_register x1
> get_memory 0x10000000
> print_pipeline
> undo
> redo
```

## Automated Test Script (Recommended)

Create a script to run all test cases across all modes and compare results automatically.

