/ -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/
/ The contents of this file are subject to the Netscape Public License
/ Version 1.1 (the "NPL"); you may not use this file except in
/ compliance with the NPL.  You may obtain a copy of the NPL at
/ http://www.mozilla.org/NPL/
/ 
/ Software distributed under the NPL is distributed on an "AS IS" basis,
/ WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
/ for the specific language governing rights and limitations under the
/ NPL.
/ 
/ The Initial Developer of this code under the NPL is Netscape
/ Communications Corporation.  Portions created by Netscape are
/ Copyright (C) 1998 Netscape Communications Corporation.  All Rights
/ Reserved.
/
	.text

	.globl	getedi
getedi:
	movl	%edi,%eax
	ret
	.type	getedi,@function
	.size	getedi,.-getedi
 
	.globl	setedi
setedi:
	movl	4(%esp),%edi
	ret
	.type	setedi,@function
	.size	setedi,.-setedi

	.globl	__MD_FlushRegisterWindows
	.globl _MD_FlushRegisterWindows

__MD_FlushRegisterWindows:
_MD_FlushRegisterWindows:

	ret

/
/ sol_getsp()
/
/ Return the current sp (for debugging)
/
	.globl sol_getsp
sol_getsp:
	movl	%esp, %eax
	ret

/
/ sol_curthread()
/
/ Return a unique identifier for the currently active thread.
/
	.globl sol_curthread
sol_curthread:
	movl	%ecx, %eax
	ret

/
/ PR_StackPush(listp, elementp)
/
/ Atomically push ElementP onto linked list ListP.
/
	.text
	.globl	PR_StackPush
	.align	4
PR_StackPush:
	movl	4(%esp), %ecx
	movl	$-1,%eax
pulock:
/ Already locked?
	cmpl	%eax,(%ecx)
	je	pulock

/ Attempt to lock it
	lock
	xchgl	%eax, (%ecx)

/ Did we set the lock?
	cmpl	$-1, %eax
	je	pulock

/ We now have the lock.  Update pointers
	movl	8(%esp), %edx
	movl	%eax, (%edx)
	movl	%edx, (%ecx)

/ Done
	ret


/
/ elementp = PR_StackPop(listp)
/
/ Atomically pop ElementP off linked list ListP
/
	.text
	.globl	PR_StackPop
	.align	4
PR_StackPop:
	movl	4(%esp), %ecx
	movl	$-1, %eax
polock:
/ Already locked?
	cmpl	%eax, (%ecx)
	je	polock

/ Attempt to lock it
	lock
	xchgl	%eax, (%ecx)

/ Did we set the lock?
	cmpl	$-1, %eax
	je	polock

/ We set the lock so now update pointers

/ Was it empty?
	movl	$0, %edx
	cmpl	%eax,%edx
	je	empty

/ Get element "next" pointer
	movl	(%eax), %edx

/ Write NULL to the element "next" pointer
	movl	$0, (%eax)

empty:
/ Put elements previous "next" value into listp
/ NOTE: This also unlocks the listp
	movl	%edx, (%ecx)

/ Return previous listp value (already in eax)
	ret
