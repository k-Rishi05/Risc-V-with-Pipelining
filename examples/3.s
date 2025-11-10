# Setup registers
addi  s0, zero, 0     # s0 = i = 0
addi  s1, zero, 1000  # s1 = limit = 1000

loop:
    # -------------------------------------------------
    # BRANCH 1 (B1)
    # This branch will go: T, T, NT, NT, T, T, NT, NT...
    # -------------------------------------------------
    andi  s5, s0, 3       # s5 = i % 4
    addi  s6, zero, 2     # s6 = 2
    
    # B1: 'blt s5, s6, b1_taken'
    # if (i%4 < 2), branch is TAKEN
    blt   s5, s6, b1_taken
    
b1_not_taken:
    addi  s2, zero, 0     # b1_taken = 0 (NT)
    # Unconditional jump over the 'taken' block
    beq   zero, zero, b1_end
b1_taken:
    addi  s2, zero, 1     # b1_taken = 1 (T)
b1_end:

    # -------------------------------------------------
    # BRANCH 2 (B2)
    # This branch will go: T, NT, T, NT, T, NT...
    # -------------------------------------------------
    andi  s5, s0, 1       # s5 = i % 2
    
    # B2: 'bne s5, zero, b2_taken'
    # if (i%2 != 0), branch is TAKEN
    bne   s5, zero, b2_taken

b2_not_taken:
    addi  s3, zero, 0     # b2_taken = 0 (NT)
    beq   zero, zero, b2_end
b2_taken:
    addi  s3, zero, 1     # b2_taken = 1 (T)
b2_end:

    # -------------------------------------------------
    # BRANCH 3 (B_TEST): The Perceptron Killer
    # -------------------------------------------------
    # Compute XNOR: (NOT s2) XNOR (NOT s3)
    # which is equivalent to s2 XNOR s3.
    # (A XNOR B) is 1 if A == B
    
    beq   s2, s3, xnor_is_one
    
xnor_is_zero:
    addi  s4, zero, 0     # xnor_result = 0
    beq   zero, zero, xnor_end
xnor_is_one:
    addi  s4, zero, 1     # xnor_result = 1
xnor_end:

    # B_TEST: 'bne s4, zero, b_test_taken'
    # Branch if (b1_outcome == b2_outcome)
    bne   s4, zero, b_test_taken

b_test_not_taken:
    nop
    beq   zero, zero, b_test_end
b_test_taken:
    nop
b_test_end:

    # -------------------------------------------------
    # Loop control
    # -------------------------------------------------
    addi  s0, s0, 1         # i++
    
    # B_LOOP: 'blt s0, s1, loop'
    # This branch is almost always TAKEN
    blt   s0, s1, loop

done:
    nop