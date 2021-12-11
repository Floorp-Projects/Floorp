/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
