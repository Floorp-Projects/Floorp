/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
