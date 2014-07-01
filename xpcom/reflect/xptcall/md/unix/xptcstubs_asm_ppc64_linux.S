# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

.set r0,0; .set r1,1; .set RTOC,2; .set r3,3; .set r4,4
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

#if _CALL_ELF == 2
#define STACK_PARAMS   96
#else
#define STACK_PARAMS   112
#endif

#if _CALL_ELF == 2
        .section ".text"
        .type   SharedStub,@function
        .globl  SharedStub
        # Make the symbol hidden so that the branch from the stub does
        # not go via a PLT.  This is not only better for performance,
        # but may be necessary to avoid linker errors since there is
        # no place to restore the TOC register in a sibling call.
        .hidden SharedStub
        .align 2
SharedStub:
0:      addis 2,12,(.TOC.-0b)@ha
        addi 2,2,(.TOC.-0b)@l
        .localentry SharedStub,.-SharedStub
#else
        .section ".text"
        .align 2
        .globl SharedStub
        # Make the symbol hidden so that the branch from the stub does
        # not go via a PLT.  This is not only better for performance,
        # but may be necessary to avoid linker errors since there is
        # no place to restore the TOC register in a sibling call.
        .hidden SharedStub
        .section ".opd","aw"
        .align 3

SharedStub:
        .quad   .SharedStub,.TOC.@tocbase
        .previous
        .type   SharedStub,@function

.SharedStub:
#endif
        mflr    r0

        std     r4, -56(r1)                     # Save all GPRS
        std     r5, -48(r1)
        std     r6, -40(r1)
        std     r7, -32(r1)
        std     r8, -24(r1)
        std     r9, -16(r1)
        std     r10, -8(r1)

        stfd    f13, -64(r1)                    # ... and FPRS
        stfd    f12, -72(r1)
        stfd    f11, -80(r1)
        stfd    f10, -88(r1)
        stfd    f9, -96(r1)
        stfd    f8, -104(r1)
        stfd    f7, -112(r1)
        stfd    f6, -120(r1)
        stfd    f5, -128(r1)
        stfd    f4, -136(r1)
        stfd    f3, -144(r1)
        stfd    f2, -152(r1)
        stfd    f1, -160(r1)

        subi    r6,r1,56                        # r6 --> gprData
        subi    r7,r1,160                       # r7 --> fprData
        addi    r5,r1,STACK_PARAMS              # r5 --> extra stack args

        std     r0, 16(r1)
	
        stdu    r1,-288(r1)
                                                # r3 has the 'self' pointer
                                                # already

        mr      r4,r11                          # r4 is methodIndex selector,
                                                # passed via r11 in the
                                                # nsNSStubBase::StubXX() call

        bl      PrepareAndDispatch
        nop

        ld      1,0(r1)                         # restore stack
        ld      r0,16(r1)                       # restore LR
        mtlr    r0
        blr

#if _CALL_ELF == 2
        .size   SharedStub,.-SharedStub
#else
        .size   SharedStub,.-.SharedStub
#endif

        # Magic indicating no need for an executable stack
        .section .note.GNU-stack, "", @progbits ; .previous
