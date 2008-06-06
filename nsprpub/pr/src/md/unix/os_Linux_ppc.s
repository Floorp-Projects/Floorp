# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape Portable Runtime (NSPR).
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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
.section .note.GNU-stack, "", @progbits ; .previous
