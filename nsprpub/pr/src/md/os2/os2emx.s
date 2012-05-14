/ -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
/ 
/ This Source Code Form is subject to the terms of the Mozilla Public
/ License, v. 2.0. If a copy of the MPL was not distributed with this
/ file, You can obtain one at http://mozilla.org/MPL/2.0/.

/ PRInt32 __PR_MD_ATOMIC_INCREMENT(PRInt32 *val)
/
/ Atomically increment the integer pointed to by 'val' and return
/ the result of the increment.
/
    .text
    .globl __PR_MD_ATOMIC_INCREMENT
    .align 4
__PR_MD_ATOMIC_INCREMENT:
    movl 4(%esp), %ecx
    movl $1, %eax
    lock
    xaddl %eax, (%ecx)
    incl %eax
    ret

/ PRInt32 __PR_MD_ATOMIC_DECREMENT(PRInt32 *val)
/
/ Atomically decrement the integer pointed to by 'val' and return
/ the result of the decrement.
/
    .text
    .globl __PR_MD_ATOMIC_DECREMENT
    .align 4
__PR_MD_ATOMIC_DECREMENT:
    movl 4(%esp), %ecx
    movl $-1, %eax
    lock
    xaddl %eax, (%ecx)
    decl %eax
    ret

/ PRInt32 __PR_MD_ATOMIC_SET(PRInt32 *val, PRInt32 newval)
/
/ Atomically set the integer pointed to by 'val' to the new
/ value 'newval' and return the old value.
/
/ An alternative implementation:
/   .text
/   .globl __PR_MD_ATOMIC_SET
/   .align 4
/__PR_MD_ATOMIC_SET:
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
    .globl __PR_MD_ATOMIC_SET
    .align 4
__PR_MD_ATOMIC_SET:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    xchgl %eax, (%ecx)
    ret

/ PRInt32 __PR_MD_ATOMIC_ADD(PRInt32 *ptr, PRInt32 val)
/
/ Atomically add 'val' to the integer pointed to by 'ptr'
/ and return the result of the addition.
/
    .text
    .globl __PR_MD_ATOMIC_ADD
    .align 4
__PR_MD_ATOMIC_ADD:
    movl 4(%esp), %ecx
    movl 8(%esp), %eax
    movl %eax, %edx
    lock
    xaddl %eax, (%ecx)
    addl %edx, %eax
    ret
