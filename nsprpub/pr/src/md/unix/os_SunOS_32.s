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

!  ======================================================================
!
!  Atomically add a new element to the top of the stack
!
!  usage : PR_StackPush(listp, elementp);
!
!  -----------------------
!  Note on REGISTER USAGE:
!  as this is a LEAF procedure, a new stack frame is not created.
!
!  So, the registers used are:
!     %o0  [input]   - the address of the stack
!     %o1  [input]   - the address of the element to be added to the stack
!  -----------------------

		.section	".text"
		.global		PR_StackPush

PR_StackPush:

pulock:	ld		[%o0],%o3				! 
		cmp		%o3,-1					! check if stack is locked
		be		pulock					! loop, if locked
		mov		-1,%o3					! use delay-slot
		swap	[%o0],%o3				! atomically lock the stack and get
										! the pointer to stack head
		cmp		%o3,-1					! check, if the stack is locked
		be		pulock					! loop, if so
		nop
		st		%o3,[%o1]
		retl                           	! return back to the caller
		st		%o1,[%o0]				! 

		.size	PR_StackPush,(.-PR_StackPush)


!  end
!  ======================================================================

!  ======================================================================
!
!  Atomically remove the element at the top of the stack
!
!  usage : elemep = PR_StackPop(listp);
!
!  -----------------------
!  Note on REGISTER USAGE:
!  as this is a LEAF procedure, a new stack frame is not created.
!
!  So, the registers used are:
!     %o0  [input]   - the address of the stack
!     %o1  [input]   - work register (top element)
!  -----------------------

		.section	".text"
		.global		PR_StackPop

PR_StackPop:

polock:	ld		[%o0],%o1				! 
		cmp		%o1,-1					! check if stack is locked
		be		polock					! loop, if locked
		mov		-1,%o1					! use delay-slot
		swap	[%o0],%o1				! atomically lock the stack and get
										! the pointer to stack head
		cmp		%o1,-1					! check, if the stack is locked
		be		polock					! loop, if so
		nop
		tst		%o1						! test for empty stack
		be,a	empty					! is empty
		st		%g0,[%o0]
		ld		[%o1], %o2				! load the second element
		st		%o2,[%o0]				! set stack head to second
		st		%g0,[%o1]				! reset the next pointer; for
										! debugging
empty:
        retl                            ! return back to the caller
		mov		%o1, %o0				! return the first element

		.size	PR_StackPop,(.-PR_StackPop)

!  end
!  ======================================================================
