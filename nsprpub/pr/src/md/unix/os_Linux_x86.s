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
/ Copyright (C) 2000 Netscape Communications Corporation.  All Rights
/ Reserved.
/

/ PRInt32 _PR_x86_AtomicIncrement(PRInt32 *val)
/
/ Atomically increment the integer pointed to by 'val' and return
/ the result of the increment.
/
    .text
    .globl _PR_x86_AtomicIncrement
    .align 4
_PR_x86_AtomicIncrement:
    movl 4(%esp), %ecx
    movl $1, %eax
    lock
    xaddl %eax, (%ecx)
    incl %eax
    ret

/ PRInt32 _PR_x86_AtomicDecrement(PRInt32 *val)
/
/ Atomically decrement the integer pointed to by 'val' and return
/ the result of the decrement.
/
    .text
    .globl _PR_x86_AtomicDecrement
    .align 4
_PR_x86_AtomicDecrement:
    movl 4(%esp), %ecx
    movl $-1, %eax
    lock
    xaddl %eax, (%ecx)
    decl %eax
    ret

/ PRInt32 _PR_x86_AtomicSet(PRInt32 *val, PRInt32 newval)
/
/ Atomically set the integer pointed to by 'val' to the new
/ value 'newval' and return the old value.
/
/ An alternative implementation:
/   .text
/   .globl _PR_x86_AtomicSet
/   .align 4
/_PR_x86_AtomicSet:
/   movl 4(%esp), %ecx
/   movl 8(%esp), %edx
/   movl (%ecx), %eax
/retry:
/   lock
/   cmpxchgl %edx, (%ecx)
/   jne retry
/   ret
/
    .text
    .globl _PR_x86_AtomicSet
    .align 4
_PR_x86_AtomicSet:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    lock
    xchgl %eax, (%ecx)
    ret

/ PRInt32 _PR_x86_AtomicAdd(PRInt32 *ptr, PRInt32 val)
/
/ Atomically add 'val' to the integer pointed to by 'ptr'
/ and return the result of the addition.
/
    .text
    .globl _PR_x86_AtomicAdd
    .align 4
_PR_x86_AtomicAdd:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    movl %eax, %edx
    lock
    xaddl %eax, (%ecx)
    addl %edx, %eax
    ret
