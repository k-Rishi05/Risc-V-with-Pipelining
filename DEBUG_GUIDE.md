# Quick Reference: Debugging Guide

## Common Issues and Solutions

### Issue 1: Wrong Register Values
**Symptom:** Register doesn't have expected value after test
**Check:**
- [ ] Is forwarding enabled? (Mode 4 required for most tests)
- [ ] Is hazard detection enabled? (Mode 3/4 required)
- [ ] Did instruction reach WB stage?
- [ ] Is register x0? (writes to x0 are ignored)
- [ ] Check pipeline visualization for stalls/bubbles

**Debug:**
```bash
> get_register xN
> print_pipeline
> step    # Single-step to see progression
```

### Issue 2: Too Many/Few Stalls
**Symptom:** CPI much higher/lower than expected
**Check:**
- [ ] Correct mode selected? (2=no hazard, 3=stall, 4=forward)
- [ ] Bubbles propagating correctly?
- [ ] Stall counter decrementing properly?
- [ ] Load-use hazards detected?

**Expected Stalls:**
- Mode 2: 0 stalls (incorrect results)
- Mode 3: 2 stalls per EX hazard, 1 per MEM hazard
- Mode 4: 0 stalls for RAW (forwarded), 1 for load-use

### Issue 3: Branch/Jump Not Working
**Symptom:** Wrong instructions executed after branch
**Check:**
- [ ] IF flush occurred?
- [ ] PC updated to branch target?
- [ ] Branch condition evaluated correctly?
- [ ] Following instruction discarded?

**Debug:**
Watch PC and pipeline after branch:
```assembly
beq x1, x2, target
addi x3, x0, 99    # Should NOT execute if taken
```

### Issue 4: Forwarding Not Working (Mode 4)
**Symptom:** Stalls occurring when forwarding should eliminate them
**Check:**
- [ ] Mode 4 enabled? (`modify_config e m 4`)
- [ ] Producer in EX/MEM or MEM/WB?
- [ ] Consumer in EX stage?
- [ ] Is producer a load? (cannot forward from EX/MEM)
- [ ] Is destination x0? (no forwarding to/from x0)

**Test:**
```assembly
addi x1, x0, 10    # Producer
addi x2, x1, 5     # Consumer - should NOT stall in Mode 4
```

### Issue 5: Undo/Redo Not Restoring State
**Symptom:** Register/memory not restored after undo
**Check:**
- [ ] Was change logged in current_delta?
- [ ] Undo stack not empty?
- [ ] Pipeline state saved before step?
- [ ] All register writes logged at WB?

**Debug:**
- Add prints in stageWB() to see logged changes
- Check undo_stack size before/after undo
- Verify old_value != new_value

### Issue 6: Memory Access Errors
**Symptom:** Load/store causes crash or wrong value
**Check:**
- [ ] Address aligned properly?
- [ ] Address within valid range?
- [ ] Load size matches data? (LB, LH, LW, LD)
- [ ] Sign extension correct? (LB vs LBU)

**Test:**
```assembly
lui x10, 0x10000     # Valid data section address
lw x1, 0(x10)        # Must be word-aligned
```

### Issue 7: Pipeline Not Draining
**Symptom:** Program doesn't end, stuck in loop
**Check:**
- [ ] PC >= program_size?
- [ ] Pipeline empty? (all stages invalid)
- [ ] Infinite branch loop?

**Debug:**
```bash
> print_pipeline    # Check if stages are valid
> get_register pc   # Check PC value
```

## Quick Diagnostic Commands

```bash
# Check register state
> get_register x1
> get_register x10
> get_register pc

# Check memory
> get_memory 0x10000000

# Print pipeline visualization
> print_pipeline

# Single-step execution
> step
> step
> step

# Undo/redo test
> undo
> get_register x1
> redo
> get_register x1

# Change mode
> modify_config e m 2    # Mode 2: No hazard
> modify_config e m 3    # Mode 3: Stall
> modify_config e m 4    # Mode 4: Forward

# Run program
> load examples/test.s
> run

# Statistics
> (Look for VM_STATS in output)
```

