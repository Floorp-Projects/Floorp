/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
* 
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
* 
* The Original Code is the Netscape Portable Runtime (NSPR).
* 
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are 
* Copyright (C) 1998-2000 Netscape Communications Corporation.  All
* Rights Reserved.
* 
* Contributor(s):
* 
* Alternatively, the contents of this file may be used under the
* terms of the GNU General Public License Version 2 or later (the
* "GPL"), in which case the provisions of the GPL are applicable 
* instead of those above.  If you wish to allow use of your 
* version of this file only under the terms of the GPL and not to
* allow others to use your version of this file under the MPL,
* indicate your decision by deleting the provisions above and
* replace them with the notice and other provisions required by
* the GPL.  If you do not delete the provisions above, a recipient
* may use your version of this file under either the MPL or the
* GPL.
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
