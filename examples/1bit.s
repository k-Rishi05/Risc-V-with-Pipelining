        addi x1, x0, 7        # x1 = loop counter = 5
        addi x2, x0, 0        # x2 = accumulator = 0

loop:   addi x2, x2, 1        # x2 = x2 + 1
        addi x1, x1, -1       # x1 = x1 - 1
        bne  x1, x0, loop     # branch back to loop if x1 != 0

        addi x3, x0, 1        # branch not taken path
        addi x4, x0, 2