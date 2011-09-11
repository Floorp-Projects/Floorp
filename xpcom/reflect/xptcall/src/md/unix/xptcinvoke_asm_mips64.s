/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   ZHANG Le    <r0bertz@gentoo.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <sys/regdef.h>
#include <sys/asm.h>

.text
.globl  invoke_count_words
.globl  invoke_copy_to_stack

LOCALSZ=7        # a0, a1, a2, a3, s0, ra, gp
FRAMESZ=(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK

RAOFF=FRAMESZ-(1*SZREG)
A0OFF=FRAMESZ-(2*SZREG)
A1OFF=FRAMESZ-(3*SZREG)
A2OFF=FRAMESZ-(4*SZREG)
A3OFF=FRAMESZ-(5*SZREG)
S0OFF=FRAMESZ-(6*SZREG)
GPOFF=FRAMESZ-(7*SZREG)

#
# _NS_InvokeByIndex_P(that, methodIndex, paramCount, params)
#                      a0       a1          a2         a3

NESTED(_NS_InvokeByIndex_P, FRAMESZ, ra)
    PTR_SUBU sp, FRAMESZ
    SETUP_GP64(GPOFF, _NS_InvokeByIndex_P)

    REG_S    ra, RAOFF(sp)
    REG_S    a0, A0OFF(sp)
    REG_S    a1, A1OFF(sp)
    REG_S    a2, A2OFF(sp)
    REG_S    a3, A3OFF(sp)
    REG_S    s0, S0OFF(sp)

    # invoke_count_words(paramCount, params)
    move     a0, a2
    move     a1, a3
    jal      invoke_count_words

    # invoke_copy_to_stack(PRUint32* d, PRUint32 paramCount,
    #                      nsXPTCVariant* s, PRUint32 *reg)

    REG_L    a1, A2OFF(sp) # a1 - paramCount
    REG_L    a2, A3OFF(sp) # a2 - params

    # save sp before we copy the params to the stack
    move     t0, sp

    # assume full size of 16 bytes per param to be safe
    sll      v0, 4         # 16 bytes * num params
    subu     sp, sp, v0    # make room
    move     a0, sp        # a0 - param stack address

    # create temporary stack space to write int and fp regs
    subu     sp, 64        # 64 = 8 regs of 8 bytes
    move     a3, sp

    # save the old sp and save the arg stack
    subu     sp, sp, 16
    REG_S    t0, 0(sp)
    REG_S    a0, 8(sp)

    # copy the param into the stack areas
    jal      invoke_copy_to_stack

    REG_L    t3, 8(sp)     # get previous a0
    REG_L    sp, 0(sp)     # get orig sp back

    REG_L    a0, A0OFF(sp) # a0 - that
    REG_L    a1, A1OFF(sp) # a1 - methodIndex

    # t1 = methodIndex * pow(2, PTRLOG)
    # (use shift instead of mult)
    sll      t1, a1, PTRLOG

    # calculate the function we need to jump to,
    # which must then be saved in t9
    lw       t9, 0(a0)
    addu     t9, t9, t1
    lw       t9, (t9)

    # get register save area from invoke_copy_to_stack
    subu     t1, t3, 64

    # a1..a7 and f13..f19 should now be set to what
    # invoke_copy_to_stack told us. skip a0 and f12
    # because that's the "this" pointer

    REG_L    a1,  0(t1)
    REG_L    a2,  8(t1)
    REG_L    a3, 16(t1)
    REG_L    a4, 24(t1)
    REG_L    a5, 32(t1)
    REG_L    a6, 40(t1)
    REG_L    a7, 48(t1)

    l.d      $f13,  0(t1)
    l.d      $f14,  8(t1)
    l.d      $f15, 16(t1)
    l.d      $f16, 24(t1)
    l.d      $f17, 32(t1)
    l.d      $f18, 40(t1)
    l.d      $f19, 48(t1)

    # save away our stack pointer and create
    # the stack pointer for the function
    move     s0, sp
    move     sp, t3

    jalr     t9

    move     sp, s0

    RESTORE_GP64
    REG_L    ra, RAOFF(sp)
    REG_L    s0, S0OFF(sp)
    PTR_ADDU sp, FRAMESZ
    j    ra
.end _NS_InvokeByIndex_P
