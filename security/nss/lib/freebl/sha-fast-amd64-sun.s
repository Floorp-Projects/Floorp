/ This Source Code Form is subject to the terms of the Mozilla Public
/ License, v. 2.0. If a copy of the MPL was not distributed with this
/ file, You can obtain one at http://mozilla.org/MPL/2.0/.

	.file	"sha_fast.c"
	.text
	.align 16
.globl SHA1_Begin
	.type	SHA1_Begin, @function
SHA1_Begin:
.LFB4:
	movl	$4023233417, %ecx
	movl	$2562383102, %edx
	movl	$3285377520, %eax
	movq	$0, 64(%rdi)
	movq	$1732584193, 72(%rdi)
	movq	%rcx, 80(%rdi)
	movq	%rdx, 88(%rdi)
	movq	$271733878, 96(%rdi)
	movq	%rax, 104(%rdi)
	ret
.LFE4:
	.size	SHA1_Begin, .-SHA1_Begin
	.align 16
	.type	shaCompress, @function
shaCompress:
.LFB7:
	pushq	%r15
.LCFI0:
	pushq	%r14
.LCFI1:
	pushq	%r13
.LCFI2:
	pushq	%r12
.LCFI3:
	movq	-88(%rdi), %r12
	movq	-80(%rdi), %r10
	movq	-72(%rdi), %r13
	movq	-64(%rdi), %r8
	pushq	%rbx
.LCFI4:
	movq	-56(%rdi), %rcx
	movl	(%rsi), %eax
	movl	%r12d, %edx
	movq	%r13, %r9
	roll	$5, %edx
	movl	4(%rsi), %ebx
	xorq	%r8, %r9 
/APP
	bswap %eax
/NO_APP
	andq	%r10, %r9
	mov	%eax, %r15d
	roll	$30, %r10d
	movq	%r15, -48(%rdi)
	xorq	%r8, %r9 
	movq	-48(%rdi), %r14
	addq	%r9, %rdx
	movq	%r10, %rax
	movl	%r12d, %r15d
	addq	%rcx, %rdx
	xorq	%r13, %rax 
	roll	$30, %r15d
	leaq	1518500249(%rdx,%r14), %rdx
	andq	%r12, %rax
	movq	%r15, %r12
/APP
	bswap %ebx
/NO_APP
	movl	%edx, %ecx
	mov	%ebx, %r11d
	xorq	%r13, %rax 
	movq	%r11, -40(%rdi)
	roll	$5, %ecx
	movq	-40(%rdi), %r9
	addq	%rax, %rcx
	xorq	%r10, %r12 
	movl	8(%rsi), %r14d
	addq	%r8, %rcx
	andq	%rdx, %r12
	movl	%edx, %r11d
	leaq	1518500249(%rcx,%r9), %rcx
	xorq	%r10, %r12 
	roll	$30, %r11d
/APP
	bswap %r14d
/NO_APP
	movl	%ecx, %r8d
	mov	%r14d, %ebx
	movl	12(%rsi), %r9d
	movq	%rbx, -32(%rdi)
	roll	$5, %r8d
	movq	-32(%rdi), %rax
	addq	%r12, %r8
	movq	%r11, %r12
	movl	%ecx, %ebx
	addq	%r13, %r8
	xorq	%r15, %r12 
	roll	$30, %ebx
	leaq	1518500249(%r8,%rax), %r8
	andq	%rcx, %r12
	movl	16(%rsi), %eax
/APP
	bswap %r9d
/NO_APP
	movl	%r8d, %edx
	mov	%r9d, %r14d
	xorq	%r15, %r12 
	movq	%r14, -24(%rdi)
	roll	$5, %edx
	movq	-24(%rdi), %r13
	addq	%r12, %rdx
	movq	%rbx, %r12
	movl	%r8d, %r14d
	addq	%r10, %rdx
	leaq	1518500249(%rdx,%r13), %rdx
	movl	20(%rsi), %r13d
/APP
	bswap %eax
/NO_APP
	movl	%edx, %ecx
	mov	%eax, %r9d
	roll	$5, %ecx
	xorq	%r11, %r12 
	movq	%r9, -16(%rdi)
	andq	%r8, %r12
	movq	-16(%rdi), %r10
	roll	$30, %r14d
	xorq	%r11, %r12 
	movq	%r14, %rax
	movl	%edx, %r9d
	addq	%r12, %rcx
	xorq	%rbx, %rax 
	roll	$30, %r9d
	addq	%r15, %rcx
	andq	%rdx, %rax
	leaq	1518500249(%rcx,%r10), %rcx
	xorq	%rbx, %rax 
	movl	24(%rsi), %r10d
/APP
	bswap %r13d
/NO_APP
	movl	%ecx, %r8d
	mov	%r13d, %r15d
	movq	%r15, -8(%rdi)
	roll	$5, %r8d
	movq	-8(%rdi), %r12
	addq	%rax, %r8
	movl	%ecx, %r15d
	addq	%r11, %r8
	movq	%r9, %r11
	roll	$30, %r15d
	leaq	1518500249(%r8,%r12), %r8
	xorq	%r14, %r11 
	movl	28(%rsi), %r12d
/APP
	bswap %r10d
/NO_APP
	andq	%rcx, %r11
	mov	%r10d, %r13d
	movl	%r8d, %edx
	movq	%r13, (%rdi)
	xorq	%r14, %r11 
	movq	(%rdi), %rax
	roll	$5, %edx
	movq	%r15, %r10
	movl	%r8d, %r13d
	addq	%r11, %rdx
	xorq	%r9, %r10 
	roll	$30, %r13d
	addq	%rbx, %rdx
	andq	%r8, %r10
	leaq	1518500249(%rdx,%rax), %rdx
	xorq	%r9, %r10 
	movl	32(%rsi), %eax
/APP
	bswap %r12d
/NO_APP
	movl	%edx, %ecx
	mov	%r12d, %ebx
	movq	%rbx, 8(%rdi)
	roll	$5, %ecx
	movq	8(%rdi), %r11
	addq	%r10, %rcx
	movq	%r13, %r10
	movl	%edx, %ebx
	addq	%r14, %rcx
	leaq	1518500249(%rcx,%r11), %rcx
/APP
	bswap %eax
/NO_APP
	movl	%ecx, %r8d
	mov	%eax, %r12d
	roll	$5, %r8d
	xorq	%r15, %r10 
	movq	%r12, 16(%rdi)
	andq	%rdx, %r10
	movq	16(%rdi), %r14
	roll	$30, %ebx
	xorq	%r15, %r10 
	movq	%rbx, %rax
	movl	36(%rsi), %r11d
	addq	%r10, %r8
	xorq	%r13, %rax 
	movl	%ecx, %r12d
	addq	%r9, %r8
	andq	%rcx, %rax
	roll	$30, %r12d
	leaq	1518500249(%r8,%r14), %r8
	xorq	%r13, %rax 
	movl	40(%rsi), %r14d
/APP
	bswap %r11d
/NO_APP
	movl	%r8d, %edx
	mov	%r11d, %r9d
	movq	%r12, %r11
	movq	%r9, 24(%rdi)
	roll	$5, %edx
	movq	24(%rdi), %r10
	addq	%rax, %rdx
	xorq	%rbx, %r11 
	movl	%r8d, %r9d
	addq	%r15, %rdx
	andq	%r8, %r11
	roll	$30, %r9d
	leaq	1518500249(%rdx,%r10), %rdx
	xorq	%rbx, %r11 
	movl	44(%rsi), %r10d
/APP
	bswap %r14d
/NO_APP
	movl	%edx, %ecx
	mov	%r14d, %r15d
	movq	%r15, 32(%rdi)
	roll	$5, %ecx
	movq	32(%rdi), %rax
	addq	%r11, %rcx
	movq	%r9, %r11
	movl	%edx, %r15d
	addq	%r13, %rcx
	xorq	%r12, %r11 
	roll	$30, %r15d
	leaq	1518500249(%rcx,%rax), %rcx
	andq	%rdx, %r11
	movl	48(%rsi), %eax
/APP
	bswap %r10d
/NO_APP
	movl	%ecx, %r8d
	mov	%r10d, %r14d
	xorq	%r12, %r11 
	movq	%r14, 40(%rdi)
	roll	$5, %r8d
	movq	40(%rdi), %r13
	addq	%r11, %r8
	movq	%r15, %r10
	movl	%ecx, %r14d
	addq	%rbx, %r8
	xorq	%r9, %r10 
	leaq	1518500249(%r8,%r13), %r8
	movl	52(%rsi), %r13d
/APP
	bswap %eax
