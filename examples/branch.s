addi x1, x0, 3
addi x2, x1, 10
add  x3, x2, x1
addi x4, x0, 3

loop:
bne  x4, x0, exit
addi x0, x0, 0
addi x4, x4, -1
jal  x0, loop

exit:
addi x0, x0, 10
