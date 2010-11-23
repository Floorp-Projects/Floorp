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
    EXPORT  |vp8_short_inv_walsh4x4_1_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA    |.text|, CODE, READONLY  ; name this block of code

;short vp8_short_inv_walsh4x4_neon(short *input, short *output)
|vp8_short_inv_walsh4x4_neon| PROC

    ; read in all four lines of values: d0->d3
    vldm.64 r0, {q0, q1}

    ; first for loop

    vadd.s16 d4, d0, d3 ;a = [0] + [12]
    vadd.s16 d5, d1, d2 ;b = [4] + [8]
    vsub.s16 d6, d1, d2 ;c = [4] - [8]
    vsub.s16 d7, d0, d3 ;d = [0] - [12]

    vadd.s16 d0, d4, d5 ;a + b
    vadd.s16 d1, d6, d7 ;c + d
    vsub.s16 d2, d4, d5 ;a - b
    vsub.s16 d3, d7, d6 ;d - c

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
    vadd.s16 d5, d1, d2 ;b = [1] + [2]
    vsub.s16 d6, d1, d2 ;c = [1] - [2]
    vsub.s16 d7, d0, d3 ;d = [0] - [3]

    vadd.s16 d0, d4, d5 ;e = a + b
    vadd.s16 d1, d6, d7 ;f = c + d
    vsub.s16 d2, d4, d5 ;g = a - b
    vsub.s16 d3, d7, d6 ;h = d - c

    vmov.i16 q2, #3
    vadd.i16 q0, q0, q2 ;e/f += 3
    vadd.i16 q1, q1, q2 ;g/h += 3

    vshr.s16 q0, q0, #3 ;e/f >> 3
    vshr.s16 q1, q1, #3 ;g/h >> 3

    vtrn.32 d0, d2
    vtrn.32 d1, d3
    vtrn.16 d0, d1
    vtrn.16 d2, d3

    vstmia.16 r1!, {q0}
    vstmia.16 r1!, {q1}

    bx lr
    ENDP    ; |vp8_short_inv_walsh4x4_neon|


;short vp8_short_inv_walsh4x4_1_neon(short *input, short *output)
|vp8_short_inv_walsh4x4_1_neon| PROC
    ; load a full line into a neon register
    vld1.16  {q0}, [r0]
    ; extract first element and replicate
    vdup.16 q1, d0[0]
    ; add 3 to all values
    vmov.i16 q2, #3
    vadd.i16 q3, q1, q2
    ; right shift
    vshr.s16 q3, q3, #3
    ; write it back
    vstmia.16 r1!, {q3}
    vstmia.16 r1!, {q3}

    bx lr
    ENDP    ; |vp8_short_inv_walsh4x4_1_neon|

    END
