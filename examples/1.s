.text
main:
    # ----------------------------------
    # Register usage
    # x5  (t0)  = loop counter
    # x6  (t1)  = branch history (8 bits)
    # x7  (t2)  = toggle bit for alternating branch
    # x28 (t3)  = LFSR state
    # x29 (t4)  = scratch / counter
    # x30       = LFSR polynomial constant (46080)
    # ----------------------------------

    # load loop count = 100 (for testing)
    addi  x5, x0, 100       # x5 = 100 iterations

    addi  x6, x0, 0         # history = 0
    addi  x7, x0, 0         # toggle = 0

    # seed LFSR in x28 with simple non-zero value
    addi  x28, x0, 1        # x28 = 1 (simple seed to avoid zero state)

    addi  x29, x0, 0        # scratch/counter = 0

    # prepare LFSR polynomial constant (0xB400 for 16-bit)
    # Using simpler value that fits in immediate
    addi  x30, x0, 177      # Simple polynomial value

# ---------------- main loop ----------------
loop_top:
    # -------------------------
    # Branch A: alternating pattern
    # -------------------------
    xori  x7, x7, 1         # toggle bit
    beq   x7, x0, ALT_NOT_TAKEN
ALT_TAKEN:
    addi  x0, x0, 0         # nop
    jal   x0, ALT_CONT
ALT_NOT_TAKEN:
    addi  x0, x0, 0
ALT_CONT:

    # -------------------------
    # Branch B: pseudo-random (LFSR)
    # -------------------------
    andi  x9, x28, 1
    beq   x9, x0, LFSR_NOT
LFSR_TAKEN:
    addi  x0, x0, 0
    jal   x0, LFSR_CONT
LFSR_NOT:
    addi  x0, x0, 0
LFSR_CONT:
    # update 16-bit Galois LFSR (if MSB=1 then xor with poly)
    slli  x10, x28, 1
    srli  x11, x28, 15
    andi  x11, x11, 1
    beq   x11, x0, LFSR_NOXOR
    xor   x10, x10, x30
LFSR_NOXOR:
    add   x28, x10, x0      # update LFSR state

    # -------------------------
    # Branch C: majority-of-last-8 pattern
    # -------------------------
    add   x8, x6, x0        # copy history
    addi  x18, x0, 0        # popcount accumulator
    addi  x19, x0, 8        # loop counter = 8 bits

POPCOUNT_LOOP:
    andi  x9, x8, 1
    add   x18, x18, x9
    srli  x8, x8, 1
    addi  x19, x19, -1
    bne   x19, x0, POPCOUNT_LOOP

    addi  x20, x0, 5        # threshold = 5
    blt   x18, x20, MAJ_NOT_TAKEN
MAJ_TAKEN:
    addi  x21, x0, 1
    jal   x0, MAJ_UPDATE
MAJ_NOT_TAKEN:
    addi  x21, x0, 0
MAJ_UPDATE:
    slli  x6, x6, 1
    andi  x6, x6, 255
    or    x6, x6, x21

    beq   x21, x0, MAJ_BRANCH_NOT_TAKEN
MAJ_BRANCH_TAKEN:
    addi  x0, x0, 0
    jal   x0, MAJ_BRANCH_CONT
MAJ_BRANCH_NOT_TAKEN:
    addi  x0, x0, 0
MAJ_BRANCH_CONT:

    addi  x29, x29, 1       # increment dummy counter

    # loop decrement and continue
    addi  x5, x5, -1
    bne   x5, x0, loop_top

# -------------------------------------------
# End - store result and exit cleanly
# -------------------------------------------
halt:
    # Store final counter value in x10 (return value register)
    add   x10, x29, x0
    # Simple exit (you can add ecall here if needed)
    addi  x0, x0, 0         # nop
    addi  x0, x0, 0         # nop
    addi  x0, x0, 0         # nop
