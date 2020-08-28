// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// PRInt32 _PR_x86_64_AtomicIncrement(PRInt32 *val)
//
// Atomically increment the integer pointed to by 'val' and return
// the result of the increment.
//
    .text
    .globl _PR_x86_64_AtomicIncrement
    .type _PR_x86_64_AtomicIncrement, @function
    .align 4
_PR_x86_64_AtomicIncrement:
    movl $1, %eax
    lock
    xaddl %eax, (%rdi)
    incl %eax
    ret
    .size _PR_x86_64_AtomicIncrement, .-_PR_x86_64_AtomicIncrement

// PRInt32 _PR_x86_64_AtomicDecrement(PRInt32 *val)
//
// Atomically decrement the integer pointed to by 'val' and return
// the result of the decrement.
//
    .text
    .globl _PR_x86_64_AtomicDecrement
    .type _PR_x86_64_AtomicDecrement, @function
    .align 4
_PR_x86_64_AtomicDecrement:
    movl $-1, %eax
    lock
    xaddl %eax, (%rdi)
    decl %eax
    ret
    .size _PR_x86_64_AtomicDecrement, .-_PR_x86_64_AtomicDecrement

// PRInt32 _PR_x86_64_AtomicSet(PRInt32 *val, PRInt32 newval)
//
// Atomically set the integer pointed to by 'val' to the new
// value 'newval' and return the old value.
//
    .text
    .globl _PR_x86_64_AtomicSet
    .type _PR_x86_64_AtomicSet, @function
    .align 4
_PR_x86_64_AtomicSet:
    movl %esi, %eax
    xchgl %eax, (%rdi)
    ret
    .size _PR_x86_64_AtomicSet, .-_PR_x86_64_AtomicSet

// PRInt32 _PR_x86_64_AtomicAdd(PRInt32 *ptr, PRInt32 val)
//
// Atomically add 'val' to the integer pointed to by 'ptr'
// and return the result of the addition.
//
    .text
    .globl _PR_x86_64_AtomicAdd
    .type _PR_x86_64_AtomicAdd, @function
    .align 4
_PR_x86_64_AtomicAdd:
    movl %esi, %eax
    lock
    xaddl %eax, (%rdi)
    addl %esi, %eax
    ret
    .size _PR_x86_64_AtomicAdd, .-_PR_x86_64_AtomicAdd

// Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits
