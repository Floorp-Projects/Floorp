//* TomsFastMath, a fast ISO C bignum library.
/ * 
/ * This project is meant to fill in where LibTomMath
/ * falls short.  That is speed ;-)
/ *
/ * This project is public domain and free for all purposes.
/ * 
/ * Tom St Denis, tomstdenis@iahu.ca
/ */

//*
/ * The source file from which this assembly was derived
/ * comes from TFM v0.03, which has the above license.
/ * This source was compiled with an unnamed compiler at
/ * the highest optimization level.  Afterwards, the
/ * trailing .section was removed because it causes errors
/ * in the Studio 10 compiler on AMD 64.
/ */

       	.file	"mp_comba.c"
	.text
	.align 16
.globl s_mp_mul_comba_4
	.type	s_mp_mul_comba_4, @function
s_mp_mul_comba_4:
.LFB2:
	pushq	%r12
.LCFI0:
	pushq	%rbp
.LCFI1:
	pushq	%rbx
.LCFI2:
	movq	16(%rdi), %r9
	movq	%rdx, %rbx
	movq	16(%rsi), %rdx
	movq	(%r9), %rax
	movq	%rax, -64(%rsp)
	movq	8(%r9), %r8
	movq	%r8, -56(%rsp)
	movq	16(%r9), %rbp
	movq	%rbp, -48(%rsp)
	movq	24(%r9), %r12
	movq	%r12, -40(%rsp)
	movq	(%rdx), %rcx
	movq	%rcx, -32(%rsp)
	movq	8(%rdx), %r10
	movq	%r10, -24(%rsp)
	movq	16(%rdx), %r11
	xorl	%r10d, %r10d
	movq	%r10, %r8
	movq	%r10, %r9
	movq	%r10, %rbp
	movq	%r11, -16(%rsp)
	movq	16(%rbx), %r11
	movq	24(%rdx), %rax
	movq	%rax, -8(%rsp)