/NO_APP
	movl	%r8d, %edx
	mov	%eax, %ebx
	roll	$5, %edx
	andq	%rcx, %r10
	movq	%rbx, 48(%rdi)
	xorq	%r9, %r10 
	movq	48(%rdi), %r11
	roll	$30, %r14d
	addq	%r10, %rdx
	movq	%r14, %rax
	movl	%r8d, %ebx
	addq	%r12, %rdx
	xorq	%r15, %rax 
	roll	$30, %ebx
	leaq	1518500249(%rdx,%r11), %rdx
	andq	%r8, %rax
	movl	56(%rsi), %r11d
/APP
	bswap %r13d
/NO_APP
	movl	%edx, %ecx
	mov	%r13d, %r12d
	xorq	%r15, %rax 
	movq	%r12, 56(%rdi)
	roll	$5, %ecx
	movq	56(%rdi), %r10
	addq	%rax, %rcx
	movl	%edx, %r12d
	addq	%r9, %rcx
	movq	%rbx, %r9
	roll	$30, %r12d
	leaq	1518500249(%rcx,%r10), %rcx
	xorq	%r14, %r9 
	movl	60(%rsi), %r10d
/APP
	bswap %r11d
/NO_APP
	andq	%rdx, %r9
	mov	%r11d, %r13d
	movl	%ecx, %r8d
	movq	%r13, 64(%rdi)
	xorq	%r14, %r9 
	movq	64(%rdi), %rax
	roll	$5, %r8d
	movq	%r12, %r11
	movl	%ecx, %r13d
	addq	%r9, %r8
	xorq	%rbx, %r11 
	roll	$30, %r13d
	addq	%r15, %r8
	andq	%rcx, %r11
	leaq	1518500249(%r8,%rax), %r8
	xorq	%rbx, %r11 
/APP
	bswap %r10d
