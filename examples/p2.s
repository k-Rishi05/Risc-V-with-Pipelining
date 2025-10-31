.text
# ALU->ALU RAW hazards should be resolved by EX forwarding (no stalls in mode 4)
# Expect: x3=3, x4=6, x5=10
addi x1, x0, 1      # x1=1
addi x2, x0, 2      # x2=2
add  x3, x1, x2     # x3=3
add  x4, x3, x3     # needs forward from EX/MEM to rs1/rs2 -> 6
addi x6, x0, 4      # independent spacer
add  x5, x4, x1     # needs forward from MEM/WB for x4 -> 10


