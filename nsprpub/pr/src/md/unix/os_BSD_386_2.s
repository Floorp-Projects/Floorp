/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * os_BSD_386_2.s
 * We need to define our own setjmp/longjmp on BSDI 2.x because libc's
 * implementation does some sanity checking that defeats user level threads.
 * This should no longer be necessary in BSDI 3.0.
 */

.globl _setjmp
.align 2
_setjmp:
		movl	4(%esp),%eax
		movl	0(%esp),%edx
		movl	%edx, 0(%eax)           /* rta */
		movl	%ebx, 4(%eax)
		movl	%esp, 8(%eax)
		movl	%ebp,12(%eax)
		movl	%esi,16(%eax)
		movl	%edi,20(%eax)
		movl	$0,%eax
		ret
 
.globl _longjmp
.align 2
_longjmp:
		movl	4(%esp),%edx
		movl	8(%esp),%eax
		movl	0(%edx),%ecx
		movl	4(%edx),%ebx
		movl	8(%edx),%esp
		movl	12(%edx),%ebp
		movl	16(%edx),%esi
		movl	20(%edx),%edi
		cmpl	$0,%eax
		jne		1f
		movl	$1,%eax
1:		movl	%ecx,0(%esp)
		ret