/NO_APP
	movl	%r8d, %esi
	mov	%r10d, %r15d
	movq	%r15, 72(%rdi)
	roll	$5, %esi
	movq	72(%rdi), %r9
	movq	56(%rdi), %r10
	movq	16(%rdi), %rcx
	addq	%r11, %rsi
	movq	-32(%rdi), %rdx
	addq	%r14, %rsi
	movq	-48(%rdi), %rax
	leaq	1518500249(%rsi,%r9), %r14
	movq	%r13, %r11
	movl	%r8d, %r15d
	xorq	%rcx, %r10 
	xorq	%rdx, %r10 
	movl	%r14d, %ecx
	xorl	%eax, %r10d
	roll	%r10d
	roll	$5, %ecx
	xorq	%r12, %r11 
	andq	%r8, %r11
	movq	%r10, -48(%rdi)
	movq	-48(%rdi), %r9
	xorq	%r12, %r11 
	roll	$30, %r15d
	movl	%r14d, %r10d
	addq	%r11, %rcx
	movq	64(%rdi), %r11
	movq	24(%rdi), %rdx
	addq	%rbx, %rcx
	movq	-24(%rdi), %rbx
	movq	-40(%rdi), %rax
	leaq	1518500249(%rcx,%r9), %rcx
	movq	%r15, %r8
	roll	$30, %r10d
	xorq	%rdx, %r11 
	xorq	%r13, %r8 
	xorq	%rbx, %r11 
	andq	%r14, %r8
	movl	%ecx, %r9d
	xorl	%eax, %r11d
	xorq	%r13, %r8 
	roll	$5, %r9d
	roll	%r11d
	addq	%r8, %r9
	movq	%r10, %rax
	movq	%r11, -40(%rdi)
	movq	-40(%rdi), %rsi
	addq	%r12, %r9
	movq	72(%rdi), %rbx
	movq	32(%rdi), %rdx
	xorq	%r15, %rax 
	movq	-16(%rdi), %r14
	movq	-32(%rdi), %r12
	andq	%rcx, %rax
	leaq	1518500249(%r9,%rsi), %r9
	xorq	%r15, %rax 
	movl	%ecx, %r11d
	xorq	%rdx, %rbx 
	roll	$30, %r11d
	xorq	%r14, %rbx 
	movl	%r9d, %esi
	xorl	%r12d, %ebx
	roll	$5, %esi
	roll	%ebx
	addq	%rax, %rsi
	movq	%rbx, -32(%rdi)
	movq	-32(%rdi), %r8
	addq	%r13, %rsi
	movq	-48(%rdi), %r12
	movq	40(%rdi), %rdx
	movq	%r11, %r13
	movq	-8(%rdi), %r14
	movq	-24(%rdi), %rcx
	movl	%r9d, %ebx
	leaq	1518500249(%rsi,%r8), %rsi
	xorq	%rdx, %r12 
	xorq	%r14, %r12 
	movl	%esi, %r8d
	xorl	%ecx, %r12d
	roll	%r12d
	roll	$5, %r8d
	xorq	%r10, %r13 
	andq	%r9, %r13
	movq	%r12, -24(%rdi)
	movq	-24(%rdi), %rax
	xorq	%r10, %r13 
	roll	$30, %ebx
	movl	%esi, %r12d
	addq	%r13, %r8
	xorq	%rbx, %rsi 
	roll	$30, %r12d
	addq	%r15, %r8
	movq	-40(%rdi), %r15
	movq	48(%rdi), %rdx
	movq	(%rdi), %r14
	movq	-16(%rdi), %r9
	leaq	1518500249(%r8,%rax), %r13
	xorq	%r11, %rsi 
	xorq	%rdx, %r15 
	movl	%r13d, %ecx
	xorq	%r14, %r15 
	roll	$5, %ecx
	xorl	%r9d, %r15d
	addq	%rsi, %rcx
	roll	%r15d
	addq	%r10, %rcx
	movq	%r15, -16(%rdi)
	movq	-16(%rdi), %rsi
	movl	%r13d, %r15d
	movq	-32(%rdi), %r14
	movq	56(%rdi), %rax
	xorq	%r12, %r13 
	movq	8(%rdi), %rdx
	movq	-8(%rdi), %r10
	xorq	%rbx, %r13 
	leaq	1859775393(%rcx,%rsi), %r9
	roll	$30, %r15d
	xorq	%rax, %r14 
	xorq	%rdx, %r14 
	movl	%r9d, %esi
	xorl	%r10d, %r14d
	roll	$5, %esi
	roll	%r14d
	addq	%r13, %rsi
	movq	%r14, -8(%rdi)
	movq	-8(%rdi), %r8
	addq	%r11, %rsi
	movq	-24(%rdi), %r13
	movq	64(%rdi), %rax
	movl	%r9d, %r14d
	movq	16(%rdi), %rdx
	movq	(%rdi), %r11
	xorq	%r15, %r9 
	leaq	1859775393(%rsi,%r8), %r10
	xorq	%rax, %r13 
	xorq	%rdx, %r13 
	movl	%r10d, %r8d
	xorl	%r11d, %r13d
	roll	$5, %r8d
	roll	%r13d
	xorq	%r12, %r9 
	roll	$30, %r14d
	addq	%r9, %r8
	movq	%r13, (%rdi)
	movq	(%rdi), %rcx
	addq	%rbx, %r8
	movq	-16(%rdi), %rbx
	movq	72(%rdi), %rax
	movq	24(%rdi), %rdx
	movq	8(%rdi), %r9
	movl	%r10d, %r13d
	leaq	1859775393(%r8,%rcx), %r11
	xorq	%r14, %r10 
	roll	$30, %r13d
	xorq	%rax, %rbx 
	xorq	%r15, %r10 
	xorq	%rdx, %rbx 
	movl	%r11d, %ecx
	xorl	%r9d, %ebx
	roll	$5, %ecx
	roll	%ebx
	addq	%r10, %rcx
	movq	%rbx, 8(%rdi)
	movq	8(%rdi), %rsi
	addq	%r12, %rcx
	movq	-8(%rdi), %r12
	movq	-48(%rdi), %rax
	movl	%r11d, %ebx
	movq	32(%rdi), %rdx
	movq	16(%rdi), %r9
	xorq	%r13, %r11 
	leaq	1859775393(%rcx,%rsi), %r10
	xorq	%r14, %r11 
	roll	$30, %ebx
	xorq	%rax, %r12 
	xorq	%rdx, %r12 
	movl	%r10d, %esi
	xorl	%r9d, %r12d
	roll	$5, %esi
	roll	%r12d
	addq	%r11, %rsi
	movq	%r12, 16(%rdi)
	addq	%r15, %rsi
	movq	16(%rdi), %r8
	movq	(%rdi), %r15
	movq	-40(%rdi), %rax
	movl	%r10d, %r12d
	movq	40(%rdi), %rdx
	movq	24(%rdi), %r9
	xorq	%rbx, %r10 
	leaq	1859775393(%rsi,%r8), %r11
	xorq	%r13, %r10 
	xorq	%rax, %r15 
	xorq	%rdx, %r15 
	movl	%r11d, %r8d
	xorl	%r9d, %r15d
	roll	$5, %r8d
	roll	%r15d
	addq	%r10, %r8
	movq	%r15, 24(%rdi)
	movq	24(%rdi), %rcx
	addq	%r14, %r8
	movq	8(%rdi), %r14
	movq	-32(%rdi), %rax
	roll	$30, %r12d
	movq	48(%rdi), %rdx
	movq	32(%rdi), %r10
	movl	%r11d, %r15d
	leaq	1859775393(%r8,%rcx), %r9
	xorq	%r12, %r11 
	roll	$30, %r15d
	xorq	%rax, %r14 
	xorq	%rbx, %r11 
	xorq	%rdx, %r14 
	movl	%r9d, %ecx
	xorl	%r10d, %r14d
	roll	$5, %ecx
	roll	%r14d
	addq	%r11, %rcx
	movq	%r14, 32(%rdi)
	addq	%r13, %rcx
	movq	32(%rdi), %rsi
	movq	16(%rdi), %r13
	movq	-24(%rdi), %rax
	movl	%r9d, %r14d
	movq	56(%rdi), %rdx
	movq	40(%rdi), %r11
	xorq	%r15, %r9 
	leaq	1859775393(%rcx,%rsi), %r10
	xorq	%r12, %r9 
	roll	$30, %r14d
	xorq	%rax, %r13 
	xorq	%rdx, %r13 
	movl	%r10d, %esi
	xorl	%r11d, %r13d
	roll	$5, %esi
	roll	%r13d
	addq	%r9, %rsi
	movq	%r13, 40(%rdi)
	movq	40(%rdi), %r8
	addq	%rbx, %rsi
	movq	24(%rdi), %rbx
	movq	-16(%rdi), %rax
	movl	%r10d, %r13d
	movq	64(%rdi), %rdx
	movq	48(%rdi), %r9
	xorq	%r14, %r10 
	leaq	1859775393(%rsi,%r8), %r11
	xorq	%r15, %r10 
	roll	$30, %r13d
	xorq	%rax, %rbx 
	xorq	%rdx, %rbx 
	movl	%r11d, %r8d
	xorl	%r9d, %ebx
	roll	$5, %r8d
	roll	%ebx
	addq	%r10, %r8
	movq	%rbx, 48(%rdi)
	addq	%r12, %r8
	movq	48(%rdi), %rcx
	movq	32(%rdi), %r12
	movq	-8(%rdi), %rax
	movl	%r11d, %ebx
	movq	72(%rdi), %rdx
	movq	56(%rdi), %r9
	leaq	1859775393(%r8,%rcx), %r10
	xorq	%rax, %r12 
	xorq	%rdx, %r12 
	movl	%r10d, %ecx
	xorl	%r9d, %r12d
	xorq	%r13, %r11 
	roll	$5, %ecx
	xorq	%r14, %r11 
	roll	%r12d
	roll	$30, %ebx
	addq	%r11, %rcx
	movq	%r12, 56(%rdi)
	movq	56(%rdi), %rsi
	addq	%r15, %rcx
	movq	40(%rdi), %r15
	movq	(%rdi), %rax
	movq	-48(%rdi), %rdx
	movq	64(%rdi), %r9
	movl	%r10d, %r12d
	leaq	1859775393(%rcx,%rsi), %r11
	xorq	%rbx, %r10 
	roll	$30, %r12d
	xorq	%rax, %r15 
	xorq	%r13, %r10 
	xorq	%rdx, %r15 
	movl	%r11d, %esi
	xorl	%r9d, %r15d
	roll	$5, %esi
	roll	%r15d
	addq	%r10, %rsi
	movq	%r15, 64(%rdi)
	movq	64(%rdi), %r8
	addq	%r14, %rsi
	movq	48(%rdi), %r14
	movq	8(%rdi), %rax
	movl	%r11d, %r15d
	movq	-40(%rdi), %rdx
	movq	72(%rdi), %r10
	xorq	%r12, %r11 
	leaq	1859775393(%rsi,%r8), %r9
	xorq	%rbx, %r11 
	roll	$30, %r15d
	xorq	%rax, %r14 
	xorq	%rdx, %r14 
	movl	%r9d, %r8d
	xorl	%r10d, %r14d
	roll	$5, %r8d
	roll	%r14d
	addq	%r11, %r8
	movq	%r14, 72(%rdi)
	addq	%r13, %r8
	movq	72(%rdi), %rcx
	movq	56(%rdi), %r13
	movq	16(%rdi), %rax
	movl	%r9d, %r14d
	movq	-32(%rdi), %rdx
	movq	-48(%rdi), %r11
	leaq	1859775393(%r8,%rcx), %r10
	xorq	%rax, %r13 
	xorq	%rdx, %r13 
	movl	%r10d, %ecx
	xorl	%r11d, %r13d
	roll	$5, %ecx
	roll	%r13d
	xorq	%r15, %r9 
	roll	$30, %r14d
	xorq	%r12, %r9 
	movq	%r13, -48(%rdi)
	movq	-48(%rdi), %rsi
	addq	%r9, %rcx
	movl	%r10d, %r13d
	xorq	%r14, %r10 
	addq	%rbx, %rcx
	movq	64(%rdi), %rbx
	movq	24(%rdi), %rax
	movq	-24(%rdi), %rdx
	leaq	1859775393(%rcx,%rsi), %r11
	movq	-40(%rdi), %r9
	xorq	%r15, %r10 
	roll	$30, %r13d
	xorq	%rax, %rbx 
	movl	%r11d, %esi
	xorq	%rdx, %rbx 
	roll	$5, %esi
	xorl	%r9d, %ebx
	addq	%r10, %rsi
	roll	%ebx
	addq	%r12, %rsi
	movq	%rbx, -40(%rdi)
	movq	-40(%rdi), %r8
	movl	%r11d, %ebx
	movq	72(%rdi), %r12
	movq	32(%rdi), %rax
	xorq	%r13, %r11 
	movq	-16(%rdi), %rdx
	movq	-32(%rdi), %r9
	xorq	%r14, %r11 
	leaq	1859775393(%rsi,%r8), %r10
	roll	$30, %ebx
	xorq	%rax, %r12 
	xorq	%rdx, %r12 
	movl	%r10d, %r8d
	xorl	%r9d, %r12d
	roll	$5, %r8d
	roll	%r12d
	addq	%r11, %r8
	movq	%r12, -32(%rdi)
	movq	-32(%rdi), %rcx
	addq	%r15, %r8
	movq	-48(%rdi), %r15
	movq	40(%rdi), %rax
	movl	%r10d, %r12d
	movq	-8(%rdi), %rdx
	movq	-24(%rdi), %r9
	xorq	%rbx, %r10 
	leaq	1859775393(%r8,%rcx), %r11
	xorq	%r13, %r10 
	xorq	%rax, %r15 
	xorq	%rdx, %r15 
	movl	%r11d, %ecx
	xorl	%r9d, %r15d
	roll	$5, %ecx
	roll	%r15d
	addq	%r10, %rcx
	addq	%r14, %rcx
	movq	%r15, -24(%rdi)
	movq	-24(%rdi), %rsi
	movq	-40(%rdi), %r14
	movq	48(%rdi), %rax
	roll	$30, %r12d
	movq	(%rdi), %rdx
	movq	-16(%rdi), %r10
	movl	%r11d, %r15d
	leaq	1859775393(%rcx,%rsi), %r9
	xorq	%r12, %r11 
	roll	$30, %r15d
	xorq	%rax, %r14 
	xorq	%rbx, %r11 
	xorq	%rdx, %r14 
	movl	%r9d, %esi
	xorl	%r10d, %r14d
	roll	$5, %esi
	roll	%r14d
	addq	%r11, %rsi
	movq	%r14, -16(%rdi)
	movq	-16(%rdi), %r8
	addq	%r13, %rsi
	movq	-32(%rdi), %r11
	movq	56(%rdi), %rax
	movl	%r9d, %r14d
	movq	8(%rdi), %rdx
	movq	-8(%rdi), %r10
	xorq	%r15, %r9 
	leaq	1859775393(%rsi,%r8), %r13
	xorq	%r12, %r9 
	roll	$30, %r14d
	xorq	%rax, %r11 
	xorq	%rdx, %r11 
	movl	%r13d, %r8d
	xorl	%r10d, %r11d
	roll	$5, %r8d
	movl	%r13d, %r10d
	roll	%r11d
	addq	%r9, %r8
	xorq	%r14, %r13 
	movq	%r11, -8(%rdi)
	addq	%rbx, %r8
	movq	-8(%rdi), %rbx
	movq	-24(%rdi), %r9
	movq	64(%rdi), %rax
	xorq	%r15, %r13 
	movq	16(%rdi), %rdx
	movq	(%rdi), %rcx
	leaq	1859775393(%r8,%rbx), %r11
	xorq	%rax, %r9 
	xorq	%rdx, %r9 
	movl	%r11d, %ebx
	xorl	%ecx, %r9d
	roll	$5, %ebx
	roll	%r9d
	addq	%r13, %rbx
	movq	%r9, (%rdi)
	movq	(%rdi), %rsi
	addq	%r12, %rbx
	movq	-16(%rdi), %r12
	movq	72(%rdi), %r13
	movl	%r11d, %r9d
	leaq	1859775393(%rbx,%rsi), %rcx
	movl	%r10d, %ebx
	movq	24(%rdi), %r10
	movq	8(%rdi), %rax
	xorq	%r13, %r12 
	roll	$30, %ebx
	movl	%ecx, %esi
	xorq	%r10, %r12 
	xorq	%rbx, %r11 
	roll	$5, %esi
	xorl	%eax, %r12d
	xorq	%r14, %r11 
	roll	$30, %r9d
	roll	%r12d
	addq	%r11, %rsi
	movq	%rcx, %rax
	movq	%r12, 8(%rdi)
	movq	8(%rdi), %rdx
	addq	%r15, %rsi
	movq	-8(%rdi), %r11
	movq	-48(%rdi), %r13
	movl	%ecx, %r12d
	movq	32(%rdi), %r10
	movq	16(%rdi), %r8
	orq	%r9, %rcx
	leaq	1859775393(%rsi,%rdx), %rsi
	andq	%rbx, %rcx
	andq	%r9, %rax
	xorq	%r13, %r11 
	orq	%rcx, %rax
	roll	$30, %r12d
	xorq	%r10, %r11 
	movq	%rsi, %r10
	xorl	%r8d, %r11d
	movl	%esi, %r8d
	andq	%r12, %r10
	roll	%r11d
	roll	$5, %r8d
	movq	%r11, 16(%rdi)
	addq	%rax, %r8
	movq	16(%rdi), %r15
	movq	(%rdi), %r13
	movq	-40(%rdi), %rdx
	addq	%r14, %r8
	movq	40(%rdi), %r14
	movq	24(%rdi), %rcx
	movl	%esi, %r11d
	addq	%r15, %r8
	movl	$2400959708, %r15d
	orq	%r12, %rsi
	xorq	%rdx, %r13 
	addq	%r15, %r8
	andq	%r9, %rsi
	xorq	%r14, %r13 
	orq	%rsi, %r10
	xorl	%ecx, %r13d
	movl	%r8d, %ecx
	roll	%r13d
	roll	$5, %ecx
	movq	%r13, 24(%rdi)
	addq	%r10, %rcx
	movq	24(%rdi), %rax
	movq	8(%rdi), %r14
	movq	-32(%rdi), %rdx
	addq	%rbx, %rcx
	movq	48(%rdi), %rbx
	movq	32(%rdi), %rsi
	roll	$30, %r11d
	addq	%rax, %rcx
	movl	%r8d, %r13d
	movq	%r8, %r10
	xorq	%rdx, %r14 
	addq	%r15, %rcx
	orq	%r11, %r8
	xorq	%rbx, %r14 
	andq	%r12, %r8
	andq	%r11, %r10
	xorl	%esi, %r14d
	movl	%ecx, %esi
	orq	%r8, %r10
	roll	$5, %esi
	roll	%r14d
	roll	$30, %r13d
	addq	%r10, %rsi
	movq	%r14, 32(%rdi)
	movq	32(%rdi), %rax
	addq	%r9, %rsi
	movq	16(%rdi), %r9
	movq	-24(%rdi), %rdx
	movq	56(%rdi), %rbx
	movq	40(%rdi), %r8
	movl	%ecx, %r14d
	addq	%rax, %rsi
	movq	%rcx, %r10
	orq	%r13, %rcx
	xorq	%rdx, %r9 
	addq	%r15, %rsi
	andq	%r11, %rcx
	xorq	%rbx, %r9 
	andq	%r13, %r10
	roll	$30, %r14d
	xorl	%r8d, %r9d
	movl	%esi, %r8d
	orq	%rcx, %r10
	roll	%r9d
	roll	$5, %r8d
	movq	%r9, 40(%rdi)
	addq	%r10, %r8
	movq	40(%rdi), %rax
	movq	24(%rdi), %r10
	movq	-16(%rdi), %rdx
	addq	%r12, %r8
	movq	64(%rdi), %rbx
	movq	48(%rdi), %rcx
	movl	%esi, %r9d
	addq	%rax, %r8
	movq	%rsi, %r12
	xorq	%rdx, %r10 
	addq	%r15, %r8
	xorq	%rbx, %r10 
	orq	%r14, %rsi
	andq	%r14, %r12
	andq	%r13, %rsi
	xorl	%ecx, %r10d
	movl	%r8d, %ecx
	orq	%rsi, %r12
	roll	%r10d
	roll	$5, %ecx
	movq	%r10, 48(%rdi)
	addq	%r12, %rcx
	movq	48(%rdi), %rax
	movq	32(%rdi), %r12
	movq	-8(%rdi), %rdx
	addq	%r11, %rcx
	movq	72(%rdi), %rbx
	movq	56(%rdi), %rsi
	roll	$30, %r9d
	addq	%rax, %rcx
	movl	%r8d, %r10d
	movq	%r8, %r11
	xorq	%rdx, %r12 
	addq	%r15, %rcx
	orq	%r9, %r8
	xorq	%rbx, %r12 
	andq	%r14, %r8
	andq	%r9, %r11
	xorl	%esi, %r12d
	movl	%ecx, %esi
	orq	%r8, %r11
	roll	%r12d
	roll	$5, %esi
	roll	$30, %r10d
	movq	%r12, 56(%rdi)
	addq	%r11, %rsi
	movq	56(%rdi), %rax
	movq	40(%rdi), %r11
	movq	(%rdi), %rdx
	addq	%r13, %rsi
	movq	-48(%rdi), %rbx
	movq	64(%rdi), %r8
	movq	%rcx, %r13
	addq	%rax, %rsi
	andq	%r10, %r13
	movl	%ecx, %r12d
	xorq	%rdx, %r11 
	addq	%r15, %rsi
	xorq	%rbx, %r11 
	xorl	%r8d, %r11d
	movl	%esi, %r8d
	roll	%r11d
	roll	$5, %r8d
	orq	%r10, %rcx
	andq	%r9, %rcx
	movq	%r11, 64(%rdi)
	movq	64(%rdi), %rax
	orq	%rcx, %r13
	roll	$30, %r12d
	movl	%esi, %r11d
	addq	%r13, %r8
	movq	48(%rdi), %r13
	movq	8(%rdi), %rdx
	movq	-40(%rdi), %rbx
	addq	%r14, %r8
	movq	72(%rdi), %rcx
	addq	%rax, %r8
	movq	%rsi, %r14
	orq	%r12, %rsi
	xorq	%rdx, %r13 
	addq	%r15, %r8
	andq	%r10, %rsi
	xorq	%rbx, %r13 
	andq	%r12, %r14
	roll	$30, %r11d
	xorl	%ecx, %r13d
	movl	%r8d, %ecx
	orq	%rsi, %r14
	roll	%r13d
	roll	$5, %ecx
	movq	%r13, 72(%rdi)
	addq	%r14, %rcx
	movq	72(%rdi), %rax
	movq	56(%rdi), %r14
	movq	16(%rdi), %rdx
	addq	%r9, %rcx
	movq	-32(%rdi), %rbx
	movq	-48(%rdi), %rsi
	movl	%r8d, %r13d
	addq	%rax, %rcx
	movq	%r8, %r9
	orq	%r11, %r8
	xorq	%rdx, %r14 
	addq	%r15, %rcx
	andq	%r12, %r8
	xorq	%rbx, %r14 
	andq	%r11, %r9
	xorl	%esi, %r14d
	movl	%ecx, %esi
	orq	%r8, %r9
	roll	$5, %esi
	roll	%r14d
	addq	%r9, %rsi
	movq	%r14, -48(%rdi)
	movq	-48(%rdi), %rax
	addq	%r10, %rsi
	movq	64(%rdi), %r10
	movq	24(%rdi), %rdx
	movq	-24(%rdi), %rbx
	movq	-40(%rdi), %r8
	movl	%ecx, %r14d
	addq	%rax, %rsi
	roll	$30, %r13d
	movq	%rcx, %r9
	xorq	%rdx, %r10 
	addq	%r15, %rsi
	orq	%r13, %rcx
	xorq	%rbx, %r10 
	andq	%r11, %rcx
	andq	%r13, %r9
	xorl	%r8d, %r10d
	movl	%esi, %r8d
	orq	%rcx, %r9
	roll	$5, %r8d
	roll	%r10d
	roll	$30, %r14d
	addq	%r9, %r8
	movq	%r10, -40(%rdi)
	movq	-40(%rdi), %rax
	addq	%r12, %r8
	movq	72(%rdi), %r12
	movq	32(%rdi), %rdx
	movq	-16(%rdi), %rbx
	movq	-32(%rdi), %rcx
	movl	%esi, %r10d
	addq	%rax, %r8
	movq	%rsi, %r9
	orq	%r14, %rsi
	xorq	%rdx, %r12 
	addq	%r15, %r8
	andq	%r13, %rsi
	xorq	%rbx, %r12 
	andq	%r14, %r9
	roll	$30, %r10d
	xorl	%ecx, %r12d
	movl	%r8d, %ecx
	orq	%rsi, %r9
	roll	$5, %ecx
	roll	%r12d
	addq	%r9, %rcx
	movq	%r12, -32(%rdi)
	movq	-32(%rdi), %rax
	addq	%r11, %rcx
	movq	-48(%rdi), %r11
	movq	40(%rdi), %rdx
	movq	-8(%rdi), %rbx
	movq	-24(%rdi), %rsi
	movl	%r8d, %r12d
	addq	%rax, %rcx
	movq	%r8, %r9
	xorq	%rdx, %r11 
	addq	%r15, %rcx
	xorq	%rbx, %r11 
	xorl	%esi, %r11d
	orq	%r10, %r8
	andq	%r10, %r9
	andq	%r14, %r8
	movl	%ecx, %esi
	roll	%r11d
	orq	%r8, %r9
	roll	$5, %esi
	movq	%r11, -24(%rdi)
	addq	%r9, %rsi
	movq	-24(%rdi), %rax
	roll	$30, %r12d
	addq	%r13, %rsi
	movq	-40(%rdi), %r13
	movq	48(%rdi), %rdx
	movq	(%rdi), %rbx
	movq	-16(%rdi), %r8
	movl	%ecx, %r11d
	addq	%rax, %rsi
	movq	%rcx, %r9
	orq	%r12, %rcx
	xorq	%rdx, %r13 
	addq	%r15, %rsi
	andq	%r10, %rcx
	xorq	%rbx, %r13 
	andq	%r12, %r9
	roll	$30, %r11d
	xorl	%r8d, %r13d
	movl	%esi, %r8d
	orq	%rcx, %r9
	roll	%r13d
	roll	$5, %r8d
	movq	%r13, -16(%rdi)
	addq	%r9, %r8
	movq	-16(%rdi), %rax
	movq	-32(%rdi), %r9
	movq	56(%rdi), %rdx
	addq	%r14, %r8
	movq	8(%rdi), %rcx
	movq	-8(%rdi), %rbx
	movl	%esi, %r13d
	addq	%rax, %r8
	movq	%rsi, %r14
	orq	%r11, %rsi
	xorq	%rdx, %r9 
	addq	%r15, %r8
	andq	%r11, %r14
	xorq	%rcx, %r9 
	xorl	%ebx, %r9d
	movl	%r8d, %ebx
	roll	%r9d
	roll	$5, %ebx
	andq	%r12, %rsi
	orq	%rsi, %r14
	movq	%r9, -8(%rdi)
	movq	-8(%rdi), %rax
	addq	%r14, %rbx
	movq	-24(%rdi), %r14
	movq	64(%rdi), %rdx
	movq	16(%rdi), %rcx
	addq	%r10, %rbx
	movq	(%rdi), %rsi
	roll	$30, %r13d
	addq	%rax, %rbx
	movl	%r8d, %r9d
	xorq	%rdx, %r14 
	addq	%r15, %rbx
	movq	%r8, %r10
	xorq	%rcx, %r14 
	orq	%r13, %r8
	andq	%r13, %r10
	andq	%r11, %r8
	xorl	%esi, %r14d
	movl	%ebx, %esi
	orq	%r8, %r10
	roll	$5, %esi
	roll	%r14d
	addq	%r10, %rsi
	movq	%r14, (%rdi)
	movq	(%rdi), %rax
	addq	%r12, %rsi
	movq	-16(%rdi), %r12
	movq	72(%rdi), %rdx
	movq	24(%rdi), %rcx
	movq	8(%rdi), %r8
	roll	$30, %r9d
	addq	%rax, %rsi
	movl	%ebx, %r14d
	movq	%rbx, %r10
	xorq	%rdx, %r12 
	addq	%r15, %rsi
	orq	%r9, %rbx
	xorq	%rcx, %r12 
	andq	%r13, %rbx
	andq	%r9, %r10
	xorl	%r8d, %r12d
	movl	%esi, %r8d
	orq	%rbx, %r10
	roll	%r12d
	roll	$5, %r8d
	movq	%r12, 8(%rdi)
	movq	8(%rdi), %rax
	addq	%r10, %r8
	movq	-8(%rdi), %rbx
	movq	-48(%rdi), %rdx
	addq	%r11, %r8
	movq	32(%rdi), %r11
	movq	16(%rdi), %rcx
	movl	%esi, %r12d
	addq	%rax, %r8
	movq	%rsi, %r10
	addq	%r15, %r8
	xorq	%rdx, %rbx 
	roll	$30, %r14d
	xorq	%r11, %rbx 
	orq	%r14, %rsi
	andq	%r14, %r10
	xorl	%ecx, %ebx
	andq	%r9, %rsi
	movl	%r8d, %ecx
	roll	%ebx
	orq	%rsi, %r10
	roll	$5, %ecx
	movq	%rbx, 16(%rdi)
	movq	16(%rdi), %rsi
	addq	%r10, %rcx
	movq	(%rdi), %r11
	movq	-40(%rdi), %rax
	addq	%r13, %rcx
	movq	40(%rdi), %rdx
	movq	24(%rdi), %r13
	roll	$30, %r12d
	addq	%rsi, %rcx
	movl	%r8d, %ebx
	movq	%r8, %r10
	xorq	%rax, %r11 
	addq	%r15, %rcx
	orq	%r12, %r8
	xorq	%rdx, %r11 
	andq	%r14, %r8
	andq	%r12, %r10
	xorl	%r13d, %r11d
	movl	%ecx, %r13d
	orq	%r8, %r10
	roll	%r11d
	roll	$5, %r13d
	roll	$30, %ebx
	movq	%r11, 24(%rdi)
	addq	%r10, %r13
	movq	24(%rdi), %rsi
	movq	8(%rdi), %r10
	movq	-32(%rdi), %rax
	addq	%r9, %r13
	movq	48(%rdi), %rdx
	movq	32(%rdi), %r8
	movl	%ecx, %r11d
	addq	%rsi, %r13
	movq	%rcx, %r9
	xorq	%rax, %r10 
	addq	%r15, %r13
	xorq	%rdx, %r10 
	xorl	%r8d, %r10d
	movl	%r13d, %r8d
	roll	%r10d
	orq	%rbx, %rcx
	andq	%rbx, %r9
	movq	%r10, 32(%rdi)
	andq	%r12, %rcx
	movl	%r13d, %r10d
	orq	%rcx, %r9
	roll	$5, %r10d
	movq	32(%rdi), %rsi
	addq	%r9, %r10
	roll	$30, %r11d
	movq	%r13, %rcx
	addq	%r14, %r10
	movq	16(%rdi), %r14
	movq	-24(%rdi), %rax
	movq	56(%rdi), %rdx
	movq	40(%rdi), %r9
	addq	%rsi, %r10
	addq	%r15, %r10
	orq	%r11, %r13
	andq	%r11, %rcx
	xorq	%rax, %r14 
	andq	%rbx, %r13
	xorq	%rdx, %r14 
	orq	%r13, %rcx
	xorl	%r9d, %r14d
	movl	%r10d, %r9d
	roll	%r14d
	roll	$5, %r9d
	movq	%r14, 40(%rdi)
	movq	40(%rdi), %rsi
	addq	%rcx, %r9
	movq	24(%rdi), %r13
	addq	%r12, %r9
	movq	-16(%rdi), %r12
	movq	64(%rdi), %rax
	movl	%r10d, %r14d
	addq	%rsi, %r9
	movl	%r8d, %esi
	addq	%r15, %r9
	movq	48(%rdi), %r15
	xorq	%r12, %r13 
	roll	$30, %esi
	xorq	%rax, %r13 
	xorq	%rsi, %r10 
	xorl	%r15d, %r13d
	movl	%r9d, %r15d
	xorq	%r11, %r10 
	roll	$5, %r15d
	roll	%r13d
	addq	%r10, %r15
	movq	%r13, 48(%rdi)
	movq	48(%rdi), %r10
	addq	%rbx, %r15
	movq	32(%rdi), %rbx
	movq	-8(%rdi), %r8
	movq	72(%rdi), %rdx
	movq	56(%rdi), %rcx
	roll	$30, %r14d
	addq	%r10, %r15
	movl	$3395469782, %r10d
	movl	%r9d, %r13d
	xorq	%r8, %rbx 
	addq	%r10, %r15
	xorq	%r14, %r9 
	xorq	%rdx, %rbx 
	xorq	%rsi, %r9 
	roll	$30, %r13d
	xorl	%ecx, %ebx
	movl	%r15d, %ecx
	roll	%ebx
	roll	$5, %ecx
	movq	%rbx, 56(%rdi)
	addq	%r9, %rcx
	movq	56(%rdi), %r12
	movq	40(%rdi), %r9
	movq	(%rdi), %rax
	addq	%r11, %rcx
	movq	-48(%rdi), %r8
	movq	64(%rdi), %r11
	movl	%r15d, %ebx
	addq	%r12, %rcx
	xorq	%r13, %r15 
	roll	$30, %ebx
	xorq	%rax, %r9 
	addq	%r10, %rcx
	xorq	%r14, %r15 
	xorq	%r8, %r9 
	xorl	%r11d, %r9d
	movl	%ecx, %r11d
	roll	%r9d
	roll	$5, %r11d
	movq	%r9, 64(%rdi)
	addq	%r15, %r11
	movq	64(%rdi), %rdx
	movq	48(%rdi), %r15
	movq	8(%rdi), %r12
	addq	%rsi, %r11
	movq	-40(%rdi), %rax
	movq	72(%rdi), %r8
	movl	%ecx, %r9d
	addq	%rdx, %r11
	xorq	%r12, %r15 
	addq	%r10, %r11
	xorq	%rax, %r15 
	xorl	%r8d, %r15d
	movl	%r11d, %r8d
	roll	%r15d
	roll	$5, %r8d
	xorq	%rbx, %rcx 
	xorq	%r13, %rcx 
	movq	%r15, 72(%rdi)
	movq	72(%rdi), %rsi
	addq	%rcx, %r8
	movq	56(%rdi), %r12
	movq	16(%rdi), %rcx
	movq	-32(%rdi), %rdx
	addq	%r14, %r8
	movq	-48(%rdi), %r14
	addq	%rsi, %r8
	roll	$30, %r9d
	movl	%r11d, %r15d
	xorq	%rcx, %r12 
	addq	%r10, %r8
	xorq	%r9, %r11 
	xorq	%rdx, %r12 
	xorq	%rbx, %r11 
	roll	$30, %r15d
	xorl	%r14d, %r12d
	movl	%r8d, %r14d
	roll	$5, %r14d
	roll	%r12d
	addq	%r11, %r14
	movq	%r12, -48(%rdi)
	movq	-48(%rdi), %rax
	addq	%r13, %r14
	movq	64(%rdi), %r13
	movq	24(%rdi), %rsi
	movq	-24(%rdi), %rcx
	movq	-40(%rdi), %r11
	movl	%r8d, %r12d
	addq	%rax, %r14
	xorq	%r15, %r8 
	roll	$30, %r12d
	xorq	%rsi, %r13 
	addq	%r10, %r14
	xorq	%r9, %r8 
	xorq	%rcx, %r13 
	xorl	%r11d, %r13d
	movl	%r14d, %r11d
	roll	$5, %r11d
	roll	%r13d
	addq	%r8, %r11
	movq	%r13, -40(%rdi)
	movq	-40(%rdi), %rdx
	addq	%rbx, %r11
	movq	72(%rdi), %rbx
	movq	32(%rdi), %rax
	movq	-16(%rdi), %rsi
	movq	-32(%rdi), %r8
	movl	%r14d, %r13d
	addq	%rdx, %r11
	xorq	%rax, %rbx 
	addq	%r10, %r11
	xorq	%rsi, %rbx 
	xorl	%r8d, %ebx
	xorq	%r12, %r14 
	movl	%r11d, %r8d
	xorq	%r15, %r14 
	roll	%ebx
	roll	$5, %r8d
	movq	%rbx, -32(%rdi)
	addq	%r14, %r8
	movq	-32(%rdi), %rcx
	movq	-48(%rdi), %r14
	movq	40(%rdi), %rdx
	addq	%r9, %r8
	movq	-8(%rdi), %rax
	movq	-24(%rdi), %r9
	roll	$30, %r13d
	addq	%rcx, %r8
	movl	%r11d, %ebx
	xorq	%r13, %r11 
	xorq	%rdx, %r14 
	addq	%r10, %r8
	xorq	%r12, %r11 
	xorq	%rax, %r14 
	roll	$30, %ebx
	xorl	%r9d, %r14d
	movl	%r8d, %r9d
	roll	$5, %r9d
	roll	%r14d
	addq	%r11, %r9
	movq	%r14, -24(%rdi)
	movq	-24(%rdi), %rsi
	addq	%r15, %r9
	movq	-40(%rdi), %r15
	movq	48(%rdi), %rcx
	movq	(%rdi), %rdx
	movq	-16(%rdi), %r11
	movl	%r8d, %r14d
	addq	%rsi, %r9
	xorq	%rbx, %r8 
	xorq	%rcx, %r15 
	addq	%r10, %r9
	xorq	%r13, %r8 
	xorq	%rdx, %r15 
	xorl	%r11d, %r15d
	movl	%r9d, %r11d
	roll	%r15d
	roll	$5, %r11d
	movq	%r15, -16(%rdi)
	addq	%r8, %r11
	movq	-16(%rdi), %rax
	addq	%r12, %r11
	movq	-32(%rdi), %r12
	movq	56(%rdi), %rsi
	movq	8(%rdi), %rcx
	movq	-8(%rdi), %r8
	movl	%r9d, %r15d
	addq	%rax, %r11
	addq	%r10, %r11
	roll	$30, %r14d
	xorq	%rsi, %r12 
	xorq	%rcx, %r12 
	xorq	%r14, %r9 
	roll	$30, %r15d
	xorl	%r8d, %r12d
	movl	%r11d, %r8d
	xorq	%rbx, %r9 
	roll	$5, %r8d
	roll	%r12d
	addq	%r9, %r8
	movq	%r12, -8(%rdi)
	movq	-8(%rdi), %rdx
	addq	%r13, %r8
	movq	-24(%rdi), %r13
	movq	64(%rdi), %rax
	movq	16(%rdi), %rsi
	movq	(%rdi), %rcx
	movl	%r11d, %r12d
	addq	%rdx, %r8
	xorq	%r15, %r11 
	roll	$30, %r12d
	xorq	%rax, %r13 
	addq	%r10, %r8
	xorq	%r14, %r11 
	xorq	%rsi, %r13 
	xorl	%ecx, %r13d
	movl	%r8d, %ecx
	roll	$5, %ecx
	roll	%r13d
	addq	%r11, %rcx
	movq	%r13, (%rdi)
	movq	(%rdi), %r9
	addq	%rbx, %rcx
	movq	-16(%rdi), %rbx
	movq	72(%rdi), %rdx
	movq	24(%rdi), %rax
	movq	8(%rdi), %rsi
	movl	%r8d, %r13d
	addq	%r9, %rcx
	xorq	%r12, %r8 
	xorq	%rdx, %rbx 
	addq	%r10, %rcx
	xorq	%r15, %r8 
	xorq	%rax, %rbx 
	xorl	%esi, %ebx
	movl	%ecx, %esi
	roll	$5, %esi
	roll	%ebx
	addq	%r8, %rsi
	movq	%rbx, 8(%rdi)
	movq	8(%rdi), %r11
	addq	%r14, %rsi
	movq	-8(%rdi), %r14
	movq	-48(%rdi), %r9
	movq	32(%rdi), %rdx
	movq	16(%rdi), %r8
	roll	$30, %r13d
	addq	%r11, %rsi
	movl	%ecx, %ebx
	xorq	%r13, %rcx 
	xorq	%r9, %r14 
	addq	%r10, %rsi
	xorq	%r12, %rcx 
	xorq	%rdx, %r14 
	roll	$30, %ebx
	xorl	%r8d, %r14d
	movl	%esi, %r8d
	roll	$5, %r8d
	roll	%r14d
	addq	%rcx, %r8
	movq	%r14, 16(%rdi)
	movq	16(%rdi), %rax
	addq	%r15, %r8
	movq	(%rdi), %r15
	movq	-40(%rdi), %r11
	movq	40(%rdi), %r9
	movq	24(%rdi), %rcx
	movl	%esi, %r14d
	addq	%rax, %r8
	xorq	%rbx, %rsi 
	roll	$30, %r14d
	xorq	%r11, %r15 
	addq	%r10, %r8
	xorq	%r13, %rsi 
	xorq	%r9, %r15 
	xorl	%ecx, %r15d
	movl	%r8d, %ecx
	roll	%r15d
	roll	$5, %ecx
	movq	%r15, 24(%rdi)
	addq	%rsi, %rcx
	movq	24(%rdi), %rdx
	movq	8(%rdi), %r11
	movq	-32(%rdi), %rax
	addq	%r12, %rcx
	movq	48(%rdi), %r12
	movq	32(%rdi), %rsi
	movl	%r8d, %r15d
	addq	%rdx, %rcx
	xorq	%rax, %r11 
	addq	%r10, %rcx
	xorq	%r12, %r11 
	xorl	%esi, %r11d
	movl	%ecx, %esi
	roll	%r11d
	movq	%r11, 32(%rdi)
	movl	%ecx, %r11d
	movq	32(%rdi), %r9
	roll	$5, %r11d
	xorq	%r14, %r8 
	movq	16(%rdi), %r12
	xorq	%rbx, %r8 
	movq	-24(%rdi), %rdx
	movq	56(%rdi), %rax
	addq	%r8, %r11
	movq	40(%rdi), %r8
	roll	$30, %r15d
	addq	%r13, %r11
	xorq	%r15, %rcx 
	addq	%r9, %r11
	xorq	%rdx, %r12 
	xorq	%r14, %rcx 
	addq	%r10, %r11
	xorq	%rax, %r12 
	xorl	%r8d, %r12d
	movl	%r11d, %r8d
	roll	$5, %r8d
	roll	%r12d
	addq	%rcx, %r8
	movq	%r12, 40(%rdi)
	movq	40(%rdi), %r13
	addq	%rbx, %r8
	movq	24(%rdi), %rbx
	movq	-16(%rdi), %r9
	movq	64(%rdi), %rdx
	movq	48(%rdi), %rcx
	movl	%r11d, %r12d
	addq	%r13, %r8
	movl	%esi, %r13d
	roll	$30, %r12d
	xorq	%r9, %rbx 
	addq	%r10, %r8
	roll	$30, %r13d
	xorq	%rdx, %rbx 
	xorq	%r13, %r11 
	xorl	%ecx, %ebx
	movl	%r8d, %ecx
	xorq	%r15, %r11 
	roll	%ebx
	roll	$5, %ecx
	movq	%rbx, 48(%rdi)
	addq	%r11, %rcx
	movq	48(%rdi), %rax
	movq	32(%rdi), %r11
	movq	-8(%rdi), %rsi
	addq	%r14, %rcx
	movq	72(%rdi), %r9
	movq	56(%rdi), %r14
	movl	%r8d, %ebx
	addq	%rax, %rcx
	xorq	%rsi, %r11 
	addq	%r10, %rcx
	xorq	%r9, %r11 
	xorl	%r14d, %r11d
	xorq	%r12, %r8 
	movl	%ecx, %r14d
	xorq	%r13, %r8 
	roll	%r11d
	roll	$5, %r14d
	movq	%r11, 56(%rdi)
	addq	%r8, %r14
	movq	56(%rdi), %rdx
	movq	40(%rdi), %r8
	movq	(%rdi), %rax
	addq	%r15, %r14
	movq	-48(%rdi), %r15
	movq	64(%rdi), %rsi
	roll	$30, %ebx
	addq	%rdx, %r14
	movl	%ecx, %r11d
	xorq	%rbx, %rcx 
	xorq	%rax, %r8 
	addq	%r10, %r14
	xorq	%r12, %rcx 
	xorq	%r15, %r8 
	roll	$30, %r11d
	xorl	%esi, %r8d
	movl	%r14d, %esi
	roll	%r8d
	roll	$5, %esi
	movq	%r8, 64(%rdi)
	movq	64(%rdi), %r9
	addq	%rcx, %rsi
	movq	48(%rdi), %r15
	movq	8(%rdi), %rcx
	addq	%r13, %rsi
	movq	-40(%rdi), %rdx
	movq	72(%rdi), %rax
	movl	%r14d, %r8d
	addq	%r9, %rsi
	xorq	%r11, %r14 
	addq	%r10, %rsi
	xorq	%rcx, %r15 
	xorq	%rbx, %r14 
	xorq	%rdx, %r15 
	movl	%esi, %r13d
	xorl	%eax, %r15d
	roll	$5, %r13d
	roll	%r15d
	addq	%r14, %r13
	movq	%r15, 72(%rdi)
	addq	%r12, %r13
	movq	72(%rdi), %r12
	addq	%r12, %r13
	addq	%r10, %r13
	movq	-88(%rdi), %r10
	roll	$30, %r8d
	addq	%r13, %r10
	movq	%r10, -88(%rdi)
	movq	-80(%rdi), %r9
	addq	%rsi, %r9
	movq	%r9, -80(%rdi)
	movq	-72(%rdi), %rcx
	addq	%r8, %rcx
	movq	%rcx, -72(%rdi)
	movq	-64(%rdi), %rdx
	addq	%r11, %rdx
	movq	%rdx, -64(%rdi)
	movq	-56(%rdi), %rax
	addq	%rbx, %rax
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	movq	%rax, -56(%rdi)
	ret
