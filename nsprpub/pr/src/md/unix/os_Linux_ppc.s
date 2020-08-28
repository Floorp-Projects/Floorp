# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Based on the programming examples in The PowerPC Architecture:
# A Specification for A New Family of RISC Processors, 2nd Ed.,
# Book I, Section E.1, "Synchronization," pp. 249-256, May 1994.
#

        .section ".text"

#
# PRInt32 _PR_ppc_AtomicIncrement(PRInt32 *val);
#
        .align  2
        .globl  _PR_ppc_AtomicIncrement
        .type _PR_ppc_AtomicIncrement,@function
_PR_ppc_AtomicIncrement:
.Lfd1:  lwarx   4,0,3
        addi    0,4,1
        stwcx.  0,0,3
        bne-    .Lfd1
        mr      3,0
        blr
.Lfe1:  .size _PR_ppc_AtomicIncrement,.Lfe1-_PR_ppc_AtomicIncrement

#
# PRInt32 _PR_ppc_AtomicDecrement(PRInt32 *val);
#
        .align  2
        .globl  _PR_ppc_AtomicDecrement
        .type _PR_ppc_AtomicDecrement,@function
_PR_ppc_AtomicDecrement:
.Lfd2:  lwarx   4,0,3
        addi    0,4,-1
        stwcx.  0,0,3
        bne-    .Lfd2
        mr      3,0
        blr
.Lfe2:  .size _PR_ppc_AtomicDecrement,.Lfe2-_PR_ppc_AtomicDecrement

#
# PRInt32 _PR_ppc_AtomicSet(PRInt32 *val, PRInt32 newval);
#
        .align  2
        .globl  _PR_ppc_AtomicSet
        .type _PR_ppc_AtomicSet,@function
_PR_ppc_AtomicSet:
.Lfd3:  lwarx   5,0,3
        stwcx.  4,0,3
        bne-    .Lfd3
        mr      3,5
        blr
.Lfe3:  .size _PR_ppc_AtomicSet,.Lfe3-_PR_ppc_AtomicSet

#
# PRInt32 _PR_ppc_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#
        .align  2
        .globl  _PR_ppc_AtomicAdd
        .type _PR_ppc_AtomicAdd,@function
_PR_ppc_AtomicAdd:
.Lfd4:  lwarx   5,0,3
        add     0,4,5
        stwcx.  0,0,3
        bne-    .Lfd4
        mr      3,0
        blr
.Lfe4:  .size _PR_ppc_AtomicAdd,.Lfe4-_PR_ppc_AtomicAdd

# Magic indicating no need for an executable stack
.section .note.GNU-stack, "", @progbits
