/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef __APPLE__
#define SYM(x) _ ## x
#else
#define SYM(x) x
#endif

            .text
            .align 2
            .globl SYM(_NS_InvokeByIndex)
#ifndef __APPLE__
            .type  _NS_InvokeByIndex,@function
#endif

/*
 * _NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
 *                   uint32_t paramCount, nsXPTCVariant* params)
 */

SYM(_NS_InvokeByIndex):
            .cfi_startproc
            # set up frame
            stp         x29, x30, [sp,#-32]!
            .cfi_adjust_cfa_offset 32
            .cfi_rel_offset x29, 0
            .cfi_rel_offset x30, 8
            mov         x29, sp
            .cfi_def_cfa_register x29
            stp         x19, x20, [sp,#16]
            .cfi_rel_offset x19, 16
            .cfi_rel_offset x20, 24

            # save methodIndex across function calls
            mov         w20, w1

            # end of stack area passed to invoke_copy_to_stack
            mov         x1, sp

            # assume 8 bytes of stack for each argument with 16-byte alignment
            add         w19, w2, #1
            and         w19, w19, #0xfffffffe
            sub         sp, sp, w19, uxth #3

            # temporary place to store args passed in r0-r7,v0-v7
            sub         sp, sp, #128

            # save 'that' on stack
            str         x0, [sp]

            # start of stack area passed to invoke_copy_to_stack
            mov         x0, sp
            bl          SYM(invoke_copy_to_stack)

            # load arguments passed in r0-r7
            ldp         x6, x7, [sp, #48]
            ldp         x4, x5, [sp, #32]
            ldp         x2, x3, [sp, #16]
            ldp         x0, x1, [sp],#64

            # load arguments passed in v0-v7
            ldp         d6, d7, [sp, #48]
            ldp         d4, d5, [sp, #32]
            ldp         d2, d3, [sp, #16]
            ldp         d0, d1, [sp],#64

            # call the method
            ldr         x16, [x0]
            add         x16, x16, w20, uxth #3
            ldr         x16, [x16]
            blr         x16

            add         sp, sp, w19, uxth #3
            .cfi_def_cfa_register sp
            ldp         x19, x20, [sp,#16]
            .cfi_restore x19
            .cfi_restore x20
            ldp         x29, x30, [sp],#32
            .cfi_adjust_cfa_offset -32
            .cfi_restore x29
            .cfi_restore x30
            ret
            .cfi_endproc

#ifndef __APPLE__
            .size _NS_InvokeByIndex, . - _NS_InvokeByIndex

            .section .note.GNU-stack, "", @progbits
#endif
