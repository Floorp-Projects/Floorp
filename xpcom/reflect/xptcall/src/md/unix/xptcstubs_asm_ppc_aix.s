 #
 # -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 #
 # The contents of this file are subject to the Netscape Public
 # License Version 1.1 (the "License"); you may not use this file
 # except in compliance with the License. You may obtain a copy of
 # the License at http://www.mozilla.org/NPL/
 #
 # Software distributed under the License is distributed on an "AS
 # IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 # implied. See the License for the specific language governing
 # rights and limitations under the License.
 #
 # The Original Code is mozilla.org code.
 #
 # The Initial Developer of the Original Code is Netscape
 # Communications Corporation.  Portions created by Netscape are
 # Copyright (C) 1999 Netscape Communications Corporation. All
 # Rights Reserved.
 #
 # Contributor(s): 
 #

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



        .rename H.10.NO_SYMBOL{PR},""
        .rename H.18.SharedStub{TC},"SharedStub"


# .text section

        .csect  H.10.NO_SYMBOL{PR}
        .globl  .SharedStub
        .globl  SharedStub{DS}
        .extern .PrepareAndDispatch


#jimbo        csect   CODE{PR}
#
# on entry SharedStub has the method selector in r12, the rest of the original
# parameters are in r3 thru r10 and f1 thru f13
#
#jimbo	import	.PrepareAndDispatch

.SharedStub:
	mflr	r0
	stw	r0,8(sp)

 	mr	r12,r3		    # Move methodIndex into r12 for LATER
 	lwz	r3,104(sp)	    # Get the 'original' r3

        stwu	sp,-176(sp)	    # room for linkage (24), fprData (104), gprData(28)
				    # outgoing params to PrepareAndDispatch (20)

	stw	r4,44(sp)
	stw	r5,48(sp)
	stw	r6,52(sp)
	stw	r7,56(sp)
	stw	r8,60(sp)
	stw	r9,64(sp)
	stw	r10,68(sp)
	stfd	f1,72(sp)
	stfd	f2,80(sp)
	stfd	f3,88(sp)
	stfd	f4,96(sp)
	stfd	f5,104(sp)
	stfd	f6,112(sp)
	stfd	f7,120(sp)
	stfd	f8,128(sp)
	stfd	f9,136(sp)
	stfd	f10,144(sp)
	stfd	f11,152(sp)
	stfd	f12,156(sp)
	stfd	f13,164(sp)

	addi	r6,sp,44	# gprData
	addi	r7,sp,72	# fprData
				# r3 has the 'self' pointer already
	mr	r4,r12		# methodIndex selector (it is now LATER)
	addi	r5,sp,312	# pointer to callers args area, beyond r3-r10 mapped range

	bl	.PrepareAndDispatch
	nop
    
	lwz	r0,184(sp)
	addi	sp,sp,176
	mtlr	r0
	blr

# .data section

        .toc                            # 0x00000038
T.18.SharedStub:
        .tc     H.18.SharedStub{TC},SharedStub{DS}

        .csect  SharedStub{DS}
        .long   .SharedStub             # "\0\0\0\0"
        .long   TOC{TC0}                # "\0\0\0008"
        .long   0x00000000              # "\0\0\0\0"
# End   csect   SharedStub{DS}

# .bss section
