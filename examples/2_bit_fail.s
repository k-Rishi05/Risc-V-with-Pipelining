# Setup registers
addi  s0, zero, 0     # s0 = i = 0
addi  s2, zero, 1000   # s2 = limit = 100
addi  s3, zero, 2     # s3 = 2

loop:
    # -------------------------------------------------
    # BRANCH 1 (B1): The Test Branch
    # -------------------------------------------------
    # We create a (T, T, N, N) pattern
    # i=0, s1=0. T
    # i=1, s1=1. T
    # i=2, s1=2. N
    # i=3, s1=3. N
    
    # s1 = i % 4
    andi  s1, s0, 3         # s1 = i & 0b11
    
    # B1: if (s1 < 2), branch
    blt   s1, s3, b1_taken
    
b1_not_taken:
    # Do some dummy work
    addi  s1, s1, 1
    beq   zero, zero, b1_end # Unconditional jump
    
b1_taken:
    # Do some different dummy work
    addi  s1, s1, -1

b1_end:
    # -------------------------------------------------
    # BRANCH 2 (B2): The Loop Branch
    # -------------------------------------------------
    addi  s0, s0, 1         # i++
    
    # B2: 'blt s0, s2, loop'
    # This branch is ALWAYS TAKEN (until the end)
    blt   s0, s2, loop

done:
    nop