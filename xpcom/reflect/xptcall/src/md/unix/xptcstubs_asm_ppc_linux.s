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

        .section ".text"
        .align 2
	.globl SharedStub
	.type  SharedStub,@function
				
SharedStub:
        stwu	1,-112(1)	# room for 
				# linkage (8),
				# gprData (32),
				# fprData (64), 
				# stack alignment(8)

        mflr	0
	stw	0,116(1)

	stw	4,12(1)         # save GP registers
	stw	5,16(1)
	stw	6,20(1)
	stw	7,24(1)
	stw	8,28(1)
	stw	9,32(1)
	stw	10,36(1)

	stfd	1,40(1)         # save FP registers
	stfd	2,48(1)
	stfd	3,56(1)
	stfd	4,64(1)
	stfd	5,72(1)
	stfd	6,80(1)
	stfd	7,88(1)
	stfd	8,96(1)

				# r3 has the 'self' pointer already
				# r4 has the 'methodIndex' selector
	addi	5,1,120		# pointer to callers args area, beyond r3-r10/f1-f8 mapped range
	addi	6,1,12		# gprData+1
	addi	7,1,40		# fprData

	bl	PrepareAndDispatch
    
	lwz	0,116(1)
	mtlr	0
	la	1,112(1)
	blr