/APP
	movq  -64(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%r8, (%r11)
	movq	%rbp, %r8
	movq	%r10, %rbp
/APP
	movq  -64(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r9     
	adcq  %rdx,%r8     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%rbp, %r12
/APP
	movq  -56(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r9     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r9, 8(%r11)
	movq	%r12, %r9
	movq	%r10, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r12, %rcx
/APP
	movq  -56(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -48(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 16(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -64(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -40(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 24(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -56(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -40(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 32(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -48(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r8, %r12
	movq	%r9, %rbp
/APP
	movq  -40(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 40(%r11)
	movq	%rbp, %r8
	movq	%r12, %rcx
/APP
	movq  -40(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rcx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r8, 48(%r11)
	movl	(%rsi), %esi
	xorl	(%rdi), %esi
	testq	%rcx, %rcx
	movq	%rcx, 56(%r11)
	movl	$8, 8(%rbx)
	jne	.L9
	.align 16
.L18:
	movl	8(%rbx), %edx
	leal	-1(%rdx), %edi
	testl	%edi, %edi
	movl	%edi, 8(%rbx)
	je	.L9
	leal	-2(%rdx), %r10d
	cmpq	$0, (%r11,%r10,8)
	je	.L18
.L9:
	movl	8(%rbx), %edx
	xorl	%r11d, %r11d
	testl	%edx, %edx
	cmovne	%esi, %r11d
	movl	%r11d, (%rbx)
	popq	%rbx
	popq	%rbp
	popq	%r12
	ret
.LFE2:
	.size	s_mp_mul_comba_4, .-s_mp_mul_comba_4
	.align 16
.globl s_mp_mul_comba_8
	.type	s_mp_mul_comba_8, @function
s_mp_mul_comba_8:
.LFB3:
	pushq	%r12
.LCFI3:
	pushq	%rbp
.LCFI4:
	pushq	%rbx
.LCFI5:
	movq	%rdx, %rbx
	subq	$8, %rsp
.LCFI6:
	movq	16(%rdi), %rdx
	movq	(%rdx), %r8
	movq	%r8, -120(%rsp)
	movq	8(%rdx), %rbp
	movq	%rbp, -112(%rsp)
	movq	16(%rdx), %r9
	movq	%r9, -104(%rsp)
	movq	24(%rdx), %r12
	movq	%r12, -96(%rsp)
	movq	32(%rdx), %rcx
	movq	%rcx, -88(%rsp)
	movq	40(%rdx), %r10
	movq	%r10, -80(%rsp)
	movq	48(%rdx), %r11
	movq	%r11, -72(%rsp)
	movq	56(%rdx), %rax
	movq	16(%rsi), %rdx
	movq	%rax, -64(%rsp)
	movq	(%rdx), %r8
	movq	%r8, -56(%rsp)
	movq	8(%rdx), %rbp
	movq	%rbp, -48(%rsp)
	movq	16(%rdx), %r9
	movq	%r9, -40(%rsp)
	movq	24(%rdx), %r12
	movq	%r12, -32(%rsp)
	movq	32(%rdx), %rcx
	movq	%rcx, -24(%rsp)
	movq	40(%rdx), %r10
	movq	%r10, -16(%rsp)
	movq	48(%rdx), %r11
	xorl	%r10d, %r10d
	movq	%r10, %r8
	movq	%r10, %r9
	movq	%r10, %rbp
	movq	%r11, -8(%rsp)
	movq	16(%rbx), %r11
	movq	56(%rdx), %rax
	movq	%rax, (%rsp)
/APP
	movq  -120(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%r8, (%r11)
	movq	%rbp, %r8
	movq	%r10, %rbp
/APP
	movq  -120(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%r9     
	adcq  %rdx,%r8     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%rbp, %r12
/APP
	movq  -112(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%r9     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r9, 8(%r11)
	movq	%r12, %r9
	movq	%r10, %r12
/APP
	movq  -120(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r12, %rcx
/APP
	movq  -112(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -104(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 16(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -96(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 24(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -88(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 32(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -80(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 40(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -72(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 48(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 56(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -112(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 64(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -104(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 72(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -96(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 80(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -88(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 88(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -80(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  -16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 96(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -72(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r8, %r12
	movq	%r9, %rbp
/APP
	movq  -64(%rsp),%rax     
	mulq  -8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 104(%r11)
	movq	%rbp, %r8
	movq	%r12, %rcx
/APP
	movq  -64(%rsp),%rax     
	mulq  (%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rcx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r8, 112(%r11)
	movl	(%rsi), %esi
	xorl	(%rdi), %esi
	testq	%rcx, %rcx
	movq	%rcx, 120(%r11)
	movl	$16, 8(%rbx)
	jne	.L35
	.align 16
.L43:
	movl	8(%rbx), %edx
	leal	-1(%rdx), %edi
	testl	%edi, %edi
	movl	%edi, 8(%rbx)
	je	.L35
	leal	-2(%rdx), %eax
	cmpq	$0, (%r11,%rax,8)
	je	.L43
.L35:
	movl	8(%rbx), %r11d
	xorl	%edx, %edx
	testl	%r11d, %r11d
	cmovne	%esi, %edx
	movl	%edx, (%rbx)
	addq	$8, %rsp
	popq	%rbx
	popq	%rbp
	popq	%r12
	ret
.LFE3:
	.size	s_mp_mul_comba_8, .-s_mp_mul_comba_8
	.align 16
.globl s_mp_mul_comba_16
	.type	s_mp_mul_comba_16, @function
s_mp_mul_comba_16:
.LFB4:
	pushq	%r12
.LCFI7:
	pushq	%rbp
.LCFI8:
	pushq	%rbx
.LCFI9:
	movq	%rdx, %rbx
	subq	$136, %rsp
.LCFI10:
	movq	16(%rdi), %rax
	movq	(%rax), %r8
	movq	%r8, -120(%rsp)
	movq	8(%rax), %rbp
	movq	%rbp, -112(%rsp)
	movq	16(%rax), %r9
	movq	%r9, -104(%rsp)
	movq	24(%rax), %r12
	movq	%r12, -96(%rsp)
	movq	32(%rax), %rcx
	movq	%rcx, -88(%rsp)
	movq	40(%rax), %r10
	movq	%r10, -80(%rsp)
	movq	48(%rax), %rdx
	movq	%rdx, -72(%rsp)
	movq	56(%rax), %r11
	movq	%r11, -64(%rsp)
	movq	64(%rax), %r8
	movq	%r8, -56(%rsp)
	movq	72(%rax), %rbp
	movq	%rbp, -48(%rsp)
	movq	80(%rax), %r9
	movq	%r9, -40(%rsp)
	movq	88(%rax), %r12
	movq	%r12, -32(%rsp)
	movq	96(%rax), %rcx
	movq	%rcx, -24(%rsp)
	movq	104(%rax), %r10
	movq	%r10, -16(%rsp)
	movq	112(%rax), %rdx
	movq	%rdx, -8(%rsp)
	movq	120(%rax), %r11
	movq	%r11, (%rsp)
	movq	16(%rsi), %r11
	movq	(%r11), %r8
	movq	%r8, 8(%rsp)
	movq	8(%r11), %rbp
	movq	%rbp, 16(%rsp)
	movq	16(%r11), %r9
	movq	%r9, 24(%rsp)
	movq	24(%r11), %r12
	movq	%r12, 32(%rsp)
	movq	32(%r11), %rcx
	movq	%rcx, 40(%rsp)
	movq	40(%r11), %r10
	movq	%r10, 48(%rsp)
	movq	48(%r11), %rdx
	movq	%rdx, 56(%rsp)
	movq	56(%r11), %rax
	movq	%rax, 64(%rsp)
	movq	64(%r11), %r8
	movq	%r8, 72(%rsp)
	movq	72(%r11), %rbp
	movq	%rbp, 80(%rsp)
	movq	80(%r11), %r9
	movq	%r9, 88(%rsp)
	movq	88(%r11), %r12
	movq	%r12, 96(%rsp)
	movq	96(%r11), %rcx
	movq	%rcx, 104(%rsp)
	movq	104(%r11), %r10
	movq	%r10, 112(%rsp)
	movq	112(%r11), %rdx
	xorl	%r10d, %r10d
	movq	%r10, %r8
	movq	%r10, %r9
	movq	%r10, %rbp
	movq	%rdx, 120(%rsp)
	movq	120(%r11), %rax
	movq	%rax, 128(%rsp)
	movq	16(%rbx), %r11
/APP
	movq  -120(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%r8, (%r11)
	movq	%rbp, %r8
	movq	%r10, %rbp
/APP
	movq  -120(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r9     
	adcq  %rdx,%r8     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%rbp, %r12
/APP
	movq  -112(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r9     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r9, 8(%r11)
	movq	%r12, %r9
	movq	%r10, %r12
/APP
	movq  -120(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r12, %rcx
/APP
	movq  -112(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -104(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 16(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -96(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 24(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -88(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 32(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -80(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 40(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -72(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 48(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -64(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 56(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -56(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 64(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -48(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 72(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -40(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 80(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -32(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 88(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -24(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 96(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  -16(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 104(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -120(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -112(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  -8(%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 112(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -120(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -112(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -104(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  8(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 120(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -112(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -104(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -96(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  16(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 128(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -104(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -96(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -88(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  24(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 136(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -96(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -88(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -80(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  32(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 144(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -88(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -80(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -72(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  40(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 152(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -80(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -72(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -64(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  48(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 160(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -72(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -64(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -56(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  56(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 168(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -64(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -56(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -48(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  64(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 176(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -56(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -48(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -40(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  72(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 184(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -48(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -40(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -32(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  80(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 192(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -40(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -32(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -24(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  88(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 200(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -32(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -24(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -16(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  96(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 208(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -24(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -16(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
	movq  -8(%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbp
	movq	%r8, %r12
/APP
	movq  (%rsp),%rax     
	mulq  104(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 216(%r11)
	movq	%r12, %r9
	movq	%rbp, %r8
	movq	%r10, %rcx
/APP
	movq  -16(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
	movq  -8(%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%r9, %rbp
	movq	%rcx, %r12
/APP
	movq  (%rsp),%rax     
	mulq  112(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, 224(%r11)
	movq	%r12, %r9
	movq	%rbp, %rcx
	movq	%r10, %r8
/APP
	movq  -8(%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r8, %r12
	movq	%r9, %rbp
/APP
	movq  (%rsp),%rax     
	mulq  120(%rsp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rbp     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rcx, 232(%r11)
	movq	%rbp, %r8
	movq	%r12, %rcx
/APP
	movq  (%rsp),%rax     
	mulq  128(%rsp)           
	addq  %rax,%r8     
	adcq  %rdx,%rcx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r8, 240(%r11)
	movl	(%rsi), %esi
	xorl	(%rdi), %esi
	testq	%rcx, %rcx
	movq	%rcx, 248(%r11)
	movl	$32, 8(%rbx)
	jne	.L76
	.align 16
.L84:
	movl	8(%rbx), %edx
	leal	-1(%rdx), %edi
	testl	%edi, %edi
	movl	%edi, 8(%rbx)
	je	.L76
	leal	-2(%rdx), %eax
	cmpq	$0, (%r11,%rax,8)
	je	.L84
.L76:
	movl	8(%rbx), %edx
	xorl	%r11d, %r11d
	testl	%edx, %edx
	cmovne	%esi, %r11d
	movl	%r11d, (%rbx)
	addq	$136, %rsp
	popq	%rbx
	popq	%rbp
	popq	%r12
	ret
.LFE4:
	.size	s_mp_mul_comba_16, .-s_mp_mul_comba_16
	.align 16
.globl s_mp_mul_comba_32
	.type	s_mp_mul_comba_32, @function
s_mp_mul_comba_32:
.LFB5:
	pushq	%rbp
.LCFI11:
	movq	%rsp, %rbp
.LCFI12:
	pushq	%r13
.LCFI13:
	movq	%rdx, %r13
	movl	$256, %edx
	pushq	%r12
.LCFI14:
	movq	%rsi, %r12
	pushq	%rbx
.LCFI15:
	movq	%rdi, %rbx
	subq	$520, %rsp
.LCFI16:
	movq	16(%rdi), %rsi
	leaq	-544(%rbp), %rdi
	call	memcpy@PLT
	movq	16(%r12), %rsi
	leaq	-288(%rbp), %rdi
	movl	$256, %edx
	call	memcpy@PLT
	movq	16(%r13), %r9
	xorl	%r8d, %r8d
	movq	%r8, %rsi
	movq	%r8, %rdi
	movq	%r8, %r10
/APP
	movq  -544(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
/NO_APP
	movq	%rsi, (%r9)
	movq	%r10, %rsi
	movq	%r8, %r10
/APP
	movq  -544(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r10, %r11
/APP
	movq  -536(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rdi, 8(%r9)
	movq	%r11, %rdi
	movq	%r8, %r11
/APP
	movq  -544(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r11, %rcx
/APP
	movq  -536(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -528(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 16(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -520(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 24(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -512(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 32(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -504(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 40(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -496(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 48(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -488(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 56(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -480(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 64(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -472(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 72(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -464(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 80(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -456(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 88(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -448(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 96(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -440(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 104(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -432(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 112(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -424(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 120(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -416(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 128(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -408(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 136(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -400(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 144(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -392(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 152(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -384(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 160(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -376(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 168(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -368(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 176(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -360(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 184(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -352(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 192(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -344(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 200(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -336(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 208(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -328(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 216(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -320(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 224(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -312(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 232(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -544(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -536(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -304(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 240(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -544(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -536(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -528(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -288(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 248(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -536(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -528(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -520(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -280(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 256(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -528(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -520(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -512(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -272(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 264(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -520(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -512(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -504(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -264(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 272(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -512(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -504(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -496(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -256(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 280(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -504(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -496(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -488(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -248(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 288(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -496(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -488(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -480(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -240(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 296(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -488(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -480(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -472(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -232(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 304(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -480(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -472(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -464(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -224(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 312(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -472(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -464(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -456(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -448(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -440(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -216(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 320(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -464(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -456(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -448(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -440(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -432(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -208(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 328(%r9)
	movq	%r11, %rdi
	movq	%r10, %r11
	movq	%r8, %r10
/APP
	movq  -456(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -448(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -440(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -432(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -424(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -416(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -408(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -400(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -392(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -384(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -376(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -368(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -360(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -352(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -344(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -336(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -328(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -320(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -312(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -304(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
	movq  -296(%rbp),%rax     
	mulq  -200(%rbp)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r11, 336(%r9)
	movq	%r10, %rsi
	movq	%r8, %r10
/APP
	movq  -448(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r10, %rcx
/APP
	movq  -440(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -432(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rsi, %r11
	movq	%rcx, %r10
/APP
	movq  -296(%rbp),%rax     
	mulq  -192(%rbp)           
	addq  %rax,%rdi     
	adcq  %rdx,%r11     
	adcq  $0,%r10        
	
/NO_APP
	movq	%rdi, 344(%r9)
	movq	%r11, %rcx
	movq	%r10, %rdi
	movq	%r8, %r11
/APP
	movq  -440(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r11, %rsi
/APP
	movq  -432(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -424(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -184(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 352(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -432(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -424(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -416(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -176(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 360(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -424(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -416(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -408(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -168(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 368(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -416(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -408(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -400(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -160(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 376(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -408(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -400(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -392(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -152(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 384(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -400(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -392(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -384(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -144(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 392(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -392(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -384(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -376(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -136(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 400(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -384(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -376(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -368(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -128(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 408(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -376(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -368(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -360(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -120(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 416(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -368(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -360(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -352(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -112(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 424(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -360(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -352(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -344(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -104(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 432(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -352(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -344(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -336(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -96(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 440(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -344(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -336(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -328(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -88(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 448(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -336(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -328(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -320(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -80(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 456(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -328(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -320(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -312(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -72(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 464(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -320(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -312(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
	movq  -304(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rcx, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -64(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 472(%r9)
	movq	%r11, %rdi
	movq	%r10, %rcx
	movq	%r8, %rsi
/APP
	movq  -312(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  -304(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r10
	movq	%rsi, %r11
/APP
	movq  -296(%rbp),%rax     
	mulq  -56(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rcx, 480(%r9)
	movq	%r11, %rdi
	movq	%r10, %rsi
	movq	%r8, %rcx
/APP
	movq  -304(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%rdi     
	adcq  $0,%rcx        
	
/NO_APP
	movq	%rcx, %r11
	movq	%rdi, %r10
/APP
	movq  -296(%rbp),%rax     
	mulq  -48(%rbp)           
	addq  %rax,%rsi     
	adcq  %rdx,%r10     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rsi, 488(%r9)
	movq	%r10, %rcx
	movq	%r11, %rsi
/APP
	movq  -296(%rbp),%rax     
	mulq  -40(%rbp)           
	addq  %rax,%rcx     
	adcq  %rdx,%rsi     
	adcq  $0,%r8        
	
/NO_APP
	movq	%rcx, 496(%r9)
	movl	(%r12), %ecx
	xorl	(%rbx), %ecx
	testq	%rsi, %rsi
	movq	%rsi, 504(%r9)
	movl	$64, 8(%r13)
	jne	.L149
	.align 16
.L157:
	movl	8(%r13), %edx
	leal	-1(%rdx), %ebx
	testl	%ebx, %ebx
	movl	%ebx, 8(%r13)
	je	.L149
	leal	-2(%rdx), %r12d
	cmpq	$0, (%r9,%r12,8)
	je	.L157
.L149:
	movl	8(%r13), %r9d
	xorl	%edx, %edx
	testl	%r9d, %r9d
	cmovne	%ecx, %edx
	movl	%edx, (%r13)
	addq	$520, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	leave
	ret
.LFE5:
	.size	s_mp_mul_comba_32, .-s_mp_mul_comba_32
	.align 16
.globl s_mp_sqr_comba_4
	.type	s_mp_sqr_comba_4, @function
s_mp_sqr_comba_4:
.LFB6:
	pushq	%rbp
.LCFI17:
	movq	%rsi, %r11
	xorl	%esi, %esi
	movq	%rsi, %r10
	movq	%rsi, %rbp
	movq	%rsi, %r8
	pushq	%rbx
.LCFI18:
	movq	%rsi, %rbx
	movq	16(%rdi), %rcx
	movq	%rsi, %rdi
/APP
	movq  (%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r10, -72(%rsp)
/APP
	movq  (%rcx),%rax     
	mulq  8(%rcx)           
	addq  %rax,%rbx     
	adcq  %rdx,%rdi     
	adcq  $0,%rbp        
	addq  %rax,%rbx     
	adcq  %rdx,%rdi     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%rbx, -64(%rsp)
/APP
	movq  (%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%rbp     
	adcq  $0,%r8        
	addq  %rax,%rdi     
	adcq  %rdx,%rbp     
	adcq  $0,%r8        
	
/NO_APP
	movq	%rbp, %rbx
	movq	%r8, %rbp
/APP
	movq  8(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rdi     
	adcq  %rdx,%rbx     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%rdi, -56(%rsp)
	movq	%rbp, %r9
	movq	%rbx, %r8
	movq	%rsi, %rdi
/APP
	movq  (%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rdi        
	addq  %rax,%r8     
	adcq  %rdx,%r9     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r9, %rbx
	movq	%rdi, %rbp
/APP
	movq  8(%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rbx     
	adcq  $0,%rbp        
	addq  %rax,%r8     
	adcq  %rdx,%rbx     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%r8, -48(%rsp)
	movq	%rbp, %r9
	movq	%rbx, %rdi
	movq	%rsi, %r8
	movl	$8, 8(%r11)
	movl	$0, (%r11)
/APP
	movq  8(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	addq  %rax,%rdi     
	adcq  %rdx,%r9     
	adcq  $0,%r8        
	
/NO_APP
	movq	%r9, %rbx
	movq	%r8, %rbp
/APP
	movq  16(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rdi     
	adcq  %rdx,%rbx     
	adcq  $0,%rbp        
	
/NO_APP
	movq	%rbp, %rax
	movq	%rdi, -40(%rsp)
	movq	%rbx, %rbp
	movq	%rax, %rdi
	movq	%rsi, %rbx
/APP
	movq  16(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%rbp     
	adcq  %rdx,%rdi     
	adcq  $0,%rbx        
	addq  %rax,%rbp     
	adcq  %rdx,%rdi     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%rbp, -32(%rsp)
	movq	%rbx, %r9
/APP
	movq  24(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rdi     
	adcq  %rdx,%r9     
	adcq  $0,%rsi        
	
/NO_APP
	movq	16(%r11), %rdx
	movq	%rdi, -24(%rsp)
	movq	%r9, -16(%rsp)
	movq	%r10, (%rdx)
	movq	-64(%rsp), %r8
	movq	%r8, 8(%rdx)
	movq	-56(%rsp), %rbp
	movq	%rbp, 16(%rdx)
	movq	-48(%rsp), %rdi
	movq	%rdi, 24(%rdx)
	movq	-40(%rsp), %rsi
	movq	%rsi, 32(%rdx)
	movq	-32(%rsp), %rbx
	movq	%rbx, 40(%rdx)
	movq	-24(%rsp), %rcx
	movq	%rcx, 48(%rdx)
	movq	-16(%rsp), %rax
	movq	%rax, 56(%rdx)
	movl	8(%r11), %edx
	testl	%edx, %edx
	je	.L168
	leal	-1(%rdx), %ecx
	movq	16(%r11), %rsi
	mov	%ecx, %r10d
	cmpq	$0, (%rsi,%r10,8)
	jne	.L166
	movl	%ecx, %edx
	.align 16
.L167:
	testl	%edx, %edx
	movl	%edx, %ecx
	je	.L171
	decl	%edx
	mov	%edx, %eax
	cmpq	$0, (%rsi,%rax,8)
	je	.L167
	movl	%ecx, 8(%r11)
	movl	%ecx, %edx
.L166:
	testl	%edx, %edx
	je	.L168
	popq	%rbx
	popq	%rbp
	movl	(%r11), %eax
	movl	%eax, (%r11)
	ret
.L171:
	movl	%edx, 8(%r11)
	.align 16
.L168:
	popq	%rbx
	popq	%rbp
	xorl	%eax, %eax
	movl	%eax, (%r11)
	ret
.LFE6:
	.size	s_mp_sqr_comba_4, .-s_mp_sqr_comba_4
	.align 16
.globl s_mp_sqr_comba_8
	.type	s_mp_sqr_comba_8, @function
s_mp_sqr_comba_8:
.LFB7:
	pushq	%r14
.LCFI19:
	xorl	%r9d, %r9d
	movq	%r9, %r14
	movq	%r9, %r10
	pushq	%r13
.LCFI20:
	movq	%r9, %r13
	pushq	%r12
.LCFI21:
	movq	%r9, %r12
	pushq	%rbp
.LCFI22:
	movq	%rsi, %rbp
	movq	%r9, %rsi
	pushq	%rbx
.LCFI23:
	movq	%r9, %rbx
	subq	$8, %rsp
.LCFI24:
	movq	16(%rdi), %rcx
/APP
	movq  (%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r14     
	adcq  %rdx,%rbx     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r14, -120(%rsp)
/APP
	movq  (%rcx),%rax     
	mulq  8(%rcx)           
	addq  %rax,%rbx     
	adcq  %rdx,%r12     
	adcq  $0,%r10        
	addq  %rax,%rbx     
	adcq  %rdx,%r12     
	adcq  $0,%r10        
	
/NO_APP
	movq	%rbx, -112(%rsp)
/APP
	movq  (%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%r12     
	adcq  %rdx,%r10     
	adcq  $0,%r13        
	addq  %rax,%r12     
	adcq  %rdx,%r10     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r10, %rbx
	movq	%r13, %r10
	movq	%r9, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r12     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r12, -104(%rsp)
	movq	%r10, %rdi
	movq	%rbx, %r11
/APP
	movq  (%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	addq  %rax,%r11     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %rbx
	movq	%rsi, %r10
	movq	%r9, %rdi
/APP
	movq  8(%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%r11     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	addq  %rax,%r11     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r9, %rsi
	movq	%r11, -96(%rsp)
	movq	%r10, %r8
	movq	%rbx, %r12
	movq	%r9, %r11
/APP
	movq  (%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r12     
	adcq  %rdx,%r8     
	adcq  $0,%r13        
	addq  %rax,%r12     
	adcq  %rdx,%r8     
	adcq  $0,%r13        
	
	movq  8(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r12     
	adcq  %rdx,%r8     
	adcq  $0,%r13        
	addq  %rax,%r12     
	adcq  %rdx,%r8     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r8, %rbx
	movq	%r13, %r10
	movq	%r9, %r8
/APP
	movq  16(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r12     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r12, -88(%rsp)
/APP
	movq  (%rcx),%rax     
	mulq  40(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r11         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r11         
	
/NO_APP
	movq	%rbx, -80(%rsp)
/APP
	movq  (%rcx),%rax     
	mulq  48(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
/APP
	movq  24(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -72(%rsp)
	movq	%r11, %r10
/APP
	movq  (%rcx),%rax     
	mulq  56(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
/APP
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%rax         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%rax         
	
/NO_APP
	movq	%rbx, -64(%rsp)
	movq	%rax, %r11
	movq	%r9, %rbx
/APP
	movq  8(%rcx),%rax     
	mulq  56(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	
/NO_APP
	movq	%rbx, %rsi
	movq	%r13, %rdi
	movq	%r11, %rbx
	movq	%r12, %r13
	movq	%rsi, %r11
/APP
	movq  32(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -56(%rsp)
	movq	%r9, %r10
/APP
	movq  16(%rcx),%rax     
	mulq  56(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %r13,%r13        
	
	movq  24(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%r13        
	
	movq  32(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%r13        
	
/NO_APP
	movq	%rdi, %r12
	movq	%r13, %rax
/APP
	addq %r8,%rbx         
	adcq %r12,%r11         
	adcq %rax,%r10         
	addq %r8,%rbx         
	adcq %r12,%r11         
	adcq %rax,%r10         
	
/NO_APP
	movq	%rbx, -48(%rsp)
	movq	%r11, %r12
	movq	%r10, %rsi
	movq	%r9, %rbx
	movq	%r9, %r11
/APP
	movq  24(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r12     
	adcq  %rdx,%rsi     
	adcq  $0,%rbx        
	addq  %rax,%r12     
	adcq  %rdx,%rsi     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%rbx, %r13
/APP
	movq  32(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r12     
	adcq  %rdx,%rsi     
	adcq  $0,%r13        
	addq  %rax,%r12     
	adcq  %rdx,%rsi     
	adcq  $0,%r13        
	
/NO_APP
	movq	%rsi, %r10
	movq	%r13, %rbx
	movq	%r9, %r13
/APP
	movq  40(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r12     
	adcq  %rdx,%r10     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r12, -40(%rsp)
	movq	%rbx, %r8
	movq	%r10, %rdi
/APP
	movq  32(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%r8     
	adcq  $0,%r11        
	addq  %rax,%rdi     
	adcq  %rdx,%r8     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r8, %r10
	movq	%r11, %rbx
/APP
	movq  40(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%r10     
	adcq  $0,%rbx        
	addq  %rax,%rdi     
	adcq  %rdx,%r10     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%rdi, -32(%rsp)
	movq	%rbx, %rsi
	movq	%r10, %r12
/APP
	movq  40(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r12     
	adcq  %rdx,%rsi     
	adcq  $0,%r13        
	addq  %rax,%r12     
	adcq  %rdx,%rsi     
	adcq  $0,%r13        
	
/NO_APP
	movq	%rsi, %r10
	movq	%r13, %rbx
/APP
	movq  48(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r12     
	adcq  %rdx,%r10     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r12, -24(%rsp)
	movq	%r10, %rdi
	movq	%rbx, %rsi
	movq	%r9, %r10
	movl	$16, 8(%rbp)
	movl	$0, (%rbp)
/APP
	movq  48(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r10        
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r10        
	
/NO_APP
	movq	%rdi, -16(%rsp)
	movq	%r10, %r8
/APP
	movq  56(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r9        
	
/NO_APP
	movq	16(%rbp), %rax
	movq	%rsi, -8(%rsp)
	movq	%r8, (%rsp)
	movq	%r14, (%rax)
	movq	-112(%rsp), %rbx
	movq	%rbx, 8(%rax)
	movq	-104(%rsp), %rcx
	movq	%rcx, 16(%rax)
	movq	-96(%rsp), %rdx
	movq	%rdx, 24(%rax)
	movq	-88(%rsp), %r14
	movq	%r14, 32(%rax)
	movq	-80(%rsp), %r13
	movq	%r13, 40(%rax)
	movq	-72(%rsp), %r12
	movq	%r12, 48(%rax)
	movq	-64(%rsp), %r11
	movq	%r11, 56(%rax)
	movq	-56(%rsp), %r10
	movq	%r10, 64(%rax)
	movq	-48(%rsp), %r9
	movq	%r9, 72(%rax)
	movq	-40(%rsp), %r8
	movq	%r8, 80(%rax)
	movq	-32(%rsp), %rdi
	movq	%rdi, 88(%rax)
	movq	-24(%rsp), %rsi
	movq	%rsi, 96(%rax)
	movq	-16(%rsp), %rbx
	movq	%rbx, 104(%rax)
	movq	-8(%rsp), %rcx
	movq	%rcx, 112(%rax)
	movq	(%rsp), %rdx
	movq	%rdx, 120(%rax)
	movl	8(%rbp), %edx
	testl	%edx, %edx
	je	.L192
	leal	-1(%rdx), %ecx
	movq	16(%rbp), %rsi
	mov	%ecx, %r14d
	cmpq	$0, (%rsi,%r14,8)
	jne	.L190
	movl	%ecx, %edx
	.align 16
.L191:
	testl	%edx, %edx
	movl	%edx, %ecx
	je	.L195
	decl	%edx
	mov	%edx, %r9d
	cmpq	$0, (%rsi,%r9,8)
	je	.L191
	movl	%ecx, 8(%rbp)
	movl	%ecx, %edx
.L190:
	testl	%edx, %edx
	je	.L192
	movl	(%rbp), %eax
	movl	%eax, (%rbp)
	addq	$8, %rsp
	popq	%rbx
	popq	%rbp
	popq	%r12
	popq	%r13
	popq	%r14
	ret
.L195:
	movl	%edx, 8(%rbp)
	.align 16
.L192:
	xorl	%eax, %eax
	movl	%eax, (%rbp)
	addq	$8, %rsp
	popq	%rbx
	popq	%rbp
	popq	%r12
	popq	%r13
	popq	%r14
	ret
.LFE7:
	.size	s_mp_sqr_comba_8, .-s_mp_sqr_comba_8
	.align 16
.globl s_mp_sqr_comba_16
	.type	s_mp_sqr_comba_16, @function
s_mp_sqr_comba_16:
.LFB8:
	pushq	%rbp
.LCFI25:
	xorl	%r9d, %r9d
	movq	%r9, %r8
	movq	%r9, %r11
	movq	%rsp, %rbp
.LCFI26:
	pushq	%r14
.LCFI27:
	movq	%rsi, %r14
	movq	%r9, %rsi
	pushq	%r13
.LCFI28:
	movq	%r9, %r13
	pushq	%r12
.LCFI29:
	movq	%r9, %r12
	pushq	%rbx
.LCFI30:
	movq	%r9, %rbx
	subq	$256, %rsp
.LCFI31:
	movq	16(%rdi), %rcx
/APP
	movq  (%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r8     
	adcq  %rdx,%rbx     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, -288(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  8(%rcx)           
	addq  %rax,%rbx     
	adcq  %rdx,%rsi     
	adcq  $0,%r12        
	addq  %rax,%rbx     
	adcq  %rdx,%rsi     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rbx, -280(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	addq  %rax,%rsi     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r12, %rbx
	movq	%r13, %r10
/APP
	movq  8(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rsi     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%rsi, -272(%rbp)
	movq	%r10, %rdi
	movq	%r9, %rsi
	movq	%rbx, %r10
/APP
	movq  (%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r10     
	adcq  %rdx,%rdi     
	adcq  $0,%r11        
	addq  %rax,%r10     
	adcq  %rdx,%rdi     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rdi, %r12
	movq	%r11, %rbx
	movq	%r9, %rdi
/APP
	movq  8(%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%rbx        
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r9, %r11
	movq	%r10, -264(%rbp)
	movq	%rbx, %r8
	movq	%r12, %r13
	movq	%r9, %r12
/APP
	movq  (%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
	movq  8(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, %rbx
	movq	%r12, %r10
	movq	%r9, %r8
/APP
	movq  16(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r13     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r13, -256(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  40(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r11         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r11         
	
/NO_APP
	movq	%rbx, -248(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  48(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
/APP
	movq  24(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -240(%rbp)
	movq	%r11, %r10
/APP
	movq  (%rcx),%rax     
	mulq  56(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rdx
/APP
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%rdx         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%rdx         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rbx, -232(%rbp)
	movq	%r9, %rbx
/APP
	movq  (%rcx),%rax     
	mulq  64(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	
	movq  32(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%r11     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r13, %rdi
	movq	%r10, -224(%rbp)
	movq	%r12, %rsi
	movq	%rbx, %r10
	movq	%r9, %r12
/APP
	movq  (%rcx),%rax     
	mulq  72(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r11         
	adcq %rdi,%r10         
	adcq %rsi,%r12         
	addq %r8,%r11         
	adcq %rdi,%r10         
	adcq %rsi,%r12         
	
/NO_APP
	movq	%r11, -216(%rbp)
	movq	%r12, %rbx
/APP
	movq  (%rcx),%rax     
	mulq  80(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%rbx         
	adcq %r12,%rax         
	addq %r8,%r10         
	adcq %r13,%rbx         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%rbx, %r11
	movq	%r13, %rdi
	movq	%rdx, %rbx
	movq	%r12, %rsi
/APP
	movq  40(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%r11     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r10, -208(%rbp)
	movq	%rbx, %r10
/APP
	movq  (%rcx),%rax     
	mulq  88(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rdx
/APP
	addq %r8,%r11         
	adcq %rdi,%r10         
	adcq %rsi,%rdx         
	addq %r8,%r11         
	adcq %rdi,%r10         
	adcq %rsi,%rdx         
	
/NO_APP
	movq	%rdx, %r13
	movq	%r11, -200(%rbp)
	movq	%r13, %r12
/APP
	movq  (%rcx),%rax     
	mulq  96(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %rdx
	movq	%rsi, %r11
/APP
	addq %r8,%r10         
	adcq %rdx,%r12         
	adcq %r11,%rax         
	addq %r8,%r10         
	adcq %rdx,%r12         
	adcq %r11,%rax         
	
/NO_APP
	movq	%rdx, %rbx
	movq	%rax, %r13
	movq	%r11, %rsi
/APP
	movq  48(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%rbx, %rdi
	movq	%r10, -192(%rbp)
	movq	%r13, %r10
/APP
	movq  (%rcx),%rax     
	mulq  104(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r9, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r12         
	adcq %rdi,%r10         
	adcq %rsi,%r13         
	addq %r8,%r12         
	adcq %rdi,%r10         
	adcq %rsi,%r13         
	
/NO_APP
	movq	%r12, -184(%rbp)
	movq	%r13, %r12
/APP
	movq  (%rcx),%rax     
	mulq  112(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %rbx
	movq	%rsi, %rdx
/APP
	addq %r8,%r10         
	adcq %rbx,%r12         
	adcq %rdx,%rax         
	addq %r8,%r10         
	adcq %rbx,%r12         
	adcq %rdx,%rax         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r13
	movq	%rbx, %rdi
/APP
	movq  56(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r10, -176(%rbp)
	movq	%r13, %r10
/APP
	movq  (%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r9, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r12         
	adcq %rdi,%r10         
	adcq %rsi,%r13         
	addq %r8,%r12         
	adcq %rdi,%r10         
	adcq %rsi,%r13         
	
/NO_APP
	movq	%r12, -168(%rbp)
	movq	%r13, %r12
/APP
	movq  8(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %rbx
	movq	%rsi, %rdx
/APP
	addq %r8,%r10         
	adcq %rbx,%r12         
	adcq %rdx,%rax         
	addq %r8,%r10         
	adcq %rbx,%r12         
	adcq %rdx,%rax         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r13
	movq	%rbx, %rdi
/APP
	movq  64(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r10, -160(%rbp)
	movq	%r9, %r11
/APP
	movq  16(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r13, %r10
	movq	%r9, %rbx
/APP
	movq  24(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r12         
	adcq %rdi,%r10         
	adcq %rsi,%r11         
	addq %r8,%r12         
	adcq %rdi,%r10         
	adcq %rsi,%r11         
	
/NO_APP
	movq	%r12, -152(%rbp)
/APP
	movq  24(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	
/NO_APP
	movq	%rbx, %rdx
	movq	%r13, %rdi
	movq	%r11, %rbx
	movq	%r12, %rsi
	movq	%rdx, %r11
	movq	%r9, %r12
/APP
	movq  72(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -144(%rbp)
	movq	%r11, %r10
/APP
	movq  32(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r12         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r12         
	
/NO_APP
	movq	%rbx, -136(%rbp)
	movq	%r12, %r11
/APP
	movq  40(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
/APP
	movq  80(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -128(%rbp)
	movq	%r11, %r10
/APP
	movq  48(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rdx
/APP
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%rdx         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%rdx         
	
/NO_APP
	movq	%rbx, -120(%rbp)
	movq	%rdx, %r11
	movq	%r9, %rbx
/APP
	movq  56(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	
/NO_APP
	movq	%rbx, %rdx
	movq	%r13, %rdi
	movq	%r11, %rbx
	movq	%r12, %rsi
	movq	%rdx, %r11
	movq	%r9, %r12
/APP
	movq  88(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -112(%rbp)
	movq	%r11, %r10
/APP
	movq  64(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  88(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r12         
	addq %r8,%rbx         
	adcq %rdi,%r10         
	adcq %rsi,%r12         
	
/NO_APP
	movq	%rbx, -104(%rbp)
	movq	%r12, %r11
/APP
	movq  72(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  88(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r9, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r10         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
/APP
	movq  96(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r10     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r10, -96(%rbp)
	movq	%r9, %r10
/APP
	movq  80(%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  88(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  96(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r12
	movq	%rsi, %rax
	movq	%r9, %rsi
/APP
	addq %r8,%rbx         
	adcq %r12,%r11         
	adcq %rax,%r10         
	addq %r8,%rbx         
	adcq %r12,%r11         
	adcq %rax,%r10         
	
/NO_APP
	movq	%r9, %r12
	movq	%rbx, -88(%rbp)
	movq	%r11, %r13
	movq	%r10, %r11
/APP
	movq  88(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%r13     
	adcq  %rdx,%r11     
	adcq  $0,%r12        
	addq  %rax,%r13     
	adcq  %rdx,%r11     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r12, %rdi
/APP
	movq  96(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r13     
	adcq  %rdx,%r11     
	adcq  $0,%rdi        
	addq  %rax,%r13     
	adcq  %rdx,%r11     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r11, %rbx
	movq	%rdi, %r10
	movq	%r9, %r11
/APP
	movq  104(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r13     
	adcq  %rdx,%rbx     
	adcq  $0,%r10        
	
/NO_APP
	movq	%r13, -80(%rbp)
	movq	%r10, %r8
	movq	%rbx, %r10
/APP
	movq  96(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%r10     
	adcq  %rdx,%r8     
	adcq  $0,%rsi        
	addq  %rax,%r10     
	adcq  %rdx,%r8     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %r12
	movq	%rsi, %rbx
/APP
	movq  104(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%rbx        
	addq  %rax,%r10     
	adcq  %rdx,%r12     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r10, -72(%rbp)
	movq	%rbx, %r13
	movq	%r12, %rbx
/APP
	movq  104(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rbx     
	adcq  %rdx,%r13     
	adcq  $0,%r11        
	addq  %rax,%rbx     
	adcq  %rdx,%r13     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r11, %r12
	movq	%r13, %r10
/APP
	movq  112(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rbx     
	adcq  %rdx,%r10     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rbx, -64(%rbp)
	movq	%r10, %rdi
	movq	%r9, %rbx
	movq	%r12, %rsi
/APP
	movq  112(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rbx        
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%rdi, -56(%rbp)
	movq	%rbx, %r8
/APP
	movq  120(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r9        
	
/NO_APP
	movq	%rsi, -48(%rbp)
	movq	16(%r14), %rdi
	leaq	-288(%rbp), %rsi
	movl	$256, %edx
	movq	%r8, -40(%rbp)
	movl	$32, 8(%r14)
	movl	$0, (%r14)
	call	memcpy@PLT
	movl	8(%r14), %edx
	testl	%edx, %edx
	je	.L232
	leal	-1(%rdx), %ecx
	movq	16(%r14), %rsi
	mov	%ecx, %r9d
	cmpq	$0, (%rsi,%r9,8)
	jne	.L230
	movl	%ecx, %edx
	.align 16
.L231:
	testl	%edx, %edx
	movl	%edx, %ecx
	je	.L235
	decl	%edx
	mov	%edx, %eax
	cmpq	$0, (%rsi,%rax,8)
	je	.L231
	movl	%ecx, 8(%r14)
	movl	%ecx, %edx
.L230:
	testl	%edx, %edx
	je	.L232
	movl	(%r14), %eax
	movl	%eax, (%r14)
	addq	$256, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	leave
	ret
.L235:
	movl	%edx, 8(%r14)
	.align 16
.L232:
	xorl	%eax, %eax
	movl	%eax, (%r14)
	addq	$256, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	leave
	ret
.LFE8:
	.size	s_mp_sqr_comba_16, .-s_mp_sqr_comba_16
	.align 16
.globl s_mp_sqr_comba_32
	.type	s_mp_sqr_comba_32, @function
s_mp_sqr_comba_32:
.LFB9:
	pushq	%rbp
.LCFI32:
	xorl	%r10d, %r10d
	movq	%r10, %r8
	movq	%r10, %r11
	movq	%rsp, %rbp
.LCFI33:
	pushq	%r14
.LCFI34:
	movq	%rsi, %r14
	movq	%r10, %rsi
	pushq	%r13
.LCFI35:
	movq	%r10, %r13
	pushq	%r12
.LCFI36:
	movq	%r10, %r12
	pushq	%rbx
.LCFI37:
	movq	%r10, %rbx
	subq	$512, %rsp
.LCFI38:
	movq	16(%rdi), %rcx
/APP
	movq  (%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r8     
	adcq  %rdx,%rbx     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, -544(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  8(%rcx)           
	addq  %rax,%rbx     
	adcq  %rdx,%rsi     
	adcq  $0,%r12        
	addq  %rax,%rbx     
	adcq  %rdx,%rsi     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rbx, -536(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	addq  %rax,%rsi     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r12, %rbx
	movq	%r13, %r9
/APP
	movq  8(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rsi     
	adcq  %rdx,%rbx     
	adcq  $0,%r9        
	
/NO_APP
	movq	%rsi, -528(%rbp)
	movq	%r9, %rdi
	movq	%r10, %rsi
	movq	%rbx, %r9
/APP
	movq  (%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r9     
	adcq  %rdx,%rdi     
	adcq  $0,%r11        
	addq  %rax,%r9     
	adcq  %rdx,%rdi     
	adcq  $0,%r11        
	
/NO_APP
	movq	%rdi, %r12
	movq	%r11, %r13
	movq	%r10, %rdi
/APP
	movq  8(%rcx),%rax     
	mulq  16(%rcx)           
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r10, %r11
	movq	%r9, -520(%rbp)
	movq	%r13, %r8
	movq	%r12, %r13
	movq	%r10, %r12
/APP
	movq  (%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
	movq  8(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	addq  %rax,%r13     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r8, %rbx
	movq	%r12, %r9
	movq	%r10, %r8
/APP
	movq  16(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r13     
	adcq  %rdx,%rbx     
	adcq  $0,%r9        
	
/NO_APP
	movq	%r13, -512(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  40(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  24(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%r11         
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%r11         
	
/NO_APP
	movq	%rbx, -504(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  48(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
/APP
	movq  24(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r9, -496(%rbp)
	movq	%r11, %r9
/APP
	movq  (%rcx),%rax     
	mulq  56(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  32(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rdx
/APP
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%rdx         
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%rdx         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rbx, -488(%rbp)
	movq	%r10, %rbx
/APP
	movq  (%rcx),%rax     
	mulq  64(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	
	movq  32(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r11     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r13, %rdi
	movq	%r9, -480(%rbp)
	movq	%r12, %rsi
	movq	%rbx, %r9
	movq	%r10, %r12
/APP
	movq  (%rcx),%rax     
	mulq  72(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  40(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r11         
	adcq %rdi,%r9         
	adcq %rsi,%r12         
	addq %r8,%r11         
	adcq %rdi,%r9         
	adcq %rsi,%r12         
	
/NO_APP
	movq	%r11, -472(%rbp)
	movq	%r12, %rbx
/APP
	movq  (%rcx),%rax     
	mulq  80(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%rbx         
	adcq %r12,%rax         
	addq %r8,%r9         
	adcq %r13,%rbx         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%rbx, %r11
	movq	%r13, %rdi
	movq	%rdx, %rbx
	movq	%r12, %rsi
/APP
	movq  40(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r11     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r9, -464(%rbp)
	movq	%rbx, %r9
/APP
	movq  (%rcx),%rax     
	mulq  88(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  48(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rdx
/APP
	addq %r8,%r11         
	adcq %rdi,%r9         
	adcq %rsi,%rdx         
	addq %r8,%r11         
	adcq %rdi,%r9         
	adcq %rsi,%rdx         
	
/NO_APP
	movq	%rdx, %r13
	movq	%r11, -456(%rbp)
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  96(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %rax
	movq	%rsi, %r11
/APP
	addq %r8,%r9         
	adcq %rax,%r12         
	adcq %r11,%r13         
	addq %r8,%r9         
	adcq %rax,%r12         
	adcq %r11,%r13         
	
/NO_APP
	movq	%rax, %rbx
	movq	%r11, %rsi
/APP
	movq  48(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%rbx, %rdi
	movq	%r9, -448(%rbp)
	movq	%r13, %r9
/APP
	movq  (%rcx),%rax     
	mulq  104(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  56(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r12         
	adcq %rdi,%r9         
	adcq %rsi,%r13         
	addq %r8,%r12         
	adcq %rdi,%r9         
	adcq %rsi,%r13         
	
/NO_APP
	movq	%r12, -440(%rbp)
	movq	%r10, %r12
/APP
	movq  (%rcx),%rax     
	mulq  112(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r13, %rdx
	movq	%rdi, %rbx
	movq	%rsi, %r13
/APP
	addq %r8,%r9         
	adcq %rbx,%rdx         
	adcq %r13,%r12         
	addq %r8,%r9         
	adcq %rbx,%rdx         
	adcq %r13,%r12         
	
/NO_APP
	movq	%r12, %rax
	movq	%r13, %r11
	movq	%rdx, %r12
	movq	%rax, %r13
	movq	%rbx, %rdi
	movq	%r11, %rsi
/APP
	movq  56(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r9, -432(%rbp)
	movq	%r13, %r9
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  120(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  8(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  64(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rax
	movq	%rdi, %rdx
	movq	%rsi, %rbx
/APP
	addq %rax,%r12         
	adcq %rdx,%r9         
	adcq %rbx,%r13         
	addq %rax,%r12         
	adcq %rdx,%r9         
	adcq %rbx,%r13         
	
/NO_APP
	movq	%r12, -424(%rbp)
	movq	%rdx, %r8
	movq	%rax, %rsi
	movq	%rbx, %rdi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  128(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  64(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -416(%rbp)
	movq	%r13, %r9
/APP
	movq  (%rcx),%rax     
	mulq  136(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  72(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%r12, -408(%rbp)
	movq	%rdx, %rdi
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  144(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  72(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -400(%rbp)
	movq	%r13, %r9
/APP
	movq  (%rcx),%rax     
	mulq  152(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  80(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%r12, -392(%rbp)
	movq	%rdx, %rdi
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  160(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  80(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -384(%rbp)
	movq	%r13, %r9
/APP
	movq  (%rcx),%rax     
	mulq  168(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  88(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%r12, -376(%rbp)
	movq	%rdx, %rdi
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  176(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  88(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -368(%rbp)
	movq	%r13, %r9
/APP
	movq  (%rcx),%rax     
	mulq  184(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  16(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  24(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  32(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  40(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  48(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  56(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  64(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  88(%rcx),%rax     
	mulq  96(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %rdi
	movq	%r12, -360(%rbp)
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  192(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
/APP
	addq %rsi,%r9         
	adcq %rbx,%r12         
	adcq %rax,%r13         
	addq %rsi,%r9         
	adcq %rbx,%r12         
	adcq %rax,%r13         
	
/NO_APP
	movq	%rax, %r11
	movq	%rbx, %r8
/APP
	movq  96(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rdi
	movq	%r9, -352(%rbp)
	movq	%r13, %r9
/APP
	movq  (%rcx),%rax     
	mulq  200(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  104(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	
/NO_APP
	movq	%r12, -344(%rbp)
	movq	%r10, %r12
/APP
	movq  (%rcx),%rax     
	mulq  208(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r13, %rdx
	movq	%r8, %rbx
	movq	%rdi, %r13
/APP
	addq %rsi,%r9         
	adcq %rbx,%rdx         
	adcq %r13,%r12         
	addq %rsi,%r9         
	adcq %rbx,%rdx         
	adcq %r13,%r12         
	
/NO_APP
	movq	%r12, %rax
	movq	%r13, %r11
	movq	%rdx, %r12
	movq	%rax, %r13
	movq	%rbx, %r8
	movq	%r11, %rdi
/APP
	movq  104(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r9, -336(%rbp)
	movq	%r13, %r9
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  216(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  112(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	
/NO_APP
	movq	%r12, -328(%rbp)
/APP
	movq  (%rcx),%rax     
	mulq  224(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r13, %rax
	movq	%r10, %rdx
	movq	%r8, %rbx
	movq	%rdi, %r12
/APP
	addq %rsi,%r9         
	adcq %rbx,%rax         
	adcq %r12,%rdx         
	addq %rsi,%r9         
	adcq %rbx,%rax         
	adcq %r12,%rdx         
	
/NO_APP
	movq	%rdx, %rdi
	movq	%r12, %r11
	movq	%rbx, %r8
	movq	%rax, %r12
	movq	%rdi, %r13
	movq	%r11, %rdi
/APP
	movq  112(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r9, -320(%rbp)
	movq	%r13, %rbx
	movq	%r10, %r9
/APP
	movq  (%rcx),%rax     
	mulq  232(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  120(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%rbx         
	adcq %rdi,%r9         
	addq %rsi,%r12         
	adcq %r8,%rbx         
	adcq %rdi,%r9         
	
/NO_APP
	movq	%r12, -312(%rbp)
	movq	%r9, %r13
/APP
	movq  (%rcx),%rax     
	mulq  240(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r10, %rax
	movq	%r8, %r11
	movq	%rdi, %rdx
/APP
	addq %rsi,%rbx         
	adcq %r11,%r13         
	adcq %rdx,%rax         
	addq %rsi,%rbx         
	adcq %r11,%r13         
	adcq %rdx,%rax         
	
/NO_APP
	movq	%rdx, %r9
	movq	%rax, %rdx
	movq	%r13, %r12
	movq	%r11, %r8
	movq	%rdx, %r13
	movq	%r9, %rdi
/APP
	movq  120(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rbx     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%rbx, -304(%rbp)
	movq	%r13, %rbx
	movq	%r10, %r13
/APP
	movq  (%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  8(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  128(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%rbx         
	adcq %rdi,%r13         
	addq %rsi,%r12         
	adcq %r8,%rbx         
	adcq %rdi,%r13         
	
/NO_APP
	movq	%r12, -296(%rbp)
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  8(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  16(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  24(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r8, %r11
	movq	%rdi, %rax
/APP
	addq %rsi,%rbx         
	adcq %r11,%r12         
	adcq %rax,%r13         
	addq %rsi,%rbx         
	adcq %r11,%r12         
	adcq %rax,%r13         
	
/NO_APP
	movq	%rax, %r9
	movq	%r11, %r8
/APP
	movq  128(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rbx     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r9, %rdi
	movq	%rbx, -288(%rbp)
	movq	%r13, %r9
/APP
	movq  16(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  24(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  136(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	
/NO_APP
	movq	%r12, -280(%rbp)
	movq	%r10, %r12
/APP
	movq  24(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  32(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r13, %rdx
	movq	%r8, %rbx
	movq	%rdi, %r13
/APP
	addq %rsi,%r9         
	adcq %rbx,%rdx         
	adcq %r13,%r12         
	addq %rsi,%r9         
	adcq %rbx,%rdx         
	adcq %r13,%r12         
	
/NO_APP
	movq	%r12, %rax
	movq	%r13, %r11
	movq	%rdx, %r12
	movq	%rax, %r13
	movq	%rbx, %r8
	movq	%r11, %rdi
/APP
	movq  136(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r9, -272(%rbp)
	movq	%r13, %r9
	movq	%r10, %r13
/APP
	movq  32(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  40(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  144(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r13         
	
/NO_APP
	movq	%r12, -264(%rbp)
/APP
	movq  40(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  48(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r13, %rax
	movq	%r10, %rdx
	movq	%r8, %rbx
	movq	%rdi, %r12
/APP
	addq %rsi,%r9         
	adcq %rbx,%rax         
	adcq %r12,%rdx         
	addq %rsi,%r9         
	adcq %rbx,%rax         
	adcq %r12,%rdx         
	
/NO_APP
	movq	%rdx, %rdi
	movq	%r12, %r11
	movq	%rbx, %r8
	movq	%rax, %r12
	movq	%rdi, %r13
	movq	%r11, %rdi
/APP
	movq  144(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r10, %r11
	movq	%r9, -256(%rbp)
	movq	%r13, %r9
/APP
	movq  48(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  56(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  144(%rcx),%rax     
	mulq  152(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r11         
	addq %rsi,%r12         
	adcq %r8,%r9         
	adcq %rdi,%r11         
	
/NO_APP
	movq	%r12, -248(%rbp)
	movq	%r11, %r13
/APP
	movq  56(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  64(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  72(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  144(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%r10, %rax
	movq	%rsi, %rdx
	movq	%r8, %rbx
	movq	%rdi, %r12
/APP
	addq %rdx,%r9         
	adcq %rbx,%r13         
	adcq %r12,%rax         
	addq %rdx,%r9         
	adcq %rbx,%r13         
	adcq %r12,%rax         
	
/NO_APP
	movq	%r12, %r11
	movq	%rdx, %r8
	movq	%rax, %rdx
	movq	%r13, %r12
	movq	%rbx, %rdi
	movq	%rdx, %r13
	movq	%r11, %rsi
/APP
	movq  152(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r9, -240(%rbp)
	movq	%r13, %r9
	movq	%r10, %r13
/APP
	movq  64(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  72(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  80(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  88(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  96(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  104(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  112(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  120(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  128(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  136(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  144(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  160(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rax
	movq	%rdi, %rdx
	movq	%rsi, %rbx
/APP
	addq %rax,%r12         
	adcq %rdx,%r9         
	adcq %rbx,%r13         
	addq %rax,%r12         
	adcq %rdx,%r9         
	adcq %rbx,%r13         
	
/NO_APP
	movq	%r12, -232(%rbp)
	movq	%rdx, %r8
	movq	%rax, %rsi
	movq	%rbx, %rdi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  72(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  80(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  88(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  144(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  152(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  160(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -224(%rbp)
	movq	%r13, %r9
/APP
	movq  80(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  88(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  96(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  104(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  112(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  120(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  128(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  136(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  144(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  168(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%r12, -216(%rbp)
	movq	%rdx, %rdi
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  88(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  96(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  104(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  144(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  152(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  160(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  168(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -208(%rbp)
	movq	%r13, %r9
/APP
	movq  96(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  104(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  112(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  120(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  128(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  136(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  144(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  176(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%r12, -200(%rbp)
	movq	%rdx, %rdi
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  104(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  112(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  120(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  144(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  152(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  160(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  168(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  176(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -192(%rbp)
	movq	%r13, %r9
/APP
	movq  112(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  120(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  128(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  136(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  144(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  184(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r8, %rbx
	movq	%rdi, %rax
	movq	%rsi, %rdx
/APP
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	addq %rbx,%r12         
	adcq %rax,%r9         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%r12, -184(%rbp)
	movq	%rdx, %rdi
	movq	%rax, %r8
	movq	%rbx, %rsi
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  120(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%rsi     
	movq  %rdx,%r8     
	xorq  %rdi,%rdi        
	
	movq  128(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  136(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  144(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  152(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  160(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  168(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
	movq  176(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%rdi        
	
/NO_APP
	movq	%rsi, %rax
	movq	%r8, %rbx
	movq	%rdi, %rdx
/APP
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	addq %rax,%r9         
	adcq %rbx,%r12         
	adcq %rdx,%r13         
	
/NO_APP
	movq	%rdx, %r11
	movq	%rax, %r8
	movq	%rbx, %rdi
/APP
	movq  184(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -176(%rbp)
	movq	%r13, %r9
/APP
	movq  128(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
/NO_APP
	movq	%r10, %r13
/APP
	movq  136(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  144(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  192(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r12         
	adcq %rdi,%r9         
	adcq %rsi,%r13         
	addq %r8,%r12         
	adcq %rdi,%r9         
	adcq %rsi,%r13         
	
/NO_APP
	movq	%r12, -168(%rbp)
	movq	%r13, %r12
	movq	%r10, %r13
/APP
	movq  136(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  144(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %rbx
	movq	%rsi, %rax
/APP
	addq %r8,%r9         
	adcq %rbx,%r12         
	adcq %rax,%r13         
	addq %r8,%r9         
	adcq %rbx,%r12         
	adcq %rax,%r13         
	
/NO_APP
	movq	%rax, %r11
	movq	%rbx, %rdi
	movq	%r10, %rbx
/APP
	movq  192(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r11, %rsi
	movq	%r9, -160(%rbp)
	movq	%r13, %r9
/APP
	movq  144(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  152(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  192(%rcx),%rax     
	mulq  200(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%r12         
	adcq %rdi,%r9         
	adcq %rsi,%rbx         
	addq %r8,%r12         
	adcq %rdi,%r9         
	adcq %rsi,%rbx         
	
/NO_APP
	movq	%r12, -152(%rbp)
/APP
	movq  152(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  160(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  192(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rdx
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%rbx         
	adcq %r12,%rdx         
	addq %r8,%r9         
	adcq %r13,%rbx         
	adcq %r12,%rdx         
	
/NO_APP
	movq	%rdx, %rax
	movq	%r13, %rdi
	movq	%r12, %rsi
	movq	%rax, %r11
	movq	%r10, %r12
/APP
	movq  200(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r9, -144(%rbp)
	movq	%r11, %r9
/APP
	movq  160(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  168(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  192(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  200(%rcx),%rax     
	mulq  208(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%r12         
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%r12         
	
/NO_APP
	movq	%rbx, -136(%rbp)
	movq	%r12, %r11
/APP
	movq  168(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  176(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  192(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  200(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
/APP
	movq  208(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r9, -128(%rbp)
	movq	%r11, %r9
/APP
	movq  176(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  184(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  192(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  200(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  208(%rcx),%rax     
	mulq  216(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rdx
/APP
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%rdx         
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%rdx         
	
/NO_APP
	movq	%rbx, -120(%rbp)
	movq	%rdx, %r11
	movq	%r10, %rbx
/APP
	movq  184(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  192(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  200(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  208(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rbx         
	
/NO_APP
	movq	%rbx, %rdx
	movq	%r13, %rdi
	movq	%r11, %rbx
	movq	%r12, %rsi
	movq	%rdx, %r11
	movq	%r10, %r12
/APP
	movq  216(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r9, -112(%rbp)
	movq	%r11, %r9
/APP
	movq  192(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  200(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  208(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  216(%rcx),%rax     
	mulq  224(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%r12         
	addq %r8,%rbx         
	adcq %rdi,%r9         
	adcq %rsi,%r12         
	
/NO_APP
	movq	%rbx, -104(%rbp)
	movq	%r12, %r11
/APP
	movq  200(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  208(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  216(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%r10, %rax
	movq	%rdi, %r13
	movq	%rsi, %r12
/APP
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rax         
	addq %r8,%r9         
	adcq %r13,%r11         
	adcq %r12,%rax         
	
/NO_APP
	movq	%rax, %rdx
	movq	%r11, %rbx
	movq	%r13, %rdi
	movq	%rdx, %r11
	movq	%r12, %rsi
	movq	%r10, %r12
/APP
	movq  224(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r9     
	adcq  %rdx,%rbx     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r9, -96(%rbp)
	movq	%r10, %r9
/APP
	movq  208(%rcx),%rax     
	mulq  248(%rcx)           
	movq  %rax,%r8     
	movq  %rdx,%rdi     
	xorq  %rsi,%rsi        
	
	movq  216(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
	movq  224(%rcx),%rax     
	mulq  232(%rcx)           
	addq  %rax,%r8     
	adcq  %rdx,%rdi     
	adcq  $0,%rsi        
	
/NO_APP
	movq	%rdi, %r13
	movq	%rsi, %rax
/APP
	addq %r8,%rbx         
	adcq %r13,%r11         
	adcq %rax,%r9         
	addq %r8,%rbx         
	adcq %r13,%r11         
	adcq %rax,%r9         
	
/NO_APP
	movq	%rbx, -88(%rbp)
	movq	%r11, %rsi
	movq	%r9, %r8
/APP
	movq  216(%rcx),%rax     
	mulq  248(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r12        
	
/NO_APP
	movq	%r12, %r11
/APP
	movq  224(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r11        
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r11        
	
/NO_APP
	movq	%r8, %r13
	movq	%r11, %rbx
/APP
	movq  232(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rsi     
	adcq  %rdx,%r13     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%rsi, -80(%rbp)
	movq	%rbx, %r12
	movq	%r13, %rdi
	movq	%r10, %r13
/APP
	movq  224(%rcx),%rax     
	mulq  248(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	addq  %rax,%rdi     
	adcq  %rdx,%r12     
	adcq  $0,%r13        
	
/NO_APP
	movq	%r12, %r9
	movq	%r13, %r12
/APP
	movq  232(%rcx),%rax     
	mulq  240(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%r9     
	adcq  $0,%r12        
	addq  %rax,%rdi     
	adcq  %rdx,%r9     
	adcq  $0,%r12        
	
/NO_APP
	movq	%rdi, -72(%rbp)
	movq	%r9, %r11
	movq	%r12, %rbx
	movq	%r10, %r9
/APP
	movq  232(%rcx),%rax     
	mulq  248(%rcx)           
	addq  %rax,%r11     
	adcq  %rdx,%rbx     
	adcq  $0,%r9        
	addq  %rax,%r11     
	adcq  %rdx,%rbx     
	adcq  $0,%r9        
	
/NO_APP
	movq	%rbx, %r13
	movq	%r9, %rbx
	movq	%r10, %r9
/APP
	movq  240(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%r11     
	adcq  %rdx,%r13     
	adcq  $0,%rbx        
	
/NO_APP
	movq	%r11, -64(%rbp)
	movq	%r13, %rdi
	movq	%rbx, %rsi
/APP
	movq  240(%rcx),%rax     
	mulq  248(%rcx)           
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r9        
	addq  %rax,%rdi     
	adcq  %rdx,%rsi     
	adcq  $0,%r9        
	
/NO_APP
	movq	%rdi, -56(%rbp)
	movq	%r9, %r8
/APP
	movq  248(%rcx),%rax     
	mulq  %rax        
	addq  %rax,%rsi     
	adcq  %rdx,%r8     
	adcq  $0,%r10        
	
/NO_APP
	movq	%rsi, -48(%rbp)
	movq	16(%r14), %rdi
	leaq	-544(%rbp), %rsi
	movl	$512, %edx
	movq	%r8, -40(%rbp)
	movl	$64, 8(%r14)
	movl	$0, (%r14)
	call	memcpy@PLT
	movl	8(%r14), %edx
	testl	%edx, %edx
	je	.L304
	leal	-1(%rdx), %ecx
	movq	16(%r14), %rsi
	mov	%ecx, %r10d
	cmpq	$0, (%rsi,%r10,8)
	jne	.L302
	movl	%ecx, %edx
	.align 16
.L303:
	testl	%edx, %edx
	movl	%edx, %ecx
	je	.L307
	decl	%edx
	mov	%edx, %eax
	cmpq	$0, (%rsi,%rax,8)
	je	.L303
	movl	%ecx, 8(%r14)
	movl	%ecx, %edx
.L302:
	testl	%edx, %edx
	je	.L304
	movl	(%r14), %eax
	movl	%eax, (%r14)
	addq	$512, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	leave
	ret
.L307:
	movl	%edx, 8(%r14)
	.align 16
.L304:
	xorl	%eax, %eax
	movl	%eax, (%r14)
	addq	$512, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	leave
	ret
.LFE9:
	.size	s_mp_sqr_comba_32, .-s_mp_sqr_comba_32
