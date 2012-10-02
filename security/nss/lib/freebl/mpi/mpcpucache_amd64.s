/ This Source Code Form is subject to the terms of the Mozilla Public
/ License, v. 2.0. If a copy of the MPL was not distributed with this
/ file, You can obtain one at http://mozilla.org/MPL/2.0/.

	.file	"mpcpucache.c"
/	.section	.rodata.str1.1,"aMS",@progbits,1
	.section	.rodata
.LC0:
	.string	"GenuineIntel"
.LC1:
	.string	"AuthenticAMD"
.LC2:
	.string	"CyrixInstead"
.LC3:
	.string	"CentaurHauls"
.LC4:
	.string	"NexGenDriven"
.LC5:
	.string	"GenuineTMx86"
.LC6:
	.string	"RiseRiseRise"
.LC7:
	.string	"UMC UMC UMC "
.LC8:
	.string	"Sis Sis Sis "
.LC9:
	.string	"Geode by NSC"
	.section	.data.rel.ro.local,"aw",@progbits
	.align 32
	.type	manMap, @object
	.size	manMap, 80
manMap:
	.quad	.LC0
	.quad	.LC1
	.quad	.LC2
	.quad	.LC3
	.quad	.LC4
	.quad	.LC5
	.quad	.LC6
	.quad	.LC7
	.quad	.LC8
	.quad	.LC9
	.section	.rodata
	.align 32
	.type	CacheMap, @object
	.size	CacheMap, 512
CacheMap:
	.byte	0
	.byte	0
	.byte	3
	.byte	0
	.byte	3
	.byte	0
	.byte	4
	.byte	0
	.byte	4
	.zero	1
	.byte	1
	.byte	0
	.byte	7
	.byte	32
	.byte	1
	.byte	0
	.byte	7
	.byte	32
	.byte	1
	.byte	0
	.byte	8
	.byte	32
	.byte	1
	.byte	0
	.byte	8
	.byte	32
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	12
	.byte	64
	.byte	12
	.byte	64
	.byte	1
	.byte	0
	.byte	12
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	12
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	8
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	7
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	9
	.byte	64
	.byte	1
	.byte	0
	.byte	9
	.byte	64
	.byte	9
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	9
	.byte	0
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	3
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	4
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	8
	.byte	64
	.byte	8
	.byte	64
	.byte	8
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	5
	.byte	1
	.byte	5
	.byte	1
	.byte	5
	.byte	1
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	9
	.byte	64
	.byte	9
	.byte	64
	.byte	9
	.byte	64
	.byte	9
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	9
	.byte	32
	.byte	9
	.byte	64
	.byte	9
	.byte	64
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	3
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	4
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.byte	1
	.byte	0
	.text
	.align	16
.globl freebl_cpuid
	.type	freebl_cpuid, @function
freebl_cpuid:
.LFB2:
	movq	%rdx, %r10
	pushq	%rbx
.LCFI0:
	movq	%rcx, %r11
	movq	%rdi, %rax
/APP
	cpuid
	
/NO_APP
	movq	%rax, (%rsi)
	movq	%rbx, (%r10)
	popq	%rbx
	movq	%rcx, (%r11)
	movq	%rdx, (%r8)
	ret
.LFE2:
	.size	freebl_cpuid, .-freebl_cpuid
	.align	16
	.type	getIntelCacheEntryLineSize, @function
getIntelCacheEntryLineSize:
.LFB3:
	leaq	CacheMap(%rip), %r9
	movq	%rdx, %r10
	movzbl	1(%r9,%rdi,2), %ecx
	movzbl	(%r9,%rdi,2), %r8d
	testb	%cl, %cl
	je	.L2
	cmpl	$6, %r8d
	sete	%dl
	cmpl	$8, %r8d
	sete	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L4
	movl	$1, (%rsi)
.L9:
	movzbl	%cl, %eax 
	movq	%rax, (%r10)
	ret
	.align	16
