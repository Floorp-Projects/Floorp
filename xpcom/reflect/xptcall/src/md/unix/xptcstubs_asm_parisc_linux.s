/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corp, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Randolph Chung     <tausq@debian.org>
 *   Jory A. Pratt      <geekypenguin@gmail.com>
 *   Raúl Porcel        <armin76@gentoo.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

  .LEVEL  1.1
  .TEXT
  .ALIGN 4

curframesz:
  .EQU 128


; SharedStub has stack size of 128 bytes

lastframesz:
  .EQU 64

; the StubN C++ function has a small stack size of 64 bytes


.globl SharedStub
  .type SharedStub, @function

SharedStub:
  .PROC
  .CALLINFO CALLER,FRAME=80,SAVE_RP

  .ENTRY
        STW     %rp,-20(%sp)
        LDO     128(%sp),%sp

        STW     %r19,-32(%r30)
        STW     %r26,-36-curframesz(%r30) ; save arg0 in previous frame

        LDO     -80(%r30),%r28
        FSTD,MA %fr5,8(%r28)   ; save darg0
        FSTD,MA %fr7,8(%r28)   ; save darg1
        FSTW,MA %fr4L,4(%r28)  ; save farg0
        FSTW,MA %fr5L,4(%r28)  ; save farg1
        FSTW,MA %fr6L,4(%r28)  ; save farg2
        FSTW,MA %fr7L,4(%r28)  ; save farg3

        ; Former value of register 26 is already properly saved by StubN,
        ; but register 25-23 are not because of the argument mismatch
        STW     %r25,-40-curframesz-lastframesz(%r30) ; save r25
        STW     %r24,-44-curframesz-lastframesz(%r30) ; save r24
        STW     %r23,-48-curframesz-lastframesz(%r30) ; save r23
        COPY    %r26,%r25                             ; method index is arg1
        LDW     -36-curframesz-lastframesz(%r30),%r26 ; self is arg0
        LDO     -40-curframesz-lastframesz(%r30),%r24 ; normal args is arg2
        LDO     -80(%r30),%r23                        ; floating args is arg3

        .CALL   ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR ;in=23-26;out=28;
        BL     PrepareAndDispatch, %r31
        COPY    %r31,%r2

        LDW     -32(%r30),%r19

        LDW     -148(%sp),%rp
        LDO     -128(%sp),%sp


        BV,N     (%rp)
        NOP
        NOP

  .EXIT
  .PROCEND        ;in=26;out=28;

  .SIZE SharedStub, .-SharedStub
