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

        csect   CODE{PR}
#
# on entry SharedStub has the method selector in r12, the rest of the original
# parameters are in r3 thru r10 and f1 thru f13
#
	import	.PrepareAndDispatch

.SharedStub:
	mflr	r0
	stw	r0,8(sp)

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
	mr	r4,r12		# methodIndex selector
	addi	r5,sp,232	# pointer to callers args area, beyond r3-r10 mapped range

	bl	.PrepareAndDispatch
	nop
    
	lwz	r0,184(sp)
	addi	sp,sp,176
	mtlr	r0
	blr
	
	csect	DATA
    import	TOC
	export 	.SharedStub
		
	dc.l	.SharedStub
	dc.l	TOC
