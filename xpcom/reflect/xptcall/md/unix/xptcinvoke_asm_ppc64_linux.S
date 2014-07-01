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

# The ABI defines a fixed stack frame area of 4 doublewords (ELFv2)
# or 6 doublewords (ELFv1); the last of these doublewords is used
# as TOC pointer save area.  The fixed area is followed by a parameter
# save area of 8 doublewords (used for vararg routines), followed
# by space for parameters passed on the stack.
#
# We set STACK_TOC to the offset of the TOC pointer save area, and
# STACK_PARAMS to the offset of the first on-stack parameter.

#if _CALL_ELF == 2
#define STACK_TOC      24
#define STACK_PARAMS   96
#else
#define STACK_TOC      40
#define STACK_PARAMS   112
#endif

#
# NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
#                    uint32_t paramCount, nsXPTCVariant* params)
#

#if _CALL_ELF == 2
        .section ".text"
        .type   NS_InvokeByIndex,@function
        .globl  NS_InvokeByIndex
        .align 2
NS_InvokeByIndex:
0:      addis 2,12,(.TOC.-0b)@ha
        addi 2,2,(.TOC.-0b)@l
        .localentry NS_InvokeByIndex,.-NS_InvokeByIndex
#else
        .section ".toc","aw"
        .section ".text"
        .align 2
        .globl  NS_InvokeByIndex
        .section ".opd","aw"
        .align 3
NS_InvokeByIndex:
        .quad   .NS_InvokeByIndex,.TOC.@tocbase
        .previous
        .type   NS_InvokeByIndex,@function
.NS_InvokeByIndex:
#endif
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
        #  | (fixed area + |                | 7 GP | 13 FP | 3 NV |
        #  |  param. save) |(params)........| regs | regs  | regs |
        # (r1)......(+STACK_PARAMS)...  (-23*8).(-16*8).(-3*8)..(r31)

        # +stack frame, -unused stack params, +regs storage, +1 for alignment
        addi    r7,r5,((STACK_PARAMS/8)-7+7+13+3+1)
        rldicr  r7,r7,3,59              # multiply by 8 and mask with ~15
        neg     r7,r7
        stdux   r1,r1,r7


        # Call invoke_copy_to_stack(uint64_t* gpregs, double* fpregs,
        #                           uint32_t paramCount, nsXPTCVariant* s, 
        #                           uint64_t* d))

        # r5, r6 are passed through intact (paramCount, params)
        # r7 (d) has to be r1+STACK_PARAMS
        #        -- where parameters are passed on the stack.
        # r3, r4 are above that, easier to address from r31 than from r1

        subi    r3,r31,(23*8)           # r3 --> GPRS
        subi    r4,r31,(16*8)           # r4 --> FPRS
        addi    r7,r1,STACK_PARAMS      # r7 --> params
        bl      invoke_copy_to_stack
        nop

        # Set up to invoke function

        ld      r9,0(r29)               # vtable (r29 is 'that')
        mr      r3,r29                  # self is first arg, obviously

        sldi    r30,r30,3               # Find function descriptor 
        add     r9,r9,r30
        ld      r12,0(r9)

        std     r2,STACK_TOC(r1)        # Save r2 (TOC pointer)

#if _CALL_ELF == 2
        mtctr   r12
#else
        ld      r0,0(r12)               # Actual address from fd.
        mtctr   0
        ld      r11,16(r12)             # Environment pointer from fd.
        ld      r2,8(r12)               # TOC pointer from fd.
#endif

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

        ld      r2,STACK_TOC(r1)        # Load our own TOC pointer
        ld      r1,0(r1)                # Revert stack frame
        ld      0,16(r1)                # Reload lr
        ld      29,-24(r1)              # Restore NVGPRS
        ld      30,-16(r1)
        ld      31,-8(r1)
        mtlr    0
        blr

#if _CALL_ELF == 2
        .size   NS_InvokeByIndex,.-NS_InvokeByIndex
#else
        .size   NS_InvokeByIndex,.-.NS_InvokeByIndex
#endif

        # Magic indicating no need for an executable stack
        .section .note.GNU-stack, "", @progbits ; .previous
