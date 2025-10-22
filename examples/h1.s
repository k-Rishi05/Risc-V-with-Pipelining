addi x10,x0,5
add x10,x10,x10
add x10,x10,x10
test: beq x0,x0,jump
addi x10,x10,1
addi x10,x10,1

jump: addi x10,x10,1