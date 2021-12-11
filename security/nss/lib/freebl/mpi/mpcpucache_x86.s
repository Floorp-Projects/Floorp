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
	.size	manMap, 40
manMap:
	.long	.LC0
	.long	.LC1
	.long	.LC2
	.long	.LC3
	.long	.LC4
	.long	.LC5
	.long	.LC6
	.long	.LC7
	.long	.LC8
	.long	.LC9
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
	.align	4
.globl freebl_cpuid
	.type	freebl_cpuid, @function
freebl_cpuid:
	pushl	%ebp
	pushl	%edi
	pushl	%esi
	subl	$8, %esp
	movl	%edx, %ebp
/APP
	pushl %ebx
	xorl %ecx, %ecx
	cpuid
	mov %ebx,%esi
	popl %ebx
	
/NO_APP
	movl	%eax, (%ebp)
	movl	24(%esp), %eax
	movl	%esi, (%eax)
	movl	28(%esp), %eax
	movl	%ecx, (%eax)
	movl	32(%esp), %eax
	movl	%edx, (%eax)
	addl	$8, %esp
	popl	%esi
	popl	%edi
	popl	%ebp
	ret
	.size	freebl_cpuid, .-freebl_cpuid
	.align	4
	.type	changeFlag, @function
changeFlag:
/APP
	pushfl
	popl %edx
	movl %edx,%ecx
	xorl %eax,%edx
	pushl %edx
	popfl
	pushfl
	popl %edx
	pushl %ecx
	popfl
	
/NO_APP
	xorl	%ecx, %edx
	movl	%edx, %eax
	ret
	.size	changeFlag, .-changeFlag
	.align	4
	.type	getIntelCacheEntryLineSize, @function
getIntelCacheEntryLineSize:
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	call	.L17
.L17:
	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L17], %ebx
	movzbl	CacheMap@GOTOFF(%ebx,%eax,2), %ecx
	movb	1+CacheMap@GOTOFF(%ebx,%eax,2), %al
	testb	%al, %al
	movl	16(%esp), %edi
	je	.L3
	cmpl	$6, %ecx
	je	.L6
	cmpl	$8, %ecx
	je	.L6
	movl	(%edx), %esi
	cmpl	$1, %esi
	jg	.L15
.L8:
	cmpl	$2, %esi
	jle	.L3
	cmpl	$12, %ecx
	je	.L12
	cmpl	$14, %ecx
	je	.L12
	.align	4
.L3:
	popl	%ebx
	popl	%esi
	popl	%edi
	ret
	.align	4
.L6:
	movzbl	%al, %eax
	movl	$1, (%edx)
	movl	%eax, (%edi)
.L16:
	popl	%ebx
	popl	%esi
	popl	%edi
	ret
	.align	4
.L15:
	cmpl	$9, %ecx
	je	.L9
	cmpl	$11, %ecx
	jne	.L8
.L9:
	movzbl	%al, %eax
	movl	$2, (%edx)
	movl	%eax, (%edi)
	jmp	.L16
.L12:
	movzbl	%al, %eax
	movl	$3, (%edx)
	movl	%eax, (%edi)
	jmp	.L16
	.size	getIntelCacheEntryLineSize, .-getIntelCacheEntryLineSize
	.align	4
	.type	getIntelRegisterCacheLineSize, @function
getIntelRegisterCacheLineSize:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ecx
	movl	8(%ebp), %edi
	movl	%eax, %esi
	movl	%edx, -12(%ebp)
	shrl	$24, %eax
	pushl	%edi
	call	getIntelCacheEntryLineSize
	movl	%esi, %eax
	pushl	%edi
	shrl	$16, %eax
	movl	-12(%ebp), %edx
	andl	$255, %eax
	call	getIntelCacheEntryLineSize
	pushl	%edi
	movl	%esi, %edx
	movzbl	%dh, %eax
	movl	-12(%ebp), %edx
	call	getIntelCacheEntryLineSize
	andl	$255, %esi
	movl	%edi, 8(%ebp)
	movl	-12(%ebp), %edx
	addl	$12, %esp
	leal	-8(%ebp), %esp
	movl	%esi, %eax
	popl	%esi
	popl	%edi
	leave
	jmp	getIntelCacheEntryLineSize
	.size	getIntelRegisterCacheLineSize, .-getIntelRegisterCacheLineSize
	.align	4
.globl s_mpi_getProcessorLineSize
	.type	s_mpi_getProcessorLineSize, @function
s_mpi_getProcessorLineSize:
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$188, %esp
	call	.L52
.L52:
	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-.L52], %ebx
	movl	$9, -168(%ebp)
	movl	$262144, %eax
	call	changeFlag
	xorl	%edx, %edx
	testl	%eax, %eax
	jne	.L50
.L19:
	leal	-12(%ebp), %esp
	popl	%ebx
	popl	%esi
	movl	%edx, %eax
	popl	%edi
	leave
	ret
	.align	4
