# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

    .set NGPREGS, 8
    .set NFPREGS, 8

    .text
    .globl SharedStub
    .hidden SharedStub
    .type  SharedStub,@function

SharedStub:
    .cfi_startproc
    mv      t1, sp
    addi    sp, sp, -8*(NGPREGS+NFPREGS)-16
    .cfi_adjust_cfa_offset 8*(NGPREGS+NFPREGS)+16
    sd      a0, 0(sp)
    sd      a1, 8(sp)
    sd      a2, 16(sp)
    sd      a3, 24(sp)
    sd      a4, 32(sp)
    sd      a5, 40(sp)
    sd      a6, 48(sp)
    sd      a7, 56(sp)
    fsd     fa0, 64(sp)
    fsd     fa1, 72(sp)
    fsd     fa2, 80(sp)
    fsd     fa3, 88(sp)
    fsd     fa4, 96(sp)
    fsd     fa5, 104(sp)
    fsd     fa6, 112(sp)
    fsd     fa7, 120(sp)
    sd      ra, 136(sp)
    .cfi_rel_offset ra, 136

    /* methodIndex is passed from stub */
    mv      a1, t0
    mv      a2, t1
    mv      a3, sp
    addi    a4, sp, 8*NGPREGS

    call    PrepareAndDispatch

    ld      ra, 136(sp)
    .cfi_restore ra
    addi    sp, sp, 8*(NGPREGS+NFPREGS)+16
    .cfi_adjust_cfa_offset -8*(NGPREGS+NFPREGS)-16
    ret
    .cfi_endproc

    .size SharedStub, . - SharedStub
    .section .note.GNU-stack, "", @progbits
