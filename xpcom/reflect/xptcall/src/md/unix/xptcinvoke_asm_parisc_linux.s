/* -*- Mode: asm; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * Version: MPL 1.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

  .LEVEL 1.1
  .text
  .align 4

framesz:
  .equ 128

.globl NS_InvokeByIndex_P
  .type NS_InvokeByIndex_P, @function


NS_InvokeByIndex_P:
  .PROC
  .CALLINFO FRAME=72, CALLER,SAVE_RP, SAVE_SP, ENTRY_GR=3
  .ENTRY

; frame marker takes 48 bytes,
; register spill area takes 8 bytes,
; local stack area takes 72 bytes result in 128 bytes total

        STW          %rp,-20(%sp)
        STW,MA       %r3,128(%sp)

        LDO     -framesz(%r30),%r28
        STW     %r28,-4(%r30)       ; save previous sp
        STW     %r19,-32(%r30)

        STW     %r26,-36-framesz(%r30)  ; save argument registers in
        STW     %r25,-40-framesz(%r30)  ; in PREVIOUS frame
        STW     %r24,-44-framesz(%r30)  ;
        STW     %r23,-48-framesz(%r30)  ;

        .CALL   ARGW0=GR,ARGW1=GR,ARGW2=GR ;in=24,25,26;out=28
        BL    invoke_count_bytes,%r31
        COPY    %r31,%r2

        CMPIB,>=        0,%r28, .+76
        COPY    %r30,%r3            ; copy stack ptr to saved stack ptr
        ADD     %r30,%r28,%r30      ; extend stack frame
        LDW     -4(%r3),%r28        ; move frame
        STW     %r28,-4(%r30)
        LDW     -8(%r3),%r28
        STW     %r28,-8(%r30)
        LDW     -12(%r3),%r28
        STW     %r28,-12(%r30)
        LDW     -16(%r3),%r28
        STW     %r28,-16(%r30)
        LDW     -20(%r3),%r28
        STW     %r28,-20(%r30)
        LDW     -24(%r3),%r28
        STW     %r28,-24(%r30)
        LDW     -28(%r3),%r28
        STW     %r28,-28(%r30)
        LDW     -32(%r3),%r28
        STW     %r28,-32(%r30)

        LDO     -40(%r30),%r26         ; load copy address
        LDW     -44-framesz(%r3),%r25  ; load rest of 2 arguments
        LDW     -48-framesz(%r3),%r24  ;

        LDW     -32(%r30),%r19 ; shared lib call destroys r19; reload
        .CALL   ARGW0=GR,ARGW1=GR,ARGW2=GR ;in=24,25,26
        BL    invoke_copy_to_stack,%r31
        COPY    %r31,%r2

        LDO     -48(%r30),%r20
        EXTRW,U,= %r28,31,1,%r22
        FLDD    0(%r20),%fr7  ; load double arg 1
        EXTRW,U,= %r28,30,1,%r22
        FLDW    8(%r20),%fr5L ; load float arg 1
        EXTRW,U,= %r28,29,1,%r22
        FLDW    4(%r20),%fr6L ; load float arg 2
        EXTRW,U,= %r28,28,1,%r22
        FLDW    0(%r20),%fr7L ; load float arg 3

        LDW     -36-framesz(%r3),%r26  ; load ptr to 'that'
        LDW     -40(%r30),%r25  ; load the rest of dispatch argument registers
        LDW     -44(%r30),%r24
        LDW     -48(%r30),%r23

        LDW     -36-framesz(%r3),%r20  ; load vtable addr
        LDW     -40-framesz(%r3),%r28  ; load index
        LDW     0(%r20),%r20    ; follow vtable
        SH2ADDL %r28,%r20,%r28  ; add 4*index to vtable entry
        LDW     0(%r28),%r22    ; load vtable entry

        .CALL ARGW0=GR,ARGW1=GR,ARGW2=GR,ARGW3=GR,RTNVAL=GR ;in=22-26;out=28;
        BL    $$dyncall,%r31
        COPY    %r31,%r2

        LDW     -32(%r30),%r19
        COPY    %r3,%r30              ; restore saved stack ptr

        LDW          -148(%sp),%rp
        LDWM       -128(%sp),%r3
        BV,N             (%rp)
        NOP
  .EXIT
  .PROCEND  ;in=23,24,25,26;
  .SIZE NS_InvokeByIndex_P, .-NS_InvokeByIndex_P

