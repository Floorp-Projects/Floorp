                csect   CODE{PR}
#
#   XPTC_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
#                   PRUint32 paramCount, nsXPTCVariant* params)
#

		import	.invoke_count_words
		import	.invoke_copy_to_stack
		import	.__ptr_glue

.XPTC_InvokeByIndex:
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
		stwu	sp,-136(sp)				#  = 24 for linkage area,  8 * 13 for fprData area, 8 for saved registers

# set up for and call 'invoke_count_words' to get new stack size
#	
		mr	r3,r5
		bl	.invoke_count_words
		nop

# prepare args for 'invoke_copy_to_stack' call
#		
		lwz	r4,168(sp)				# paramCount
		lwz	r5,172(sp)				# params
		addi	r6,sp,128				# fprData
		slwi	r3,r3,2				        # number of bytes of stack required
		mr	r31,sp					# save original stack top
		sub	sp,sp,r3				# bump the stack
		addi	r3,sp,28				# parameter pointer excludes linkage area size + 'this'
		
		bl	.invoke_copy_to_stack
		nop
		
		lfd	f1,128(r31)				
		lfd	f2,120(r31)				
		lfd	f3,112(r31)				
		lfd	f4,104(r31)				
		lfd	f5,96(r31)				
		lfd	f6,88(r31)				
		lfd	f7,80(r31)				
		lfd	f8,72(r31)				
		lfd	f9,64(r31)				
		lfd	f10,56(r31)				
		lfd	f11,48(r31)				
		lfd	f12,40(r31)				
		lfd	f13,32(r31)				
		
		lwz	r3,160(r31)				# that
		lwz	r4,0(r3)				# get vTable from 'that'
		lwz	r5,164(r31)				# methodIndex
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
		lwz	r0,144(sp)
		addi    sp,sp,136
		mtlr    r0
		lwz     r31,-4(sp)
        blr


		csect	DATA
        import	TOC
		export 	.XPTC_InvokeByIndex
		
		dc.l	.XPTC_InvokeByIndex
		dc.l	TOC
		