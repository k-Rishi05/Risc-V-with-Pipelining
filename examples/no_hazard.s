lui x10, 0x10000         # x10 = 0x10000000 (data base)
addi x11, x0, 42         # x11 = 42 (producer)
addi x12, x0, 100        # x12 = 100 (independent)
addi x0, x0, 0           # nop (extra to avoid RAW on x10 for SW)
sw x11, 0(x10)           # MEM store (no immediate consumer)
addi x0, x0, 0           # nop
addi x0, x0, 0           # nop
lw x13, 0(x10)           # load 42 into x13 (producer)
addi x0, x0, 0           # nop (load-use separation)
addi x0, x0, 0           # nop (load-use separation)
add x14, x13, x12        # consumer of x13; safe after two nops
addi x5, x0, 7           # producer
addi x7, x0, 3           # producer
addi x0, x0, 0           # nop
addi x0, x0, 0           # nop
or x8, x5, x7            # consumer; safe after two nops
addi x0, x0, 0           # nop
addi x0, x0, 0           # nop
sll x9, x8, x7           # further consumer; safe after spacing
