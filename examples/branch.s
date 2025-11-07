addi x1, x0, 3
addi x2, x1, 10
add  x3, x2, x1
addi x4, x0, 3
sw x4, 0(x20)
lw x5,0(x20)

loop:
beq  x5, x0, exit
addi x0, x0, 0
addi x5, x5, -1
jal  x0, loop

exit:
addi x0, x0, 10
