// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

.text

// PRInt32 _PR_ia64_AtomicIncrement(PRInt32 *val)
//
// Atomically increment the integer pointed to by 'val' and return
// the result of the increment.
//
        .align 16
        .global _PR_ia64_AtomicIncrement#
        .proc _PR_ia64_AtomicIncrement#
_PR_ia64_AtomicIncrement:
        fetchadd4.acq r8 = [r32], 1  ;;
        adds r8 = 1, r8
        br.ret.sptk.many b0
        .endp _PR_ia64_AtomicIncrement#

// PRInt32 _PR_ia64_AtomicDecrement(PRInt32 *val)
//
// Atomically decrement the integer pointed to by 'val' and return
// the result of the decrement.
//
        .align 16
        .global _PR_ia64_AtomicDecrement#
        .proc _PR_ia64_AtomicDecrement#
_PR_ia64_AtomicDecrement:
        fetchadd4.rel r8 = [r32], -1  ;;
        adds r8 = -1, r8
        br.ret.sptk.many b0
        .endp _PR_ia64_AtomicDecrement#

// PRInt32 _PR_ia64_AtomicAdd(PRInt32 *ptr, PRInt32 val)
//
// Atomically add 'val' to the integer pointed to by 'ptr'
// and return the result of the addition.
//
        .align 16
        .global _PR_ia64_AtomicAdd#
        .proc _PR_ia64_AtomicAdd#
_PR_ia64_AtomicAdd:
        ld4 r15 = [r32]  ;;
.L3:
        mov r14 = r15
        mov ar.ccv = r15
        add r8 = r15, r33  ;;
        cmpxchg4.acq r15 = [r32], r8, ar.ccv  ;;
        cmp4.ne p6, p7 =  r15, r14
        (p6) br.cond.dptk .L3
        br.ret.sptk.many b0
        .endp _PR_ia64_AtomicAdd#

// PRInt32 _PR_ia64_AtomicSet(PRInt32 *val, PRInt32 newval)
//
// Atomically set the integer pointed to by 'val' to the new
// value 'newval' and return the old value.
//
        .align 16
        .global _PR_ia64_AtomicSet#
        .proc _PR_ia64_AtomicSet#
_PR_ia64_AtomicSet:
        xchg4 r8 = [r32], r33
        br.ret.sptk.many b0
        .endp _PR_ia64_AtomicSet#

// Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits
