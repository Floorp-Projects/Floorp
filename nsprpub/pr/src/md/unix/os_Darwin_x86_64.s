# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# PRInt32 __PR_Darwin_x86_64_AtomicIncrement(PRInt32 *val)
#
# Atomically increment the integer pointed to by 'val' and return
# the result of the increment.
#
    .text
    .globl __PR_Darwin_x86_64_AtomicIncrement
    .private_extern __PR_Darwin_x86_64_AtomicIncrement
    .align 4
__PR_Darwin_x86_64_AtomicIncrement:
    movl $1, %eax
    lock
    xaddl %eax, (%rdi)
    incl %eax
    ret

# PRInt32 __PR_Darwin_x86_64_AtomicDecrement(PRInt32 *val)
#
# Atomically decrement the integer pointed to by 'val' and return
# the result of the decrement.
#
    .text
    .globl __PR_Darwin_x86_64_AtomicDecrement
    .private_extern __PR_Darwin_x86_64_AtomicDecrement
    .align 4
__PR_Darwin_x86_64_AtomicDecrement:
    movl $-1, %eax
    lock
    xaddl %eax, (%rdi)
    decl %eax
    ret

# PRInt32 __PR_Darwin_x86_64_AtomicSet(PRInt32 *val, PRInt32 newval)
#
# Atomically set the integer pointed to by 'val' to the new
# value 'newval' and return the old value.
#
    .text
    .globl __PR_Darwin_x86_64_AtomicSet
    .private_extern __PR_Darwin_x86_64_AtomicSet
    .align 4
__PR_Darwin_x86_64_AtomicSet:
    movl %esi, %eax
    xchgl %eax, (%rdi)
    ret

# PRInt32 __PR_Darwin_x86_64_AtomicAdd(PRInt32 *ptr, PRInt32 val)
#
# Atomically add 'val' to the integer pointed to by 'ptr'
# and return the result of the addition.
#
    .text
    .globl __PR_Darwin_x86_64_AtomicAdd
    .private_extern __PR_Darwin_x86_64_AtomicAdd
    .align 4
__PR_Darwin_x86_64_AtomicAdd:
    movl %esi, %eax
    lock
    xaddl %eax, (%rdi)
    addl %esi, %eax
    ret