.L50:
	movl	$2097152, %eax
	call	changeFlag
	testl	%eax, %eax
	movl	$32, %edx
	je	.L19
	leal	-108(%ebp), %eax
	pushl	%eax
	leal	-112(%ebp), %eax
	pushl	%eax
	leal	-116(%ebp), %eax
	pushl	%eax
	leal	-120(%ebp), %edx
	xorl	%eax, %eax
	call	freebl_cpuid
	movl	-120(%ebp), %eax
	movl	%eax, -164(%ebp)
	movl	-116(%ebp), %eax
	movl	%eax, -104(%ebp)
	movl	-108(%ebp), %eax
	movl	%eax, -100(%ebp)
	movl	-112(%ebp), %eax
	movl	%eax, -96(%ebp)
	movb	$0, -92(%ebp)
	xorl	%esi, %esi
	addl	$12, %esp
	leal	-104(%ebp), %edi
	.align	4
.L28:
	subl	$8, %esp
	pushl	%edi
	pushl	manMap@GOTOFF(%ebx,%esi,4)
	call	strcmp@PLT
	addl	$16, %esp
	testl	%eax, %eax
	jne	.L26
	movl	%esi, -168(%ebp)
.L26:
	incl	%esi
	cmpl	$9, %esi
	jle	.L28
	movl	-168(%ebp), %eax
	testl	%eax, %eax
	jne	.L29
	xorl	%eax, %eax
	cmpl	$1, -164(%ebp)
	movl	$4, -144(%ebp)
	movl	$0, -140(%ebp)
	jle	.L41
	leal	-124(%ebp), %edx
	movl	%edx, -188(%ebp)
	leal	-128(%ebp), %eax
	pushl	%edx
	movl	%eax, -184(%ebp)
	leal	-132(%ebp), %edx
	pushl	%eax
	movl	%edx, -180(%ebp)
	movl	$2, %eax
	pushl	%edx
	leal	-136(%ebp), %edx
	call	freebl_cpuid
	movl	-136(%ebp), %eax
	movl	%eax, %edi
	andl	$15, %edi
	xorl	%esi, %esi
	addl	$12, %esp
	leal	-140(%ebp), %edx
	cmpl	%edi, %esi
	movl	%edx, -176(%ebp)
	jl	.L40
	jmp	.L48
	.align	4
.L49:
	movl	-136(%ebp), %eax
.L40:
	testl	%eax, %eax
	js	.L35
	xorb	%al, %al
	pushl	-176(%ebp)
	leal	-144(%ebp), %edx
	call	getIntelRegisterCacheLineSize
	popl	%eax
.L35:
	movl	-132(%ebp), %eax
	testl	%eax, %eax
	js	.L36
	pushl	-176(%ebp)
	leal	-144(%ebp), %edx
	call	getIntelRegisterCacheLineSize
	popl	%eax
.L36:
	movl	-128(%ebp), %eax
	testl	%eax, %eax
	js	.L37
	pushl	-176(%ebp)
	leal	-144(%ebp), %edx
	call	getIntelRegisterCacheLineSize
	popl	%eax
.L37:
	movl	-124(%ebp), %eax
	testl	%eax, %eax
	js	.L38
	pushl	-176(%ebp)
	leal	-144(%ebp), %edx
	call	getIntelRegisterCacheLineSize
	popl	%eax
.L38:
	incl	%esi
	cmpl	%edi, %esi
	je	.L34
	pushl	-188(%ebp)
	pushl	-184(%ebp)
	pushl	-180(%ebp)
	leal	-136(%ebp), %edx
	movl	$2, %eax
	call	freebl_cpuid
	addl	$12, %esp
.L34:
	cmpl	%edi, %esi
	jl	.L49
.L48:
	movl	-140(%ebp), %eax
.L41:
	testl	%eax, %eax
	jne	.L44
	movb	$32, %al
.L44:
	leal	-12(%ebp), %esp
	popl	%ebx
	popl	%esi
	movl	%eax, %edx
	movl	%edx, %eax
	popl	%edi
	leave
	ret
.L29:
	leal	-148(%ebp), %eax
	movl	%eax, -192(%ebp)
	movl	$0, -172(%ebp)
	leal	-152(%ebp), %edi
	pushl	%eax
	pushl	%edi
	leal	-156(%ebp), %esi
	pushl	%esi
	leal	-160(%ebp), %edx
	movl	$-2147483648, %eax
	call	freebl_cpuid
	addl	$12, %esp
	cmpl	$-2147483644, -160(%ebp)
	ja	.L51
.L42:
	movl	-172(%ebp), %eax
	jmp	.L41
.L51:
	pushl	-192(%ebp)
	pushl	%edi
	pushl	%esi
	leal	-160(%ebp), %edx
	movl	$-2147483643, %eax
	call	freebl_cpuid
	movzbl	-152(%ebp), %edx
	addl	$12, %esp
	movl	%edx, -172(%ebp)
	jmp	.L42
	.size	s_mpi_getProcessorLineSize, .-s_mpi_getProcessorLineSize