.L4:
	movl	(%rsi), %r11d
	cmpl	$1, %r11d
	jg	.L11
.L6:
	cmpl	$2, %r11d
	jle	.L2
	cmpl	$12, %r8d
	sete	%dl
	cmpl	$14, %r8d
	sete	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L2
	movzbq	1(%r9,%rdi,2), %rax
	movl	$3, (%rsi)
	movq	%rax, (%r10)
	.align	16
.L2:
	rep ; ret
	.align	16
.L11:
	cmpl	$9, %r8d
	sete	%dl
	cmpl	$11, %r8d
	sete	%al
	orl	%edx, %eax
	testb	$1, %al
	je	.L6
	movl	$2, (%rsi)
	jmp	.L9
.LFE3:
	.size	getIntelCacheEntryLineSize, .-getIntelCacheEntryLineSize
	.align	16
	.type	getIntelRegisterCacheLineSize, @function
getIntelRegisterCacheLineSize:
.LFB4:
	pushq	%rbp
.LCFI1:
	movq	%rsp, %rbp
.LCFI2:
	movq	%rbx, -24(%rbp)
.LCFI3:
	movq	%rdi, %rbx
	shrq	$24, %rdi
	movq	%r12, -16(%rbp)
.LCFI4:
	movq	%r13, -8(%rbp)
.LCFI5:
	andl	$255, %edi
	subq	$24, %rsp
.LCFI6:
	movq	%rsi, %r13
	movq	%rdx, %r12
	call	getIntelCacheEntryLineSize
	movq	%rbx, %rdi
	movq	%r12, %rdx
	movq	%r13, %rsi
	shrq	$16, %rdi
	andl	$255, %edi
	call	getIntelCacheEntryLineSize
	movq	%rbx, %rdi
	movq	%r12, %rdx
	movq	%r13, %rsi
	shrq	$8, %rdi
	andl	$255, %ebx
	andl	$255, %edi
	call	getIntelCacheEntryLineSize
	movq	%r12, %rdx
	movq	%r13, %rsi
	movq	%rbx, %rdi
	movq	8(%rsp), %r12
	movq	(%rsp), %rbx
	movq	16(%rsp), %r13
	leave
	jmp	getIntelCacheEntryLineSize
.LFE4:
	.size	getIntelRegisterCacheLineSize, .-getIntelRegisterCacheLineSize
	.align	16
.globl s_mpi_getProcessorLineSize
	.type	s_mpi_getProcessorLineSize, @function
s_mpi_getProcessorLineSize:
.LFB7:
	pushq	%rbp
.LCFI7:
	xorl	%edi, %edi
	movq	%rsp, %rbp
.LCFI8:
	pushq	%r15
.LCFI9:
	leaq	-136(%rbp), %r8
	leaq	-144(%rbp), %rcx
	leaq	-152(%rbp), %rdx
	pushq	%r14
.LCFI10:
	leaq	-160(%rbp), %rsi
	leaq	-128(%rbp), %r14
	pushq	%r13
.LCFI11:
	leaq	manMap(%rip), %r13
	pushq	%r12
.LCFI12:
	movl	$9, %r12d
	pushq	%rbx
.LCFI13:
	xorl	%ebx, %ebx
	subq	$200, %rsp
.LCFI14:
	call	freebl_cpuid
	movq	-152(%rbp), %rax
	movq	-160(%rbp), %r15
	movb	$0, -116(%rbp)
	movl	%eax, -128(%rbp)
	movq	-136(%rbp), %rax
	movl	%eax, -124(%rbp)
	movq	-144(%rbp), %rax
	movl	%eax, -120(%rbp)
	.align	16
