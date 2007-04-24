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
	.globl NS_InvokeByIndex_P
	.type  NS_InvokeByIndex_P,@function

//
// NS_InvokeByIndex_P(nsISupports* that, PRUint32 methodIndex,
//                  PRUint32 paramCount, nsXPTCVariant* params)
//

NS_InvokeByIndex_P:
	stwu    sp,-32(sp)			// setup standard stack frame
	mflr    r0				// save LR
	stw     r3,8(sp)			// r3 <= that
	stw     r4,12(sp)			// r4 <= methodIndex
	stw     r30,16(sp)
	stw     r31,20(sp)

	stw     r0,36(sp)			// store LR backchain
	mr      r31,sp

	rlwinm  r10,r5,3,0,27			// r10 = (ParamCount * 2 * 4) & ~0x0f
	addi    r0,r10,96			// reserve stack for GPR and FPR register save area r0 = r10 + 96
	lwz     r9,0(sp)			// r9 = backchain
	neg     r0,r0
	stwux   r9,sp,r0			// reserve stack space and save SP backchain

	addi    r3,sp,8				// r3 <= args
	mr      r4,r5				// r4 <= paramCount
	mr      r5,r6				// r5 <= params
	add     r6,r3,r10			// r6 <= gpregs ( == args + r10 )
	mr      r30,r6				// store in r30 for use later...
	addi    r7,r6,32			// r7 <= fpregs ( == gpregs + 32 )

	bl      invoke_copy_to_stack@local	// (args, paramCount, params, gpregs, fpregs)

	lfd     f1,32(r30)			// load FP registers with method parameters
	lfd     f2,40(r30)   
	lfd     f3,48(r30)  
	lfd     f4,56(r30)  
	lfd     f5,64(r30)  
	lfd     f6,72(r30)  
	lfd     f7,80(r30)  
	lfd     f8,88(r30)

	lwz     r3,8(r31)			// r3 <= that
	lwz     r4,12(r31)			// r4 <= methodIndex
	lwz     r5,0(r3)			// r5 <= vtable ( == *that )
#if !((__GNUC__ == 3 && __GNUC_MINOR__ < 2) || __GXX_ABI_VERSION  >= 100) // G++ pre-V3 ABI
	addi	r4,r4,2				// skip first two vtable entries
#endif
	slwi    r4,r4,2				// convert to offset ( *= 4 )
	lwzx    r0,r5,r4			// r0 <= methodpointer ( == vtable + offset )

        lwz     r4,4(r30)			// load GP regs with method parameters
	lwz     r5,8(r30)   
	lwz     r6,12(r30)  
	lwz     r7,16(r30)  
	lwz     r8,20(r30)  
	lwz     r9,24(r30)  
	lwz     r10,28(r30)

	mtlr    r0				// copy methodpointer to LR    
	blrl					// call method
	
	lwz     r30,16(r31)			// restore r30 & r31
	lwz     r31,20(r31)
	
	lwz     r11,0(sp)			// clean up the stack
	lwz     r0,4(r11)
	mtlr    r0
	mr      sp,r11
	blr

/* Magic indicating no need for an executable stack */
.section .note.GNU-stack, "", @progbits ; .previous
