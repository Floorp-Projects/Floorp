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

LOCALSZ=16
FRAMESZ=(((NARGSAVE+LOCALSZ)*SZREG)+ALSZ)&ALMASK

A1OFF=FRAMESZ-(9*SZREG)
A2OFF=FRAMESZ-(8*SZREG)
A3OFF=FRAMESZ-(7*SZREG)
A4OFF=FRAMESZ-(6*SZREG)
A5OFF=FRAMESZ-(5*SZREG)
A6OFF=FRAMESZ-(4*SZREG)
A7OFF=FRAMESZ-(3*SZREG)
GPOFF=FRAMESZ-(2*SZREG)
RAOFF=FRAMESZ-(1*SZREG)

F13OFF=FRAMESZ-(16*SZREG)
F14OFF=FRAMESZ-(15*SZREG)
F15OFF=FRAMESZ-(14*SZREG)
F16OFF=FRAMESZ-(13*SZREG)
F17OFF=FRAMESZ-(12*SZREG)
F18OFF=FRAMESZ-(11*SZREG)
F19OFF=FRAMESZ-(10*SZREG)

#define SENTINEL_ENTRY(n)         /* defined in cpp file, not here */

#define STUB_ENTRY(x)                                   \
    .if x < 10;                                         \
    MAKE_STUB(x, _ZN14nsXPTCStubBase5Stub ##x ##Ev);    \
    .elseif x < 100;                                    \
    MAKE_STUB(x, _ZN14nsXPTCStubBase6Stub ##x ##Ev);    \
    .elseif x < 1000;                                   \
    MAKE_STUB(x, _ZN14nsXPTCStubBase7Stub ##x ##Ev);    \
    .else;                                              \
    .err;                                               \
    .endif

#define MAKE_STUB(x, name)                              \
    .globl   name;                                      \
    .type    name,@function;                            \
    .aent    name,0;                                    \
name:;                                                  \
    PTR_SUBU sp,FRAMESZ;                                \
    SETUP_GP64(GPOFF, name);                            \
    li       t0,x;                                      \
    b        sharedstub;                                \

#
# open a dummy frame for the function entries
#
    .text
    .align   2
    .type    dummy,@function
    .ent     dummy, 0
dummy:
    .frame   sp, FRAMESZ, ra
    .mask    0x90000FF0, RAOFF-FRAMESZ
    .fmask   0x000FF000, F19OFF-FRAMESZ

#include "xptcstubsdef.inc"

sharedstub:

    REG_S    a1, A1OFF(sp)
    REG_S    a2, A2OFF(sp)
    REG_S    a3, A3OFF(sp)
    REG_S    a4, A4OFF(sp)
    REG_S    a5, A5OFF(sp)
    REG_S    a6, A6OFF(sp)
    REG_S    a7, A7OFF(sp)
    REG_S    ra, RAOFF(sp)

    s.d      $f13, F13OFF(sp)
    s.d      $f14, F14OFF(sp)
    s.d      $f15, F15OFF(sp)
    s.d      $f16, F16OFF(sp)
    s.d      $f17, F17OFF(sp)
    s.d      $f18, F18OFF(sp)
    s.d      $f19, F19OFF(sp)

    # t0 is methodIndex
    move     a1, t0

    # a2 is stack address where extra function params
    # are stored that do not fit in registers
    move     a2, sp
    addi     a2, FRAMESZ

    # a3 is stack address of a1..a7
    move     a3, sp
    addi     a3, A1OFF

    # a4 is stack address of f13..f19
    move     a4, sp
    addi     a4, F13OFF

    # PrepareAndDispatch(that, methodIndex, args, gprArgs, fpArgs)
    #                     a0       a1        a2     a3       a4
    #
    jal      PrepareAndDispatch

    REG_L    ra, RAOFF(sp)
    RESTORE_GP64

    PTR_ADDU sp, FRAMESZ
    j        ra
    END(dummy)
