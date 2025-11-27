        # Simple floating-point test
        # Test FP add, multiply, and load/store
        
        # Initialize some float values in memory
        lui x1, 0x40000         # x1 = 0x40000000 (2.0 in IEEE 754)
        lui x2, 0x40400         # x2 = 0x40400000 (3.0 in IEEE 754)
        
        # Store to memory
        addi x10, x0, 0x100     # memory address
        sw x1, 0(x10)           # store 2.0
        sw x2, 4(x10)           # store 3.0
        
        # Load into FP registers
        flw f1, 0(x10)          # f1 = 2.0
        flw f2, 4(x10)          # f2 = 3.0
        
        # FP add: f3 = f1 + f2 = 5.0
        fadd.s f3, f1, f2
        
        # FP multiply: f4 = f1 * f2 = 6.0
        fmul.s f4, f1, f2
        
        # Store results back
        fsw f3, 8(x10)          # store 5.0
        fsw f4, 12(x10)         # store 6.0
        
        # Load results for verification
        lw x3, 8(x10)           # x3 should contain bit pattern for 5.0
        lw x4, 12(x10)          # x4 should contain bit pattern for 6.0
