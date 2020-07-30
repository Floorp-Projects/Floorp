/*
   The lucet_context_swap function is taken from Xudong Huang's
   generator-rs project. Its MIT license is provided below.

   Copyright (c) 2017 Xudong Huang

   Permission is hereby granted, free of charge, to any
   person obtaining a copy of this software and associated
   documentation files (the "Software"), to deal in the
   Software without restriction, including without
   limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of
   the Software, and to permit persons to whom the Software
   is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice
   shall be included in all copies or substantial portions
   of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
   ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
   TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
   PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
   SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
   IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.

*/

.text
.globl lucet_context_bootstrap
#ifdef __ELF__
.type lucet_context_bootstrap,@function
#else
.globl _lucet_context_bootstrap
#endif
.align 16
lucet_context_bootstrap:
_lucet_context_bootstrap:
	/* Move each of the context-saved registers into the corresponding call
	 * argument register. See lucet_register enum for docs  */
	mov %r12, %rsi
	mov %r13, %rdx
	mov %r14, %rcx
	mov %r15, %r8
	mov %rbx, %r9
	/* the next thing on the stack is the guest function - return to it */
	ret
#ifdef __ELF__
.size lucet_context_bootstrap,.-lucet_context_bootstrap
#endif

.text
.globl lucet_context_backstop
#ifdef __ELF__
.type lucet_context_backstop,@function
#else
.globl _lucet_context_backstop
#endif
.align 16
lucet_context_backstop:
_lucet_context_backstop:
    mov -16(%rbp), %rdi  /* parent context to arg 1 */
    mov -8(%rbp), %rsi  /* own context to arg 2 */
    mov %rax, (8*8 + 8*16 + 8*0)(%rdi) /* store return values before swapping back -- offset is offsetof(struct lucet_context, retvals) */
    mov %rdx, (8*8 + 8*16 + 8*1)(%rdi)
    movdqu %xmm0, (8*8 + 8*16 + 8*2)(%rdi) /* floating-point return value */
#ifdef __ELF__
    jmp lucet_context_swap@PLT
#else
    jmp lucet_context_swap
#endif
#ifdef __ELF__
.size lucet_context_backstop,.-lucet_context_backstop
#endif

.text
.globl lucet_context_swap
#ifdef __ELF__
.type lucet_context_swap,@function
#else
.globl _lucet_context_swap
#endif
.align 16
lucet_context_swap:
_lucet_context_swap:
    // store everything in offsets from rdi (1st arg)
    mov %rbx, (0*8)(%rdi)
    mov %rsp, (1*8)(%rdi)
    mov %rbp, (2*8)(%rdi)
    mov %rdi, (3*8)(%rdi)
    mov %r12, (4*8)(%rdi)
    mov %r13, (5*8)(%rdi)
    mov %r14, (6*8)(%rdi)
    mov %r15, (7*8)(%rdi)

    movdqu %xmm0, (8*8 + 0*16)(%rdi)
    movdqu %xmm1, (8*8 + 1*16)(%rdi)
    movdqu %xmm2, (8*8 + 2*16)(%rdi)
    movdqu %xmm3, (8*8 + 3*16)(%rdi)
    movdqu %xmm4, (8*8 + 4*16)(%rdi)
    movdqu %xmm5, (8*8 + 5*16)(%rdi)
    movdqu %xmm6, (8*8 + 6*16)(%rdi)
    movdqu %xmm7, (8*8 + 7*16)(%rdi)

    // load everything from offsets from rsi (2nd arg)
    mov (0*8)(%rsi), %rbx
    mov (1*8)(%rsi), %rsp
    mov (2*8)(%rsi), %rbp
    mov (3*8)(%rsi), %rdi
    mov (4*8)(%rsi), %r12
    mov (5*8)(%rsi), %r13
    mov (6*8)(%rsi), %r14
    mov (7*8)(%rsi), %r15

    movdqu (8*8 + 0*16)(%rsi), %xmm0
    movdqu (8*8 + 1*16)(%rsi), %xmm1
    movdqu (8*8 + 2*16)(%rsi), %xmm2
    movdqu (8*8 + 3*16)(%rsi), %xmm3
    movdqu (8*8 + 4*16)(%rsi), %xmm4
    movdqu (8*8 + 5*16)(%rsi), %xmm5
    movdqu (8*8 + 6*16)(%rsi), %xmm6
    movdqu (8*8 + 7*16)(%rsi), %xmm7

    ret
#ifdef __ELF__
.size lucet_context_swap,.-lucet_context_swap
#endif

.text
.globl lucet_context_set
#ifdef __ELF__
.type lucet_context_set,@function
#else
.globl _lucet_context_set
#endif
.align 16
lucet_context_set:
_lucet_context_set:
    // load everything from offsets from rdi (1st arg)
    mov (0*8)(%rdi), %rbx
    mov (1*8)(%rdi), %rsp
    mov (2*8)(%rdi), %rbp
    mov (4*8)(%rdi), %r12
    mov (5*8)(%rdi), %r13
    mov (6*8)(%rdi), %r14
    mov (7*8)(%rdi), %r15

    movdqu (8*8 + 0*16)(%rdi), %xmm0
    movdqu (8*8 + 1*16)(%rdi), %xmm1
    movdqu (8*8 + 2*16)(%rdi), %xmm2
    movdqu (8*8 + 3*16)(%rdi), %xmm3
    movdqu (8*8 + 4*16)(%rdi), %xmm4
    movdqu (8*8 + 5*16)(%rdi), %xmm5
    movdqu (8*8 + 6*16)(%rdi), %xmm6
    movdqu (8*8 + 7*16)(%rdi), %xmm7

    // load rdi from itself last
    mov (3*8)(%rdi), %rdi
    ret
#ifdef __ELF__
.size lucet_context_set,.-lucet_context_set
#endif

.text
.globl lucet_get_current_stack_pointer
#ifdef __ELF__
.type lucet_get_current_stack_pointer,@function
#else
.globl _lucet_get_current_stack_pointer
#endif
.align 16
lucet_get_current_stack_pointer:
_lucet_get_current_stack_pointer:
    mov %rsp, %rax
    ret
#ifdef __ELF__
.size lucet_get_current_stack_pointer,.-lucet_get_current_stack_pointer
#endif

/* Mark that we don't need executable stack. */
#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
