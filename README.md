# RISC-V simulator

## Initial Setup (First Time Only)

After cloning the repository from GitHub, run these commands once:

```bash
cmake -S . -B build
cmake --build build -j 4
```

This will create the build directory and compile the simulator.

---

## Running the Interactive VM

Start the VM in interactive mode:

```bash
./build/vm --start-vm
```

### Available Commands in VM Terminal

#### Change Pipeline Mode

```bash
modify_config Execution pipeline_mode <mode_number>
```

**Example:**

```bash
modify_config Execution pipeline_mode 3
```

**Shortcut:**

```bash
c e m 3
```

**Available Modes:**

- Mode 1: Single Cycle
- Mode 2: Five Stage Pipeline (without hazard handling)
- Mode 3: Five Stage Pipeline (with data forwarding)
- Mode 4: Five Stage Pipeline (with data forwarding + load-use stalls)
- Mode 5: Static Branch Prediction (Always Not-Taken)
- Mode 6: 1-Bit Dynamic Branch Prediction
- Mode 7: 2-Bit Dynamic Branch Prediction
- Mode 8: Perceptron Branch Prediction
- Mode 9: Gshare Branch Prediction

#### Load a Program

```bash
load <program_path>
```

**Example:**

```bash
load examples/matrix_multiplication.s
```

#### Execution Controls

- **`step`** or **`s`** - Execute one cycle
- **`run`** - Run program until completion and display statistics (cycles, IPC, mispredictions, etc.)
- **`undo`** or **`u`** - Go back to previous cycle
- **`redo`** or **`r`** - Move forward to next cycle (after undo)
- **`exit`** - Exit the VM

---

## Testing and Validation

### Run Validated Test Suite

Validate the simulator against predefined test cases with ground truth collected from [Ripes](https://github.com/mortbopet/Ripes):

```bash
./run_tests_validated.sh
```

**What it does:**

- Runs selected test cases from the `examples/` directory
- Validates Modes 1-5 (Single Cycle through Static Branch Prediction)
- Compares results against ground truth stored in `test_expectations.json`
- Reports PASS/FAIL for each test case

**Ground Truth Source:** Expected outputs were collected from Ripes simulator and stored in the JSON file for validation.

---

## Branch Predictor Analysis Tools

### View Predictor Performance Comparison

Run all branch prediction modes on a program and compare their performance:

```bash
./dynamic_bp.sh <file_path>
```

**Example:**

```bash
./dynamic_bp.sh examples/matrix_multiplication.s
```

**Output:** Displays a comparison table showing:

- Cycles
- Instructions retired
- CPI (Cycles Per Instruction)
- IPC (Instructions Per Cycle)
- Branch mispredictions

for each branch prediction mode (Static, 1-Bit, 2-Bit, Perceptron, Gshare).

### Generate Comparison Graphs

Create visual graphs comparing branch predictor performance:

```bash
./graph_bp.py <file_path>
```

**Example:**

```bash
./graph_bp.py examples/xor.s
```

**Output:** Generates PNG graphs showing performance metrics across different predictors.

---

## Example Programs

The `examples/` directory contains several test programs:

- `matrix_multiplication.s` - Matrix multiplication algorithm
- `quicksort.s` - Quicksort implementation
- `binary_search.s` - Binary search algorithm
- `gcd_1.s` - Greatest Common Divisor
- `branch_test.s` - Branch instruction tests
- `xor.s` - XOR branch tests
- And many more...

---

## Additional Resources

- **COMMANDS.md** - Detailed list of all VM commands

---

## Quick Workflow Example

```bash
# 1. Start the VM
./build/vm --start-vm

# 2. Set pipeline mode to 2-Bit Dynamic BP
c e m 7

# 3. Load a program
load examples/matrix_multiplication.s

# 4. Run the program
run

# 5. Exit VM (Ctrl+C or exit command)

# 6. Compare all predictors
./dynamic_bp.sh examples/matrix_multiplication.s

# 7. Generate performance graphs
./graph_bp.py examples/matrix_multiplication.s
```

---

See [Commands](COMMANDS.md) for a list of commands.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## References

- [RISC-V Specifications](https://riscv.org/specifications/)
- [Five EmbedDev ISA manual](https://five-embeddev.com/riscv-isa-manual/)
