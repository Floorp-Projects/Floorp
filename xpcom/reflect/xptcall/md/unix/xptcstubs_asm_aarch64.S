# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef __APPLE__
#define SYM(x) _ ## x
#else
#define SYM(x) x
#endif

            .set NGPREGS,8
            .set NFPREGS,8

            .text
            .align 2
            .globl SharedStub
#ifndef __APPLE__
            .hidden SharedStub
            .type  SharedStub,@function
#endif
SharedStub:
            .cfi_startproc
            stp         x29, x30, [sp,#-16]!
            .cfi_adjust_cfa_offset 16
            .cfi_rel_offset x29, 0
            .cfi_rel_offset x30, 8
            mov         x29, sp
            .cfi_def_cfa_register x29

            sub         sp, sp, #8*(NGPREGS+NFPREGS)
            stp         x0, x1, [sp, #64+(0*8)]
            stp         x2, x3, [sp, #64+(2*8)]
            stp         x4, x5, [sp, #64+(4*8)]
            stp         x6, x7, [sp, #64+(6*8)]
            stp         d0, d1, [sp, #(0*8)]
            stp         d2, d3, [sp, #(2*8)]
            stp         d4, d5, [sp, #(4*8)]
            stp         d6, d7, [sp, #(6*8)]

            # methodIndex passed from stub
            mov         w1, w17

            add         x2, sp, #16+(8*(NGPREGS+NFPREGS))
            add         x3, sp, #8*NFPREGS
            add         x4, sp, #0

            bl          SYM(PrepareAndDispatch)

            add         sp, sp, #8*(NGPREGS+NFPREGS)
            .cfi_def_cfa_register sp
            ldp         x29, x30, [sp],#16
            .cfi_adjust_cfa_offset -16
            .cfi_restore x29
            .cfi_restore x30
            ret
            .cfi_endproc

#ifndef __APPLE__
            .size SharedStub, . - SharedStub

            .section .note.GNU-stack, "", @progbits
#endif
