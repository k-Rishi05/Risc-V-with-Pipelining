#!/bin/bash
cd /Users/Rishi/Desktop/riscv-simulator-2
echo "Testing Mode 3 (stall-only):"
echo -e "c e m 3\nload examples/p1.s\nrun\nexit" | ./build/vm --start-vm 2>&1 | grep -E "(STALL|HazardDet|VM_STATS|Program Counter: 28)"
echo ""
echo "Testing Mode 4 (forwarding):"
echo -e "c e m 4\nload examples/p1.s\nrun\nexit" | ./build/vm --start-vm 2>&1 | grep -E "(STALL|HazardDet|VM_STATS|Program Counter: 28)"
