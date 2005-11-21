// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// 
// The contents of this file are subject to the Mozilla Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/MPL/
// 
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
// implied. See the License for the specific language governing
// rights and limitations under the License.
// 
// The Original Code is the Netscape Portable Runtime (NSPR).
// 
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are 
// Copyright (C) 2000 Netscape Communications Corporation.  All
// Rights Reserved.
// 
// Contributor(s):
// 
// Alternatively, the contents of this file may be used under the
// terms of the GNU General Public License Version 2 or later (the
// "GPL"), in which case the provisions of the GPL are applicable 
// instead of those above.  If you wish to allow use of your 
// version of this file only under the terms of the GPL and not to
// allow others to use your version of this file under the MPL,
// indicate your decision by deleting the provisions above and
// replace them with the notice and other provisions required by
// the GPL.  If you do not delete the provisions above, a recipient
// may use your version of this file under either the MPL or the
// GPL.
//

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
#ifndef _LP64
        addp4 r32 = 0, r32  ;;
#endif
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
#ifndef _LP64
        addp4 r32 = 0, r32  ;;
#endif
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
#ifndef _LP64
        addp4 r32 = 0, r32  ;;
#endif
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
#ifndef _LP64
        addp4 r32 = 0, r32  ;;
#endif
        xchg4 r8 = [r32], r33
        br.ret.sptk.many b0
        .endp _PR_ia64_AtomicSet#
