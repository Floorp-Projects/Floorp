# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

.set r0,0; .set r1,1; .set r2,2; .set r3,3; .set r4,4
.set r5,5; .set r6,6; .set r7,7; .set r8,8; .set r9,9
.set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
.set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
.set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
.set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
.set r30,30; .set r31,31
.set f0,0; .set f1,1; .set f2,2; .set f3,3; .set f4,4
.set f5,5; .set f6,6; .set f7,7; .set f8,8; .set f9,9
.set f10,10; .set f11,11; .set f12,12; .set f13,13; .set f14,14
.set f15,15; .set f16,16; .set f17,17; .set f18,18; .set f19,19
.set f20,20; .set f21,21; .set f22,22; .set f23,23; .set f24,24
.set f25,25; .set f26,26; .set f27,27; .set f28,28; .set f29,29
.set f30,30; .set f31,31


#
# NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
#                    PRUint32 paramCount, nsXPTCVariant* params)
#

        .section ".toc","aw"
        .section ".text"
        .align 2
        .globl  NS_InvokeByIndex_P
        .section ".opd","aw"
        .align 3
NS_InvokeByIndex_P:
        .quad   .NS_InvokeByIndex_P,.TOC.@tocbase
        .previous
        .type   NS_InvokeByIndex_P,@function
.NS_InvokeByIndex_P:
        mflr    0
        std     0,16(r1)

        std     r29,-24(r1)
        std     r30,-16(r1)
        std     r31,-8(r1)

        mr      r29,r3                  # Save 'that' in r29
        mr      r30,r4                  # Save 'methodIndex' in r30
        mr      r31,r1                  # Save old frame

        # Allocate stack frame with space for params. Since at least the
        # first 7 parameters (not including 'that') will be in registers,
        # we don't actually need stack space for those. We must ensure
        # that the stack remains 16-byte aligned.
        #
        #  | ..128-byte stack frame.. |     | 7 GP | 13 FP | 3 NV |
        #  |               |(params)........| regs | regs  | regs |
        # (r1)...........(+112)....(+128)
        #                               (-23*8).(-16*8).(-3*8)..(r31)

        # +stack frame, -unused stack params, +regs storage, +1 for alignment
        addi    r7,r5,((112/8)-7+7+13+3+1)
        rldicr  r7,r7,3,59              # multiply by 8 and mask with ~15
        neg     r7,r7
        stdux   r1,r1,r7


        # Call invoke_copy_to_stack(PRUint64* gpregs, double* fpregs,
        #                           PRUint32 paramCount, nsXPTCVariant* s, 
        #                           PRUint64* d))

        # r5, r6 are passed through intact (paramCount, params)
        # r7 (d) has to be r1+112 -- where parameters are passed on the stack.
        # r3, r4 are above that, easier to address from r31 than from r1

        subi    r3,r31,(23*8)           # r3 --> GPRS
        subi    r4,r31,(16*8)           # r4 --> FPRS
        addi    r7,r1,112               # r7 --> params
        bl      invoke_copy_to_stack
        nop

        # Set up to invoke function

        ld      r9,0(r29)               # vtable (r29 is 'that')
        mr      r3,r29                  # self is first arg, obviously

        sldi    r30,r30,3               # Find function descriptor 
        add     r9,r9,r30
        ld      r9,0(r9)

        ld      r0,0(r9)                # Actual address from fd.
        std     r2,40(r1)               # Save r2 (TOC pointer)

        mtctr   0
        ld      r11,16(r9)              # Environment pointer from fd.
        ld      r2,8(r9)                # TOC pointer from fd.

        # Load FP and GP registers as required
        ld      r4, -(23*8)(r31) 
        ld      r5, -(22*8)(r31) 
        ld      r6, -(21*8)(r31) 
        ld      r7, -(20*8)(r31) 
        ld      r8, -(19*8)(r31) 
        ld      r9, -(18*8)(r31) 
        ld      r10, -(17*8)(r31) 

        lfd     f1, -(16*8)(r31)
        lfd     f2, -(15*8)(r31)
        lfd     f3, -(14*8)(r31)
        lfd     f4, -(13*8)(r31)
        lfd     f5, -(12*8)(r31)
        lfd     f6, -(11*8)(r31)
        lfd     f7, -(10*8)(r31)
        lfd     f8, -(9*8)(r31)
        lfd     f9, -(8*8)(r31)
        lfd     f10, -(7*8)(r31)
        lfd     f11, -(6*8)(r31)
        lfd     f12, -(5*8)(r31)
        lfd     f13, -(4*8)(r31)

        bctrl                           # Do it

        ld      r2,40(r1)               # Load our own TOC pointer
        ld      r1,0(r1)                # Revert stack frame
        ld      0,16(r1)                # Reload lr
        ld      29,-24(r1)              # Restore NVGPRS
        ld      30,-16(r1)
        ld      31,-8(r1)
        mtlr    0
        blr

        .size   NS_InvokeByIndex_P,.-.NS_InvokeByIndex_P

        # Magic indicating no need for an executable stack
        .section .note.GNU-stack, "", @progbits ; .previous
