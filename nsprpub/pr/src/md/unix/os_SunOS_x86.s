// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

//
// sol_getsp()
//
// Return the current sp (for debugging)
//
	.globl sol_getsp
sol_getsp:
	movl	%esp, %eax
	ret

//
// sol_curthread()
//
// Return a unique identifier for the currently active thread.
//
	.globl sol_curthread
sol_curthread:
	movl	%ecx, %eax
	ret

// PRInt32 _MD_AtomicIncrement(PRInt32 *val)
//
// Atomically increment the integer pointed to by 'val' and return
// the result of the increment.
//
    .text
    .globl _MD_AtomicIncrement
    .align 4
_MD_AtomicIncrement:
    movl 4(%esp), %ecx
    movl $1, %eax
    lock
    xaddl %eax, (%ecx)
    incl %eax
    ret

// PRInt32 _MD_AtomicDecrement(PRInt32 *val)
//
// Atomically decrement the integer pointed to by 'val' and return
// the result of the decrement.
//
    .text
    .globl _MD_AtomicDecrement
    .align 4
_MD_AtomicDecrement:
    movl 4(%esp), %ecx
    movl $-1, %eax
    lock
    xaddl %eax, (%ecx)
    decl %eax
    ret

// PRInt32 _MD_AtomicSet(PRInt32 *val, PRInt32 newval)
//
// Atomically set the integer pointed to by 'val' to the new
// value 'newval' and return the old value.
//
// An alternative implementation:
//   .text
//   .globl _MD_AtomicSet
//   .align 4
//_MD_AtomicSet:
//   movl 4(%esp), %ecx
//   movl 8(%esp), %edx
//   movl (%ecx), %eax
//retry:
//   lock
//   cmpxchgl %edx, (%ecx)
//   jne retry
//   ret
//
    .text
    .globl _MD_AtomicSet
    .align 4
_MD_AtomicSet:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    xchgl %eax, (%ecx)
    ret

// PRInt32 _MD_AtomicAdd(PRInt32 *ptr, PRInt32 val)
//
// Atomically add 'val' to the integer pointed to by 'ptr'
// and return the result of the addition.
//
    .text
    .globl _MD_AtomicAdd
    .align 4
_MD_AtomicAdd:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    movl %eax, %edx
    lock
    xaddl %eax, (%ecx)
    addl %edx, %eax
    ret
