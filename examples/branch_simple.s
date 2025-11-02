# Simple branch test
addi x4, x0, 3
bne x4, x0, exit
addi x0, x0, 0      # Should be flushed
exit:
addi x0, x0, 10
