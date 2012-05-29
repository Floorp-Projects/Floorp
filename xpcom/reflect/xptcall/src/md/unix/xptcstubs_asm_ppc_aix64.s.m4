# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

.set r0,0; .set sp,1; .set RTOC,2; .set r3,3; .set r4,4
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
# Define the correct name of the stub function based on the object model
define(STUB_NAME, 
       ifelse(AIX_OBJMODEL, ibm,
              `Stub'$1`__EI14nsXPTCStubBaseFv',
              `Stub'$1`__14nsXPTCStubBaseFv'))
define(STUB_ENTRY, `
    .rename H.10.NO_SYMBOL{PR},""
    .rename H.18.'STUB_NAME($1)`{TC},"'STUB_NAME($1)`"
    .csect  H.10.NO_SYMBOL{PR}
    .globl  .'STUB_NAME($1)`
    .globl  'STUB_NAME($1)`{DS}
.'STUB_NAME($1)`:
    li      r12, '$1`
    b       .SharedStub
    nop
    .toc
T.18.'STUB_NAME($1)`:
    .tc     H.18.'STUB_NAME($1)`{TC},'STUB_NAME($1)`{DS}
    .csect  'STUB_NAME($1)`{DS}
    .llong   .'STUB_NAME($1)`
    .llong   TOC{TC0}
    .llong   0x00000000
')
define(SENTINEL_ENTRY, `')
include(xptcstubsdef.inc)
    .rename H.10.NO_SYMBOL{PR},""
    .rename H.18.SharedStub{TC},"SharedStub"
# .text section
    .csect  H.10.NO_SYMBOL{PR}
    .globl  .SharedStub
    .globl  SharedStub{DS}
    .extern .PrepareAndDispatch
.SharedStub:
    mflr    r0
    std     r0,16(sp)
    stdu    sp,-248(sp)     # room for linkage (24*2), fprData (104), gprData(28*2)
                            # outgoing params to PrepareAndDispatch (40)
    std     r4,88(sp)       # link area (48) + PrepareAndDispatch params (20)
    std     r5,96(sp)
    std     r6,104(sp)
    std     r7,112(sp)
    std     r8,120(sp)
    std     r9,128(sp)
    std     r10,136(sp)
    stfd    f1,144(sp)
    stfd    f2,152(sp)
    stfd    f3,160(sp)
    stfd    f4,168(sp)
    stfd    f5,176(sp)
    stfd    f6,184(sp)
    stfd    f7,192(sp)
    stfd    f8,200(sp)
    stfd    f9,208(sp)
    stfd    f10,216(sp)
    stfd    f11,224(sp)
    stfd    f12,232(sp)
    stfd    f13,240(sp)
    addi    r6,sp,88        # gprData
    addi    r7,sp,144       # fprData
                            # r3 has the 'self' pointer already
    mr      r4,r12          # methodIndex selector (it is now LATER)
    addi    r5,sp,360       # pointer to callers args area, beyond r3-r10
                            # mapped range
    bl      .PrepareAndDispatch
    nop
    ld      r0,264(sp)
    addi    sp,sp,248
    mtlr    r0
    blr
# .data section
    .toc                            # 0x00000038
T.18.SharedStub:
    .tc     H.18.SharedStub{TC},SharedStub{DS}
    .csect  SharedStub{DS}
    .llong   .SharedStub             # "\0\0\0\0"
    .llong   TOC{TC0}                # "\0\0\0008"
    .llong   0x00000000              # "\0\0\0\0"
# End   csect   SharedStub{DS}
# .bss section
