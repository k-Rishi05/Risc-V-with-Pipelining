.text
# Load->ALU hazard: should cause 1 stall in mode 3 and 1 stall even in mode 4 (unavoidable load-use)
# Expect in mode 4: single bubble then use MEM/WB forward; x3=x1+loaded
# We place data at address 0x0 via memory init elsewhere; here use x0 base.
ld   x1, 0(x0)      # load x1 <- MEM[0]
add  x3, x1, x1     # depends on prior load -> should stall 1 then forward load result
addi x2, x0, 5      # spacer
add  x4, x3, x2     # verify forwarding of x3
