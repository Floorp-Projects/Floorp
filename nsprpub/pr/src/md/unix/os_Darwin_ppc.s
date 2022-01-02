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

.text

#
# PRInt32 __PR_DarwinPPC_AtomicIncrement(PRInt32 *val);
#
        .align  2
        .globl  __PR_DarwinPPC_AtomicIncrement
        .private_extern __PR_DarwinPPC_AtomicIncrement
__PR_DarwinPPC_AtomicIncrement:
        lwarx   r4,0,r3
        addi    r0,r4,1
        stwcx.  r0,0,r3
        bne-    __PR_DarwinPPC_AtomicIncrement
        mr      r3,r0
        blr

#
# PRInt32 __PR_DarwinPPC_AtomicDecrement(PRInt32 *val);
#
        .align  2
        .globl  __PR_DarwinPPC_AtomicDecrement
        .private_extern __PR_DarwinPPC_AtomicDecrement
__PR_DarwinPPC_AtomicDecrement:
        lwarx   r4,0,r3
        addi    r0,r4,-1
        stwcx.  r0,0,r3
        bne-    __PR_DarwinPPC_AtomicDecrement
        mr      r3,r0
        blr

#
# PRInt32 __PR_DarwinPPC_AtomicSet(PRInt32 *val, PRInt32 newval);
#
        .align  2
        .globl  __PR_DarwinPPC_AtomicSet
        .private_extern __PR_DarwinPPC_AtomicSet
__PR_DarwinPPC_AtomicSet:
        lwarx   r5,0,r3
        stwcx.  r4,0,r3
        bne-    __PR_DarwinPPC_AtomicSet
        mr      r3,r5
        blr

#
# PRInt32 __PR_DarwinPPC_AtomicAdd(PRInt32 *ptr, PRInt32 val);
#
        .align  2
        .globl  __PR_DarwinPPC_AtomicAdd
        .private_extern __PR_DarwinPPC_AtomicAdd
__PR_DarwinPPC_AtomicAdd:
        lwarx   r5,0,r3
        add     r0,r4,r5
        stwcx.  r0,0,r3
        bne-    __PR_DarwinPPC_AtomicAdd
        mr      r3,r0
        blr
