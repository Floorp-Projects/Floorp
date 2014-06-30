	.globl NS_InvokeByIndex
	.type NS_InvokeByIndex, @function
NS_InvokeByIndex:
	push       %ebp
	movl       %esp,%ebp
	push       %ebx
	call       .CG0.66
.CG0.66:
	pop        %ebx
	addl       $_GLOBAL_OFFSET_TABLE_+0x1,%ebx
	push       20(%ebp)
	push       16(%ebp)
	push       12(%ebp)
	push       8(%ebp)
	/ INLINE: invoke_by_index



	pushl	%ebx
	pushl	%esi
	movl	%esp, %ebx

	pushl	0x14(%ebp)
	pushl	0x10(%ebp)
	call	invoke_count_words
	mov	%ebx, %esp

	sall	$0x2 , %eax
	subl	%eax, %esp
	movl	%esp, %esi

	pushl	%esp
	pushl	0x14(%ebp)
	pushl	0x10(%ebp)
	call	invoke_copy_to_stack
	movl	%esi, %esp

	movl	0x8(%ebp), %ecx
	pushl	%ecx
	movl	(%ecx), %edx
	movl	0xc(%ebp), %eax
	movl	0x8(%edx, %eax, 4), %edx

	call	*%edx
	mov	%ebx, %esp
	popl	%esi
	popl	%ebx
	/ INLINE_END
	addl       $16,%esp
	pop        %ebx
	movl       %ebp,%esp
	pop        %ebp
	ret        
	.size NS_InvokeByIndex, . - NS_InvokeByIndex