.LFE7:
	.size	shaCompress, .-shaCompress
	.align 16
.globl SHA1_Update
	.type	SHA1_Update, @function
SHA1_Update:
.LFB5:
	pushq	%rbp
.LCFI5:
	movq	%rsp, %rbp
.LCFI6:
	movq	%r13, -24(%rbp)
.LCFI7:
	movq	%r14, -16(%rbp)
.LCFI8:
	movl	%edx, %r13d
	movq	%r15, -8(%rbp)
.LCFI9:
	movq	%rbx, -40(%rbp)
.LCFI10:
	movq	%rdi, %r15
	movq	%r12, -32(%rbp)
.LCFI11:
	subq	$48, %rsp
.LCFI12:
	testl	%edx, %edx
	movq	%rsi, %r14
	je	.L243
	movq	64(%rdi), %rdx
	mov	%r13d, %ecx
	leaq	(%rdx,%rcx), %rax
	movq	%rax, 64(%rdi)
	movl	%edx, %eax
	andl	$63, %eax
	movl	%eax, -44(%rbp)
	jne	.L256
.L245:
	cmpl	$63, %r13d
	jbe	.L253
	leaq	160(%r15), %rbx
	.align 16
.L250:
	movq	%r14, %rsi
	subl	$64, %r13d
	movq	%rbx, %rdi
	call	shaCompress
	addq	$64, %r14
	cmpl	$63, %r13d
	ja	.L250
