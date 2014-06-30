# -*- Mode: Asm -*-
#
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

        .section ".text"
        .align 2
	.globl SharedStub
	.type  SharedStub,@function

SharedStub:
        stwu	sp,-112(sp)			# room for 
						# linkage (8),
						# gprData (32),
						# fprData (64), 
						# stack alignment(8)
        mflr	r0
	stw	r0,116(sp)			# save LR backchain

	stw	r4,12(sp)			# save GP registers
	stw	r5,16(sp)			# (n.b. that we don't save r3
	stw	r6,20(sp)			# because PrepareAndDispatch() is savvy)
	stw	r7,24(sp)
	stw	r8,28(sp)
	stw	r9,32(sp)
	stw	r10,36(sp)

	stfd	f1,40(sp)			# save FP registers
	stfd	f2,48(sp)
	stfd	f3,56(sp)
	stfd	f4,64(sp)
	stfd	f5,72(sp)
	stfd	f6,80(sp)
	stfd	f7,88(sp)
	stfd	f8,96(sp)

						# r3 has the 'self' pointer already
	
	mr      r4,r11				# r4 <= methodIndex selector, passed
						# via r11 in the nsXPTCStubBase::StubXX() call
	
	addi	r5,sp,120			# r5 <= pointer to callers args area,
						# beyond r3-r10/f1-f8 mapped range
	
	addi	r6,sp,8				# r6 <= gprData
	addi	r7,sp,40			# r7 <= fprData
      
	bl	PrepareAndDispatch@local	# Go!
    
	lwz	r0,116(sp)			# restore LR
	mtlr	r0
	la	sp,112(sp)			# clean up the stack
	blr

