;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_short_fdct4x4_neon|
    EXPORT  |vp8_short_fdct8x4_neon|
    ARM
    REQUIRE8
    PRESERVE8


    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    short *input
; r1    short *output
; r2    int pitch
; Input has a pitch, output is contiguous
|vp8_short_fdct4x4_neon| PROC
    ldr             r12, _dct_matrix_
    vld1.16         d0, [r0], r2
    vld1.16         d1, [r0], r2
    vld1.16         d2, [r0], r2
    vld1.16         d3, [r0]
    vld1.16         {q2, q3}, [r12]

;first stage
    vmull.s16       q11, d4, d0[0]              ;i=0
    vmull.s16       q12, d4, d1[0]              ;i=1
    vmull.s16       q13, d4, d2[0]              ;i=2
    vmull.s16       q14, d4, d3[0]              ;i=3

    vmlal.s16       q11, d5, d0[1]
    vmlal.s16       q12, d5, d1[1]
    vmlal.s16       q13, d5, d2[1]
    vmlal.s16       q14, d5, d3[1]

    vmlal.s16       q11, d6, d0[2]
    vmlal.s16       q12, d6, d1[2]
    vmlal.s16       q13, d6, d2[2]
    vmlal.s16       q14, d6, d3[2]

    vmlal.s16       q11, d7, d0[3]              ;sumtemp for i=0
    vmlal.s16       q12, d7, d1[3]              ;sumtemp for i=1
    vmlal.s16       q13, d7, d2[3]              ;sumtemp for i=2
    vmlal.s16       q14, d7, d3[3]              ;sumtemp for i=3

    ; rounding
    vrshrn.i32      d22, q11, #14
    vrshrn.i32      d24, q12, #14
    vrshrn.i32      d26, q13, #14
    vrshrn.i32      d28, q14, #14

;second stage
    vmull.s16       q4, d22, d4[0]              ;i=0
    vmull.s16       q5, d22, d4[1]              ;i=1
    vmull.s16       q6, d22, d4[2]              ;i=2
    vmull.s16       q7, d22, d4[3]              ;i=3

    vmlal.s16       q4, d24, d5[0]
    vmlal.s16       q5, d24, d5[1]
    vmlal.s16       q6, d24, d5[2]
    vmlal.s16       q7, d24, d5[3]

    vmlal.s16       q4, d26, d6[0]
    vmlal.s16       q5, d26, d6[1]
    vmlal.s16       q6, d26, d6[2]
    vmlal.s16       q7, d26, d6[3]

    vmlal.s16       q4, d28, d7[0]              ;sumtemp for i=0
    vmlal.s16       q5, d28, d7[1]              ;sumtemp for i=1
    vmlal.s16       q6, d28, d7[2]              ;sumtemp for i=2
    vmlal.s16       q7, d28, d7[3]              ;sumtemp for i=3

    vrshr.s32       q0, q4, #16
    vrshr.s32       q1, q5, #16
    vrshr.s32       q2, q6, #16
    vrshr.s32       q3, q7, #16

    vmovn.i32       d0, q0
    vmovn.i32       d1, q1
    vmovn.i32       d2, q2
    vmovn.i32       d3, q3

    vst1.16         {q0, q1}, [r1]

    bx              lr

    ENDP

; r0    short *input
; r1    short *output
; r2    int pitch
|vp8_short_fdct8x4_neon| PROC
    ; Store link register and input before calling
    ;  first 4x4 fdct.  Do not need to worry about
    ;  output or pitch because those pointers are not
    ;  touched in the 4x4 fdct function
    stmdb           sp!, {r0, lr}

    bl              vp8_short_fdct4x4_neon

    ldmia           sp!, {r0, lr}

    ; Move to the next block of data.
    add             r0, r0, #8
    add             r1, r1, #32

    ; Second time through do not store off the
    ;  link register, just return from the 4x4 fdtc
    b               vp8_short_fdct4x4_neon

    ; Should never get to this.
    bx              lr

    ENDP

;-----------------

_dct_matrix_
    DCD     dct_matrix
dct_matrix
;   DCW     23170,  30274,  23170, 12540
;   DCW     23170,  12540, -23170,-30274
;   DCW     23170, -12540, -23170, 30274
;   DCW     23170, -30274,  23170,-12540
; 23170 =  0x5a82
; -23170 =  0xa57e
; 30274 =  0x7642
; -30274 =  0x89be
; 12540 =  0x30fc
; -12540 = 0xcf04
    DCD     0x76425a82, 0x30fc5a82
    DCD     0x30fc5a82, 0x89bea57e
    DCD     0xcf045a82, 0x7642a57e
    DCD     0x89be5a82, 0xcf045a82

    END