.L253:
	testl	%r13d, %r13d
	je	.L243
	mov	%r13d, %edx
	movq	%r14, %rsi
	movq	%r15, %rdi
	movq	-40(%rbp), %rbx
	movq	-32(%rbp), %r12
	movq	-24(%rbp), %r13
	movq	-16(%rbp), %r14
	movq	-8(%rbp), %r15
	leave
	jmp	memcpy@PLT
	.align 16
.L243:
	movq	-40(%rbp), %rbx
	movq	-32(%rbp), %r12
	movq	-24(%rbp), %r13
	movq	-16(%rbp), %r14
	movq	-8(%rbp), %r15
	leave
	ret
.L256:
	movl	$64, %ebx
	mov	%eax, %edi
	subl	%eax, %ebx
	cmpl	%ebx, %r13d
	cmovb	%r13d, %ebx
	addq	%r15, %rdi
	mov	%ebx, %r12d
	subl	%ebx, %r13d
	movq	%r12, %rdx
	addq	%r12, %r14
	call	memcpy@PLT
	addl	-44(%rbp), %ebx
	andl	$63, %ebx
	jne	.L245
	leaq	160(%r15), %rdi
	movq	%r15, %rsi
	call	shaCompress
	jmp	.L245
.LFE5:
	.size	SHA1_Update, .-SHA1_Update
	.section	.rodata
	.align 32
	.type	bulk_pad.0, @object
	.size	bulk_pad.0, 64
bulk_pad.0:
	.byte	-128
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.byte	0
	.text
	.align 16
.globl SHA1_End
	.type	SHA1_End, @function
SHA1_End:
.LFB6:
	pushq	%rbp
.LCFI13:
	movq	%rsp, %rbp
.LCFI14:
	movq	%r12, -24(%rbp)
.LCFI15:
	movq	%r13, -16(%rbp)
.LCFI16:
	movq	%rsi, %r13
	movq	%r14, -8(%rbp)
.LCFI17:
	movq	%rbx, -32(%rbp)
.LCFI18:
	subq	$32, %rsp
.LCFI19:
	movq	64(%rdi), %rbx
	movq	%rdx, %r14
	movl	$119, %edx
	leaq	bulk_pad.0(%rip), %rsi
	movq	%rdi, %r12
	movl	%ebx, %r8d
	salq	$3, %rbx
	andl	$63, %r8d
	subl	%r8d, %edx
	andl	$63, %edx
	incl	%edx
	call	SHA1_Update@PLT
	movq	%rbx, %rdi
	movq	%r12, %rsi
	shrq	$32, %rdi
/APP
	bswap %edi
/NO_APP
	movl	%edi, 56(%r12)
	leaq	160(%r12), %rdi
/APP
	bswap %ebx
