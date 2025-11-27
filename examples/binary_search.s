# BINARY SEARCH - Search in sorted array of 1024 elements
# Pattern: Correlated branches based on comparisons
# Expected: Perceptron should perform best
# Why: Each comparison depends on previous results (path correlation)

# Array size = 1024 (2^10)
lui   a0, 1           # a0 = 4096
addi  a0, a0, 0       # a0 = 1024 elements base address (simulated)

addi  s0, zero, 0     # Number of searches to perform
addi  s1, zero, 100   # Do 100 searches

search_loop:
    bge   s0, s1, done_searches
    
    # Binary search: looking for value (varies each iteration)
    # left = 0, right = 1023
    addi  t0, zero, 0      # left = 0
    addi  t1, zero, 1023   # right = 1023
    
    # Target value = i * 10 (varies with search number)
    slli  t6, s0, 3        # t6 = i * 8
    add   t6, t6, s0       # t6 = i * 9
    add   t6, t6, s0       # t6 = i * 10 (target)
    
    binary_search:
        bge   t0, t1, search_done   # if left >= right, done
        
        # mid = (left + right) / 2
        add   t2, t0, t1    # t2 = left + right
        srli  t2, t2, 1     # t2 = mid
        
        # Simulate array[mid] value (mid itself for simplicity)
        add   t3, t2, zero  # t3 = array[mid] = mid
        
        # Compare target with array[mid]
        blt   t6, t3, go_left    # if target < mid, go left
        
        # target >= mid, go right
        addi  t0, t2, 1     # left = mid + 1
        beq   zero, zero, continue_search
        
    go_left:
        addi  t1, t2, -1    # right = mid - 1
        
    continue_search:
        blt   t0, t1, binary_search   # Branch: correlated with path
    
    search_done:
    
    addi  s0, s0, 1     # Next search
    blt   s0, s1, search_loop

done_searches:

# Total: ~100 searches * ~10 comparisons each = ~1000 branches
# Pattern: Each branch depends on previous comparison results
# Perceptron learns the correlation between branches in the search path

addi  a0, zero, 10
ecall
