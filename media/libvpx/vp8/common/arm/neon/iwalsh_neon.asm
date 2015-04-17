;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;
    EXPORT  |vp8_short_inv_walsh4x4_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA    |.text|, CODE, READONLY  ; name this block of code

;short vp8_short_inv_walsh4x4_neon(short *input, short *mb_dqcoeff)
|vp8_short_inv_walsh4x4_neon| PROC

    ; read in all four lines of values: d0->d3
    vld1.i16 {q0-q1}, [r0@128]

    ; first for loop
    vadd.s16 d4, d0, d3 ;a = [0] + [12]
    vadd.s16 d6, d1, d2 ;b = [4] + [8]
    vsub.s16 d5, d0, d3 ;d = [0] - [12]
    vsub.s16 d7, d1, d2 ;c = [4] - [8]

    vadd.s16 q0, q2, q3 ; a+b d+c
    vsub.s16 q1, q2, q3 ; a-b d-c

    vtrn.32 d0, d2 ;d0:  0  1  8  9
                   ;d2:  2  3 10 11
    vtrn.32 d1, d3 ;d1:  4  5 12 13
                   ;d3:  6  7 14 15

    vtrn.16 d0, d1 ;d0:  0  4  8 12
                   ;d1:  1  5  9 13
    vtrn.16 d2, d3 ;d2:  2  6 10 14
                   ;d3:  3  7 11 15

    ; second for loop

    vadd.s16 d4, d0, d3 ;a = [0] + [3]
    vadd.s16 d6, d1, d2 ;b = [1] + [2]
    vsub.s16 d5, d0, d3 ;d = [0] - [3]
    vsub.s16 d7, d1, d2 ;c = [1] - [2]

    vmov.i16 q8, #3

    vadd.s16 q0, q2, q3 ; a+b d+c
    vsub.s16 q1, q2, q3 ; a-b d-c

    vadd.i16 q0, q0, q8 ;e/f += 3
    vadd.i16 q1, q1, q8 ;g/h += 3

    vshr.s16 q0, q0, #3 ;e/f >> 3
    vshr.s16 q1, q1, #3 ;g/h >> 3

    mov      r2, #64
    add      r3, r1, #32

    vst1.i16 d0[0], [r1],r2
    vst1.i16 d1[0], [r3],r2
    vst1.i16 d2[0], [r1],r2
    vst1.i16 d3[0], [r3],r2

    vst1.i16 d0[1], [r1],r2
    vst1.i16 d1[1], [r3],r2
    vst1.i16 d2[1], [r1],r2
    vst1.i16 d3[1], [r3],r2

    vst1.i16 d0[2], [r1],r2
    vst1.i16 d1[2], [r3],r2
    vst1.i16 d2[2], [r1],r2
    vst1.i16 d3[2], [r3],r2

    vst1.i16 d0[3], [r1],r2
    vst1.i16 d1[3], [r3],r2
    vst1.i16 d2[3], [r1]
    vst1.i16 d3[3], [r3]

    bx lr
    ENDP    ; |vp8_short_inv_walsh4x4_neon|

    END
