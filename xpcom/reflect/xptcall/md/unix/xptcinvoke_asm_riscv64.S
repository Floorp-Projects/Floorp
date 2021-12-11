/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

    .set NGPREGS, 8
    .set NFPREGS, 8

    .text
    .globl  _NS_InvokeByIndex
    .type   _NS_InvokeByIndex, @function
/*
 * _NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
 *                   uint32_t paramCount, nsXPTCVariant* params)
 */
_NS_InvokeByIndex:
    .cfi_startproc
    addi    sp, sp, -32
    .cfi_adjust_cfa_offset 32
    sd      s0, 16(sp)
    .cfi_rel_offset s0, 16
    sd      s1, 8(sp)
    .cfi_rel_offset s1, 8
    sd      s2, 0(sp)
    .cfi_rel_offset s2, 0
    sd      ra, 24(sp)
    .cfi_rel_offset ra, 24

    mv      s2, a0
    mv      s1, a1
    mv      s0, sp
    .cfi_def_cfa_register s0

    /* 16-bytes alignment */
    addiw   a0, a2, 1
    andi    a0, a0, -2
    slli    a0, a0, 3
    sub     sp, sp, a0
    mv      a4, sp

    addi    sp, sp, -8*(NGPREGS+NFPREGS)
    mv      a0, sp
    addi    a1, sp, 8*NGPREGS

    call    invoke_copy_to_stack

    /* 1st argument is this */
    mv      a0, s2

    ld      a1, 8(sp)
    ld      a2, 16(sp)
    ld      a3, 24(sp)
    ld      a4, 32(sp)
    ld      a5, 40(sp)
    ld      a6, 48(sp)
    ld      a7, 56(sp)

    fld     fa0, 64(sp)
    fld     fa1, 72(sp)
    fld     fa2, 80(sp)
    fld     fa3, 88(sp)
    fld     fa4, 96(sp)
    fld     fa5, 104(sp)
    fld     fa6, 112(sp)
    fld     fa7, 120(sp)

    addi    sp, sp, 8*(NGPREGS+NFPREGS)

    ld      s2, 0(s2)
    slliw   s1, s1, 3
    add     s2, s2, s1
    ld      t0, 0(s2)
    jalr    t0

    mv      sp, s0
    .cfi_def_cfa_register sp
    ld      s0, 16(sp)
    .cfi_restore s0
    ld      s1, 8(sp)
    .cfi_restore s1
    ld      s2, 0(sp)
    .cfi_restore s2
    ld      ra, 24(sp)
    .cfi_restore ra
    addi    sp, sp, 32
    .cfi_adjust_cfa_offset -32
    ret
    .cfi_endproc
    .size   _NS_InvokeByIndex, . - _NS_InvokeByIndex
    .section .note.GNU-stack, "", @progbits