## Expected Cycle Counts (5 instructions, no hazards)

| Mode | Fill | Execute | Drain | Total | CPI |
|------|------|---------|-------|-------|-----|
| 2 | 4 | 5 | 4 | 13 | 1.4 |
| 3 | 4 | 5 | 4 | 13 | 1.4 |
| 4 | 4 | 5 | 4 | 13 | 1.4 |

With hazards:
- Mode 2: Incorrect results
- Mode 3: +2 cycles per EX hazard
- Mode 4: No extra cycles (forwarded)

## Pipeline Stage Progression (5-instruction example)

```
Cycle 1: [I1] [ ] [ ] [ ] [ ]
Cycle 2: [I2] [I1] [ ] [ ] [ ]
Cycle 3: [I3] [I2] [I1] [ ] [ ]
Cycle 4: [I4] [I3] [I2] [I1] [ ]
Cycle 5: [I5] [I4] [I3] [I2] [I1]  <- Pipeline full
Cycle 6: [ ] [I5] [I4] [I3] [I2]   <- Start drain
Cycle 7: [ ] [ ] [I5] [I4] [I3]
Cycle 8: [ ] [ ] [ ] [I5] [I4]
Cycle 9: [ ] [ ] [ ] [ ] [I5]      <- Last retire
```

## Hazard Detection Cheat Sheet

### RAW Hazard Detection (Mode 3)
```
Producer in EX (ID/EX):   2 stalls
Producer in MEM (EX/MEM): 1 stall
Producer in WB (MEM/WB):  0 stalls (data ready)
```

### Forwarding (Mode 4)
```
EX/MEM → EX:  Forward (no stall)
MEM/WB → EX:  Forward (no stall)
Load in EX:   STALL 1 cycle (data not ready)
```

### Control Hazards (All modes 3+)
```
Branch/Jump in ID: Flush IF (1 cycle bubble)
```

## Bubble Identification

**Real Instruction:** `is_bubble = false`, counted as retired
**Stall Bubble:** `is_bubble = true`, NOT counted as retired

In pipeline visualization:
- Real instruction: Shows actual instruction (e.g., "addi x1,x0,10")
- Bubble: Shows "nop"

## Register File Rules

1. **x0 always zero**: Writes ignored, reads return 0
2. **x1-x31 general purpose**: Normal read/write
3. **Writing at WB**: Only WB stage writes registers
4. **Reading at ID**: ID stage reads from register file

## Memory Model

- **Data section**: Starts at 0x10000000
- **Text section**: Starts at 0x0
- **Alignment**: Word=4 bytes, Halfword=2 bytes, Byte=1 byte
- **Sign extension**: LB, LH, LW (signed), LBU, LHU, LWU (unsigned)

## Quick Mode Comparison

| Feature | Mode 2 | Mode 3 | Mode 4 |
|---------|--------|--------|--------|
| Hazard Detection | ❌ | ✅ | ✅ |
| Stalls | ❌ | ✅ Many | ✅ Minimal |
| Forwarding | ❌ | ❌ | ✅ |
| Correct Results | ❌ With hazards | ✅ | ✅ |
| Performance | Fast | Slow | Fast |

## Validation Checklist (Per Test)

- [ ] All registers have expected values
- [ ] Memory has expected values
- [ ] Cycle count reasonable for mode
- [ ] CPI in expected range
- [ ] No crashes or errors
- [ ] Pipeline drains completely
- [ ] Undo restores state
- [ ] Redo reapplies changes

## Performance Targets

**Mode 2:** CPI = 1.4 (ideal pipeline, no hazards)
**Mode 3:** CPI = 1.4 to 3.0 (depends on hazards)
**Mode 4:** CPI = 1.4 to 1.5 (forwarding reduces stalls)

## Contact

If issues persist:
1. Check TEST_CASES.md for expected behavior
2. Review ANALYSIS_AND_TESTING.md for detailed explanations
3. Run ./run_tests.sh for automated validation
4. Check /tmp/test_output_*.txt for detailed logs
