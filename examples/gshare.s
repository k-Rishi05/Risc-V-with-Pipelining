# Simple test that clearly favors Gshare over Perceptron
# 
# Key strategy: 
# 1. Only TWO test branches (plus loop branch)
# 2. Simple repeating patterns (no correlation)
# 3. Different PCs so Gshare can distinguish them
# 4. Pattern periods that fit in h=8
#
# Why Gshare wins:
# - With h=8, PHT has 256 entries
# - Only 3 branches total per iteration (br1, br2, loop)
# - GHR captures ~2-3 iterations of history
# - Each (PC, GHR) pair maps to unique PHT entry
# - Simple patterns are quickly learned
#
# Why Perceptron struggles:
# - With h=8, only ~568 perceptron tables
# - Need separate perceptrons for each branch
# - Aliasing between different PCs
# - Training needs more iterations to converge

addi  s0, zero, 0      # s0 = i = 0
addi  s1, zero, 2000   # s1 = iterations (more iterations = better training)

loop:
    # Branch 1: Taken when (i % 4) < 2
    # Pattern repeats every 4: T, T, NT, NT, T, T, NT, NT...
    # This is a simple pattern that Gshare learns quickly
    andi  t0, s0, 3      # t0 = i % 4
    slti  t1, t0, 2      # t1 = 1 if (i%4) < 2
    bne   t1, zero, br1_taken
br1_taken:
    
    # Branch 2: Taken when (i % 8) < 4  
    # Pattern repeats every 8: T, T, T, T, NT, NT, NT, NT...
    # Another simple pattern, different period
    andi  t0, s0, 7      # t0 = i % 8
    slti  t1, t0, 4      # t1 = 1 if (i%8) < 4
    bne   t1, zero, br2_taken
br2_taken:
    
    # Loop increment and test
    addi  s0, s0, 1
    blt   s0, s1, loop

done:
    nop