# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2003 Netscape Communications Corporation.  All
# Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable
# instead of those above.  If you wish to allow use of your
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

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
__PR_DarwinPPC_AtomicAdd:
        lwarx   r5,0,r3
        add     r0,r4,r5
        stwcx.  r0,0,r3
        bne-    __PR_DarwinPPC_AtomicAdd
        mr      r3,r0
        blr
