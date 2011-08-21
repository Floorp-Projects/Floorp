// -*- Mode: Asm -*-
//
// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is mozilla.org code.
//
// The Initial Developer of the Original Code is
// Netscape Communications Corporation.
// Portions created by the Initial Developer are Copyright (C) 1999
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Franz.Sirl-kernel@lauterbach.com (Franz Sirl)
//   beard@netscape.com (Patrick Beard)
//   waterson@netscape.com (Chris Waterson)
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****

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
	stwu	sp,-112(sp)			// room for
						// linkage (8),
						// gprData (32),
						// fprData (64),
						// stack alignment(8)
	mflr	r0
	stw	r0,116(sp)			// save LR backchain

	stw	r4,12(sp)			// save GP registers
	stw	r5,16(sp)			// (n.b. that we don't save r3
	stw	r6,20(sp)			// because PrepareAndDispatch() is savvy)
	stw	r7,24(sp)
	stw	r8,28(sp)
	stw	r9,32(sp)
	stw	r10,36(sp)

	stfd	f1,40(sp)			// save FP registers
	stfd	f2,48(sp)
	stfd	f3,56(sp)
	stfd	f4,64(sp)
	stfd	f5,72(sp)
	stfd	f6,80(sp)
	stfd	f7,88(sp)
	stfd	f8,96(sp)

						// r3 has the 'self' pointer already

	mr	r4,r11				// r4 <= methodIndex selector, passed
						// via r11 in the nsXPTCStubBase::StubXX() call

	addi	r5,sp,120			// r5 <= pointer to callers args area,
						// beyond r3-r10/f1-f8 mapped range

	addi	r6,sp,8				// r6 <= gprData
	addi	r7,sp,40			// r7 <= fprData

	bl	PrepareAndDispatch@local	// Go!

	lwz	r0,116(sp)			// restore LR
	mtlr	r0
	la	sp,112(sp)			// clean up the stack
	blr

// Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits ; .previous
