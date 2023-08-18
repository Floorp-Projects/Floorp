# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

    .set NGPRGES, 8
    .set NFPREGS, 8

    .text
    .globl SharedStub
    .hidden SharedStub
    .type  SharedStub,@function

SharedStub:
    .cfi_startproc
    move      $t0, $sp
    addi.d    $sp, $sp, -8*(NGPRGES+NFPREGS)-16
    .cfi_def_cfa_offset 8*(NGPRGES+NFPREGS)+16
    st.d      $a0, $sp, 0
    st.d      $a1, $sp, 8
    st.d      $a2, $sp, 16
    st.d      $a3, $sp, 24
    st.d      $a4, $sp, 32
    st.d      $a5, $sp, 40
    st.d      $a6, $sp, 48
    st.d      $a7, $sp, 56
    fst.d     $fa0, $sp, 64
    fst.d     $fa1, $sp, 72
    fst.d     $fa2, $sp, 80
    fst.d     $fa3, $sp, 88
    fst.d     $fa4, $sp, 96
    fst.d     $fa5, $sp, 104
    fst.d     $fa6, $sp, 112
    fst.d     $fa7, $sp, 120
    st.d      $ra, $sp, 136
    .cfi_offset 1, 136

    /* methodIndex is passed from stub */
    move      $a1, $t6
    move      $a2, $t0
    move      $a3, $sp
    addi.d    $a4, $sp, 8*NGPRGES

    bl        PrepareAndDispatch

    ld.d      $ra, $sp, 136
    .cfi_restore 1
    addi.d    $sp, $sp, 8*(NGPRGES+NFPREGS)+16
    .cfi_def_cfa_offset -8*(NGPRGES+NFPREGS)-16
    jirl      $zero, $ra, 0
    .cfi_endproc

    .size SharedStub, .-SharedStub
    .section .note.GNU-stack, "", @progbits