/NO_APP
	movl	%ebx, 60(%r12)
	call	shaCompress
	movl	72(%r12), %esi
	movl	80(%r12), %ebx
	movl	88(%r12), %ecx
	movl	96(%r12), %edx
	movl	104(%r12), %eax
	movq	8(%rsp), %r12
/APP
	bswap %ebx
	bswap %esi
/NO_APP
	movl	%ebx, 4(%r13)
	movl	%esi, (%r13)
/APP
	bswap %ecx
	bswap %edx
/NO_APP
	movl	%ecx, 8(%r13)
	movl	%edx, 12(%r13)
/APP
	bswap %eax
/NO_APP
	movq	(%rsp), %rbx
	movl	%eax, 16(%r13)
        cmpq    $0, %r14
        je      .L133
	movl	$20, (%r14)
.L133:
	movq	16(%rsp), %r13
	movq	24(%rsp), %r14
	leave
	ret
.LFE6:
	.size	SHA1_End, .-SHA1_End
	.align 16
.globl SHA1_NewContext
	.type	SHA1_NewContext, @function
SHA1_NewContext:
.LFB8:
	movl	$248, %edi
	jmp	PORT_Alloc_Util@PLT
.LFE8:
	.size	SHA1_NewContext, .-SHA1_NewContext
	.align 16
