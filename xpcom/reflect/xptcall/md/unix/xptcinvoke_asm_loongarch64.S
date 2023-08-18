/* This Source Code Form subject to the terms of Mozilla Public
 * License, v. 2.0 If a copy of the MPL was not distributed with
 * this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
  addi.d    $sp, $sp, -32
  .cfi_def_cfa_offset 32
  st.d      $s0, $sp, 16
  .cfi_offset 23, -16
  st.d      $s1, $sp, 8
  .cfi_offset 24, -24
  st.d      $s2, $sp, 0
  .cfi_offset 25, -32
  st.d      $ra, $sp, 24
  .cfi_offset 1, -8
  
  move      $s2, $a0
  move      $s1, $a1
  move      $s0, $sp
  .cfi_def_cfa_register 23

  /* 16-bytes alignment */
  addi.d    $a0, $a2, 1
  li.d      $t4, 0xfffffffffffffffe
  and       $a0, $a0, $t4
  slli.d    $a0, $a0, 3
  sub.d     $sp, $sp, $a0
  move      $a4, $sp

  addi.d    $sp, $sp, -8*(NFPREGS+NGPREGS)
  move      $a0, $sp
  addi.d    $a1, $sp, 8*NGPREGS

  bl        invoke_copy_to_stack

  /* 1st argument is this */
  move      $a0, $s2

  ld.d      $a1, $sp, 8
  ld.d      $a2, $sp, 16
  ld.d      $a3, $sp, 24
  ld.d      $a4, $sp, 32
  ld.d      $a5, $sp, 40
  ld.d      $a6, $sp, 48
  ld.d      $a7, $sp, 56

  fld.d     $fa0, $sp, 64
  fld.d     $fa1, $sp, 72
  fld.d     $fa2, $sp, 80
  fld.d     $fa3, $sp, 88
  fld.d     $fa4, $sp, 96
  fld.d     $fa5, $sp, 104
  fld.d     $fa6, $sp, 112
  fld.d     $fa7, $sp, 120

  addi.d    $sp, $sp, 8*(NGPREGS+NFPREGS)

  ld.d      $s2, $s2, 0
  slli.w    $s1, $s1, 3
  add.d     $s2, $s2, $s1
  ld.d      $t3, $s2, 0
  jirl      $ra, $t3, 0

  move      $sp, $s0
  .cfi_def_cfa_register 3
  ld.d      $s0, $sp, 16
  .cfi_restore 23
  ld.d      $s1, $sp, 8
  .cfi_restore 24
  ld.d      $s2, $sp, 0
  .cfi_restore 25
  ld.d      $ra, $sp, 24
  .cfi_restore 1
  addi.d    $sp, $sp, 32
  .cfi_def_cfa_offset -32
  jirl      $zero, $ra, 0
  .cfi_endproc
  .size   _NS_InvokeByIndex, .-_NS_InvokeByIndex
  .section .note.GNU-stack, "", @progbits

