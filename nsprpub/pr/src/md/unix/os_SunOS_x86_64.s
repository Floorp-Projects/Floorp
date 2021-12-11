// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// PRInt32 _MD_AtomicIncrement(PRInt32 *val)
//
// Atomically increment the integer pointed to by 'val' and return
// the result of the increment.
//
    .text
    .globl _MD_AtomicIncrement
    .align 4
_MD_AtomicIncrement:
    movl $1, %eax
    lock
    xaddl %eax, (%rdi)
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
    movl $-1, %eax
    lock
    xaddl %eax, (%rdi)
    decl %eax
    ret

// PRInt32 _MD_AtomicSet(PRInt32 *val, PRInt32 newval)
//
// Atomically set the integer pointed to by 'val' to the new
// value 'newval' and return the old value.
//
    .text
    .globl _MD_AtomicSet
    .align 4
_MD_AtomicSet:
    movl %esi, %eax
    xchgl %eax, (%rdi)
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
    movl %esi, %eax
    lock
    xaddl %eax, (%rdi)
    addl %esi, %eax
    ret
