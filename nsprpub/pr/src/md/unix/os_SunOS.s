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
	.text

/*
 * sol_getsp()
 *
 * Return the current sp (for debugging)
 */
	.global sol_getsp
sol_getsp:
	retl
   	mov     %sp, %o0


/*
 * sol_curthread()
 *
 * Return a unique identifier for the currently active thread.
 */
	.global sol_curthread
sol_curthread:
    retl
    mov %g7, %o0
                  

	.global __MD_FlushRegisterWindows
	.global _MD_FlushRegisterWindows

__MD_FlushRegisterWindows:
_MD_FlushRegisterWindows:

	ta	3
	ret
	restore

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