.globl SHA1_DestroyContext
	.type	SHA1_DestroyContext, @function
SHA1_DestroyContext:
.LFB9:
	pushq	%rbp
.LCFI20:
	movl	$248, %edx
	movq	%rsp, %rbp
.LCFI21:
	movq	%rbx, -16(%rbp)
.LCFI22:
	movq	%r12, -8(%rbp)
.LCFI23:
	movl	%esi, %ebx
	subq	$16, %rsp
.LCFI24:
	xorl	%esi, %esi
	movq	%rdi, %r12
	call	memset@PLT
	testl	%ebx, %ebx
	jne	.L268
	movq	(%rsp), %rbx
	movq	8(%rsp), %r12
	leave
	ret
	.align 16
.L268:
	movq	%r12, %rdi
	movq	(%rsp), %rbx
	movq	8(%rsp), %r12
	leave
	jmp	PORT_Free_Util@PLT
.LFE9:
	.size	SHA1_DestroyContext, .-SHA1_DestroyContext
	.align 16
.globl SHA1_HashBuf
	.type	SHA1_HashBuf, @function
SHA1_HashBuf:
.LFB10:
	pushq	%rbp
.LCFI25:
	movq	%rsp, %rbp
.LCFI26:
	movq	%rbx, -32(%rbp)
.LCFI27:
	leaq	-288(%rbp), %rbx
	movq	%r12, -24(%rbp)
.LCFI28:
	movq	%r13, -16(%rbp)
.LCFI29:
	movq	%r14, -8(%rbp)
.LCFI30:
	movq	%rsi, %r13
	subq	$304, %rsp
.LCFI31:
	movq	%rdi, %r14
	movl	%edx, %r12d
	movq	%rbx, %rdi
	call	SHA1_Begin@PLT
	movl	%r12d, %edx
	movq	%r13, %rsi
	movq	%rbx, %rdi
	call	SHA1_Update@PLT
	leaq	-292(%rbp), %rdx
	movq	%r14, %rsi
	movq	%rbx, %rdi
	movl	$20, %ecx
	call	SHA1_End@PLT
	movq	-32(%rbp), %rbx
	movq	-24(%rbp), %r12
	xorl	%eax, %eax
	movq	-16(%rbp), %r13
	movq	-8(%rbp), %r14
	leave
	ret
.LFE10:
	.size	SHA1_HashBuf, .-SHA1_HashBuf
	.align 16
.globl SHA1_Hash
	.type	SHA1_Hash, @function
SHA1_Hash:
.LFB11:
	pushq	%rbp
.LCFI32:
	movq	%rsp, %rbp
.LCFI33:
	movq	%rbx, -16(%rbp)
.LCFI34:
	movq	%r12, -8(%rbp)
.LCFI35:
	movq	%rsi, %rbx
	subq	$16, %rsp
.LCFI36:
	movq	%rdi, %r12
	movq	%rsi, %rdi
	call	strlen@PLT
	movq	%rbx, %rsi
	movq	%r12, %rdi
	movq	(%rsp), %rbx
	movq	8(%rsp), %r12
	leave
	movl	%eax, %edx
	jmp	SHA1_HashBuf@PLT
.LFE11:
	.size	SHA1_Hash, .-SHA1_Hash
	.align 16
.globl SHA1_FlattenSize
	.type	SHA1_FlattenSize, @function
SHA1_FlattenSize:
.LFB12:
	movl	$248, %eax
	ret
.LFE12:
	.size	SHA1_FlattenSize, .-SHA1_FlattenSize
	.align 16
.globl SHA1_Flatten
	.type	SHA1_Flatten, @function
SHA1_Flatten:
.LFB13:
	pushq	%rbp
.LCFI37:
	movq	%rsi, %rax
	movl	$248, %edx
	movq	%rdi, %rsi
	movq	%rax, %rdi
	movq	%rsp, %rbp
.LCFI38:
	call	memcpy@PLT
	leave
	xorl	%eax, %eax
	ret
.LFE13:
	.size	SHA1_Flatten, .-SHA1_Flatten
	.align 16
.globl SHA1_Resurrect
	.type	SHA1_Resurrect, @function
SHA1_Resurrect:
.LFB14:
	pushq	%rbp
.LCFI39:
	movq	%rsp, %rbp
.LCFI40:
	movq	%rbx, -16(%rbp)
.LCFI41:
	movq	%r12, -8(%rbp)
.LCFI42:
	subq	$16, %rsp
.LCFI43:
	movq	%rdi, %r12
	call	SHA1_NewContext@PLT
	movq	%rax, %rbx
	xorl	%eax, %eax
	testq	%rbx, %rbx
	je	.L273
	movl	$248, %edx
	movq	%r12, %rsi
	movq	%rbx, %rdi
	call	memcpy@PLT
	movq	%rbx, %rax
.L273:
	movq	(%rsp), %rbx
	movq	8(%rsp), %r12
	leave
	ret
.LFE14:
	.size	SHA1_Resurrect, .-SHA1_Resurrect
	.align 16
.globl SHA1_Clone
	.type	SHA1_Clone, @function
SHA1_Clone:
.LFB15:
	movl	$248, %edx
	jmp	memcpy@PLT
.LFE15:
	.size	SHA1_Clone, .-SHA1_Clone
	.align 16
.globl SHA1_TraceState
	.type	SHA1_TraceState, @function
SHA1_TraceState:
.LFB16:
	movl	$-5992, %edi
	jmp	PORT_SetError_Util@PLT
.LFE16:
	.size	SHA1_TraceState, .-SHA1_TraceState
	.align 16
.globl SHA1_EndRaw
        .type   SHA1_EndRaw, @function
SHA1_EndRaw:
.LFB50:
        movq    72(%rdi), %rax
/APP
        bswap %eax
/NO_APP
        movl    %eax, (%rsi)
        movq    80(%rdi), %rax
/APP
        bswap %eax
/NO_APP
        movl    %eax, 4(%rsi)
        movq    88(%rdi), %rax
/APP
        bswap %eax
/NO_APP
        movl    %eax, 8(%rsi)
        movq    96(%rdi), %rax
/APP
        bswap %eax
/NO_APP
        movl    %eax, 12(%rsi)
        movq    104(%rdi), %rax
/APP
        bswap %eax
/NO_APP
        testq   %rdx, %rdx
        movl    %eax, 16(%rsi)
        je      .L14
        movl    $20, (%rdx)
.L14:
        rep
        ret
.LFE50:
        .size   SHA1_EndRaw, .-SHA1_EndRaw
