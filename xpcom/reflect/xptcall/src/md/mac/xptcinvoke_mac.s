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
#   XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
#                   PRUint32 paramCount, nsXPTCVariant* params)
#

		import	.invoke_count_words
		import	.invoke_copy_to_stack
		import	.__ptr_glue

._XPTC_InvokeByIndex:
		mflr	r0
		stw	r31,-4(sp)
#
# save off the incoming values in the caller's parameter area
#		
		stw	r3,24(sp)				# that
		stw	r4,28(sp)				# methodIndex
		stw	r5,32(sp)				# paramCount
		stw	r6,36(sp)				# params
		stw	r0,8(sp)
		stwu	sp,-144(sp)			#  = 24 for linkage area,  8 * 13 for fprData area, 8 for saved registers,
                                    #    8 to keep stack 16-byte aligned.

# set up for and call 'invoke_count_words' to get new stack size
#	
		mr	r3,r5
		mr	r4,r6
		bl	.invoke_count_words
		nop

# prepare args for 'invoke_copy_to_stack' call
#		
		lwz	r4,176(sp)				# paramCount
		lwz	r5,180(sp)				# params
#		addi	r6,sp,128				# fprData
		mr	r6,sp					# fprData
		slwi	r3,r3,2				        # number of bytes of stack required
		addi	r3,r3,28				# linkage area
		mr	r31,sp					# save original stack top
		sub	sp,sp,r3				# bump the stack
		clrrwi sp,sp,4              # keep the stack 16-byte aligned.
		lwz	r3,0(r31)				# act like real alloca, so 0(sp) always points back to 
		stw	r3,0(sp)				# previous stack frame.
		addi	r3,sp,28				# parameter pointer excludes linkage area size + 'this'
		
		bl	.invoke_copy_to_stack
		nop
		
		lfd	f1,0(r31)				
		lfd	f2,8(r31)				
		lfd	f3,16(r31)				
		lfd	f4,24(r31)				
		lfd	f5,32(r31)				
		lfd	f6,40(r31)				
		lfd	f7,48(r31)				
		lfd	f8,56(r31)				
		lfd	f9,64(r31)				
		lfd	f10,72(r31)				
		lfd	f11,80(r31)				
		lfd	f12,88(r31)				
		lfd	f13,96(r31)				
		
		lwz	r3,168(r31)				# that
		lwz	r4,0(r3)				# get vTable from 'that'
		lwz	r5,172(r31)				# methodIndex
		slwi	r5,r5,2					# methodIndex * 4
		addi	r5,r5,8					# step over junk at start of vTable !
		lwzx	r12,r5,r4				# get function pointer
		
		lwz	r4,28(sp)
		lwz	r5,32(sp)
		lwz	r6,36(sp)
		lwz	r7,40(sp)
		lwz	r8,44(sp)
		lwz	r9,48(sp)
		lwz	r10,52(sp)
		
		bl	.__ptr_glue
		nop		
		
		
		mr      sp,r31
		lwz	r0,152(sp)
		addi    sp,sp,144
		mtlr    r0
		lwz     r31,-4(sp)
        blr

# traceback table.
	traceback:
		dc.l	0
		dc.l	0x00002040
		dc.l	0
		dc.l	(traceback - ._XPTC_InvokeByIndex)	# size of the code.
		dc.w	20					# short length of identifier.
		dc.b	'._XPTC_InvokeByIndex'

		csect	DATA
        import	TOC
		export 	._XPTC_InvokeByIndex
		
		dc.l	._XPTC_InvokeByIndex
		dc.l	TOC
		