.L18:
	movslq	%ebx,%rax
	movq	%r14, %rsi
	movq	(%r13,%rax,8), %rdi
	call	strcmp@PLT
	testl	%eax, %eax
	cmove	%ebx, %r12d
	incl	%ebx
	cmpl	$9, %ebx
	jle	.L18
	testl	%r12d, %r12d
	jne	.L19
	xorl	%eax, %eax
	decl	%r15d
	movl	$4, -204(%rbp)
	movq	$0, -200(%rbp)
	jle	.L21
	leaq	-168(%rbp), %r8
	leaq	-176(%rbp), %rcx
	leaq	-184(%rbp), %rdx
	leaq	-192(%rbp), %rsi
	movl	$2, %edi
	xorl	%ebx, %ebx
	call	freebl_cpuid
	movq	-192(%rbp), %rdi
	movl	%edi, %r12d
	andl	$15, %r12d
	cmpl	%r12d, %ebx
	jl	.L30
	jmp	.L38
	.align	16
.L25:
	movq	-184(%rbp), %rdi
	testl	$2147483648, %edi 
	je	.L40
.L26:
	movq	-176(%rbp), %rdi
	testl	$2147483648, %edi 
	je	.L41
.L27:
	movq	-168(%rbp), %rdi
	testl	$2147483648, %edi 
	je	.L42
.L28:
	incl	%ebx
	cmpl	%r12d, %ebx
	je	.L24
	leaq	-168(%rbp), %r8
	leaq	-176(%rbp), %rcx
	leaq	-184(%rbp), %rdx
	leaq	-192(%rbp), %rsi
	movl	$2, %edi
	call	freebl_cpuid
.L24:
	cmpl	%r12d, %ebx
	jge	.L38
	movq	-192(%rbp), %rdi
.L30:
	testl	$2147483648, %edi 
	jne	.L25
	leaq	-200(%rbp), %rdx
	leaq	-204(%rbp), %rsi
	andl	$4294967040, %edi
	call	getIntelRegisterCacheLineSize
	movq	-184(%rbp), %rdi
	testl	$2147483648, %edi 
	jne	.L26
.L40:
	leaq	-200(%rbp), %rdx
	leaq	-204(%rbp), %rsi
	call	getIntelRegisterCacheLineSize
	movq	-176(%rbp), %rdi
	testl	$2147483648, %edi 
	jne	.L27
.L41:
	leaq	-200(%rbp), %rdx
	leaq	-204(%rbp), %rsi
	call	getIntelRegisterCacheLineSize
	movq	-168(%rbp), %rdi
	testl	$2147483648, %edi 
	jne	.L28
.L42:
	leaq	-200(%rbp), %rdx
	leaq	-204(%rbp), %rsi
	call	getIntelRegisterCacheLineSize
	jmp	.L28
.L38:
	movq	-200(%rbp), %rax
.L21:
	movq	%rax, %rdx
	movl	$32, %eax
	testq	%rdx, %rdx
	cmoveq	%rax, %rdx
	addq	$200, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	leave
	movq	%rdx, %rax
	ret
.L19:
	leaq	-216(%rbp), %r8
	leaq	-224(%rbp), %rcx
	leaq	-232(%rbp), %rdx
	leaq	-240(%rbp), %rsi
	movl	$2147483648, %edi
	xorl	%ebx, %ebx
	call	freebl_cpuid
	movl	$2147483652, %eax
	cmpq	%rax, -240(%rbp)
	ja	.L43
.L32:
	movq	%rbx, %rdx
	movl	$32, %eax
	testq	%rdx, %rdx
	cmoveq	%rax, %rdx
	addq	$200, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	leave
	movq	%rdx, %rax
	ret
.L43:
	leaq	-216(%rbp), %r8
	leaq	-224(%rbp), %rcx
	leaq	-232(%rbp), %rdx
	leaq	-240(%rbp), %rsi
	movl	$2147483653, %edi
	call	freebl_cpuid
	movzbq	-224(%rbp), %rbx
	jmp	.L32
.LFE7:
	.size	s_mpi_getProcessorLineSize, .-s_mpi_getProcessorLineSize
