# MATRIX MULTIPLICATION - 20x20 matrices
# Pattern: Deeply nested loops with predictable branches
# Expected: 2-Bit and Gshare should perform best
# Why: Inner loops have strong repetitive patterns (always taken until exit)

# Matrix dimensions: 20x20
addi  a0, zero, 20    # N = 20

# Initialize result matrix to zero (not shown for brevity)

# Outer loop: i
addi  s0, zero, 0     # i = 0
outer_i:
    bge   s0, a0, done_i    # if i >= N, exit
    
    # Middle loop: j
    addi  s1, zero, 0   # j = 0
    outer_j:
        bge   s1, a0, done_j    # if j >= N, exit
        
        # Initialize sum for C[i][j]
        addi  t0, zero, 0   # sum = 0
        
        # Inner loop: k
        addi  s2, zero, 0   # k = 0
        inner_k:
            bge   s2, a0, done_k    # if k >= N, exit
            
            # C[i][j] += A[i][k] * B[k][j]
            # (Simplified - just doing additions for branch pattern)
            addi  t0, t0, 1     # sum++
            
            addi  s2, s2, 1     # k++
            blt   s2, a0, inner_k   # Branch: taken 19 times, not-taken once
        done_k:
        
        addi  s1, s1, 1     # j++
        blt   s1, a0, outer_j   # Branch: taken 19 times, not-taken once
    done_j:
    
    addi  s0, s0, 1     # i++
    blt   s0, a0, outer_i   # Branch: taken 19 times, not-taken once
done_i:

# Total branches: 20*20*20 + 20*20 + 20 = 8,420 branches
# Each branch: TTTTTTTTTTTTTTTTTTTN (19 taken, 1 not-taken)
# Strong biased pattern - perfect for 2-bit saturating counter

# Exit
addi  a0, zero, 10
ecall