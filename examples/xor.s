# MULTIPLE INTERLEAVED XOR PATTERNS
# 3 branches with XOR relationships that confuse perceptron
# Pattern period = 256 iterations (too long for h=8 to fully capture)

addi  s0, zero, 0      # i = 0  
lui   s1, 2            # s1 = 8192
addi  s1, s1, 1808     # s1 = 10000

loop:
    # Branch 1: Based on bit pattern of i
    andi  t0, s0, 7       # bits 2:0
    slti  t1, t0, 4
    bne   t1, zero, b1
b1:

    # Branch 2: Based on different bits
    srli  t2, s0, 3
    andi  t2, t2, 7       # bits 5:3
    slti  t3, t2, 4
    bne   t3, zero, b2
b2:

    # Branch 3: XOR of previous two outcomes
    xor   t4, t1, t3
    bne   t4, zero, b3
b3:

    # -------------------------------------------------
    # Loop control
    # -------------------------------------------------
    addi  s0, s0, 1         # i++
    
    # B_LOOP: 'blt s0, s1, loop'
    # This branch is almost always TAKEN
    blt   s0, s1, loop

done:
    nop