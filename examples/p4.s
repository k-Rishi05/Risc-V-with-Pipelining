.text
# ALU->STORE data forwarding: store latest rs2 even when ALU uses imm for address
# Expect: memory at x10 (base) gets value produced by prior add
addi x10, x0, 0x100   # base address
addi x1,  x0, 7       # src A
addi x2,  x0, 8       # src B
add  x5,  x1, x2      # x5=15
sw   x5,  0(x10)      # store should forward rs2=x5 from EX/MEM/MEM/WB (no stall)
# Verify with a load back
lw   x6,  0(x10)      # x6 should read 15
