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
.set BO_IF,12
.set CR0_EQ,2

        .rename H.10.NO_SYMBOL{PR},""
        .rename H.18.NS_InvokeByIndex{TC},"NS_InvokeByIndex"


# .text section

        .csect  H.10.NO_SYMBOL{PR}
        .globl  .NS_InvokeByIndex
        .globl  NS_InvokeByIndex{DS}
        .extern .invoke_copy_to_stack
        .extern ._ptrgl{PR}

#
#   NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
#                      uint32_t paramCount, nsXPTCVariant* params)
#

.NS_InvokeByIndex:
        mflr    r0
        std     r31,-8(sp)
#
# save off the incoming values in the caller's parameter area
#		
        std     r3,48(sp)       # that
        std     r4,56(sp)       # methodIndex
        std     r5,64(sp)       # paramCount
        std     r6,72(sp)       # params
	std     r0,16(sp)
	stdu    sp,-168(sp)     # 2*24=48 for linkage area, 
                                # 8*13=104 for fprData area
                                #      16 for saved registers

# prepare args for 'invoke_copy_to_stack' call
#		
        ld      r4,232(sp)      # paramCount (168+8+56)
        ld      r5,240(sp)      # params
        mr      r6,sp           # fprData
        sldi    r3,r4,3         # number of bytes of stack required
                                # is at most numParams*8
        addi    r3,r3,56        # linkage area (48) + this (8)
        mr      r31,sp          # save original stack top
        subfc   sp,r3,sp        # bump the stack
        addi    r3,sp,56        # parameter pointer excludes linkage area
                                # size + 'this'

        bl      .invoke_copy_to_stack
        nop

        lfd     f1,0(r31)       # Restore floating point registers	
	lfd     f2,8(r31)
        lfd     f3,16(r31)
        lfd     f4,24(r31)
        lfd     f5,32(r31)
        lfd     f6,40(r31)
        lfd     f7,48(r31)
        lfd     f8,56(r31)
        lfd     f9,64(r31)
        lfd     f10,72(r31)
        lfd     f11,80(r31)
        lfd     f12,88(r31)
        lfd     f13,96(r31)
		
        ld      r3,216(r31)     # that (168+48)
        ld      r4,0(r3)        # get vTable from 'that'
        ld      r5,224(r31)     # methodIndex (168+56)
        sldi    r5,r5,3         # methodIndex * 8
                                # No junk at the start of 64bit vtable !!!
        ldx     r11,r5,r4       # get function pointer (this jumps
                                # either to the function if no adjustment
                                # is needed (displacement = 0), or it 
                                # jumps to the thunk code, which will jump
                                # to the function at the end)
        
        # No adjustment of the that pointer in 64bit mode, this is done
        # by the thunk code

        ld      r4,56(sp)
        ld      r5,64(sp)
        ld      r6,72(sp)
        ld      r7,80(sp)
        ld      r8,88(sp)
        ld      r9,96(sp)
        ld      r10,104(sp)

        bl      ._ptrgl{PR}
        nop

        mr      sp,r31
        ld      r0,184(sp)      # 168+16
        addi    sp,sp,168
        mtlr    r0
        ld      r31,-8(sp)
        blr

# .data section

        .toc                            # 0x00000038
T.18.NS_InvokeByIndex:
        .tc     H.18.NS_InvokeByIndex{TC},NS_InvokeByIndex{DS}

        .csect  NS_InvokeByIndex{DS}
        .llong  .NS_InvokeByIndex     # "\0\0\0\0"
        .llong  TOC{TC0}                # "\0\0\0008"
        .llong  0x00000000              # "\0\0\0\0"
# End   csect   NS_InvokeByIndex{DS}

# .bss section
