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

	.data
	.align 2
#
# on entry SharedStub has the method selector in r12, the rest of the original
# parameters are in r3 thru r10 and f1 thru f13
#
.globl _SharedStub
	
_SharedStub:
	mflr	r0
	stw	r0,8(r1)

	stwu	r1,-176(r1)

	stw	r4,44(r1)
	stw	r5,48(r1)
	stw	r6,52(r1)
	stw	r7,56(r1)
	stw	r8,60(r1)
	stw	r9,64(r1)
	stw	r10,68(r1)
	stfd	f1,72(r1)
	stfd	f2,80(r1)
	stfd	f3,88(r1)
	stfd	f4,96(r1)
	stfd	f5,104(r1)
	stfd	f6,112(r1)
	stfd	f7,120(r1)
	stfd	f8,128(r1)
	stfd	f9,136(r1)
	stfd	f10,144(r1)
	stfd	f11,152(r1)
	stfd	f12,156(r1)
	stfd	f13,164(r1)

	addi	r6,r1,44
	addi	r7,r1,72

	mr	r4,r12
	addi	r5,r1,232

	bl	_PrepareAndDispatch
	nop
    
	lwz	r0,184(r1)
	addi	r1,r1,176
	mtlr	r0
	blr
