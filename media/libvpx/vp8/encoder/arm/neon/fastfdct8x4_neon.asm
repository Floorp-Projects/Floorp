;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_fast_fdct8x4_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;void vp8_fast_fdct4x4_c(short *input, short *output, int pitch);
;NOTE:
;The input *src_diff. src_diff is calculated as:
;diff_ptr[c] = src_ptr[c] - pred_ptr[c]; (in Subtract* function)
;In which *src_ptr and *pred_ptr both are unsigned char.
;Therefore, *src_diff should be in the range of [-255, 255].
;CAUTION:
;The input values of 25th block are set in vp8_build_dcblock function, which are out of [-255, 255].
;But, VP8 encoder only uses vp8_short_fdct4x4_c for 25th block, not vp8_fast_fdct4x4_c. That makes
;it ok for assuming *input in [-255, 255] in vp8_fast_fdct4x4_c, but not ok in vp8_short_fdct4x4_c.

|vp8_fast_fdct8x4_neon| PROC
    vld1.16         {q1}, [r0], r2              ;load input
    ldr             r12, _ffdct8_coeff_
    vld1.16         {q2}, [r0], r2
    vld1.16         {q3}, [r0], r2
    vld1.16         {d0}, [r12]
    vld1.16         {q4}, [r0], r2

    ;First for-loop
    ;transpose d2, d4, d6, d8. Then, d2=ip[0], d4=ip[1], d6=ip[2], d8=ip[3]
    ;transpose d3, d5, d7, d9. Then, d3=ip[0], d5=ip[1], d7=ip[2], d9=ip[3]
    vtrn.32         d2, d6
    vtrn.32         d3, d7
    vtrn.32         d4, d8
    vtrn.32         d5, d9
    vtrn.16         d2, d4
    vtrn.16         d3, d5
    vtrn.16         d6, d8
    vtrn.16         d7, d9

    vadd.s16        d10, d2, d8             ;ip[0]+ip[3]
    vadd.s16        d11, d4, d6             ;ip[1]+ip[2]
    vsub.s16        d12, d4, d6             ;ip[1]-ip[2]
    vsub.s16        d13, d2, d8             ;ip[0]-ip[3]
    vadd.s16        d22, d3, d9
    vadd.s16        d23, d5, d7
    vsub.s16        d24, d5, d7
    vsub.s16        d25, d3, d9

    vshl.i16        q5, q5, #1              ; a1, b1
    vshl.i16        q6, q6, #1              ; c1, d1
    vshl.i16        q1, q11, #1
    vshl.i16        q2, q12, #1

    vadd.s16        d14, d10, d11           ;temp1 = a1 + b1
    vsub.s16        d15, d10, d11           ;temp2 = a1 - b1
    vadd.s16        d24, d2, d3
    vsub.s16        d25, d2, d3

    vqdmulh.s16     q8, q7, d0[1]
    vqdmulh.s16     q13, q12, d0[1]
    vqdmulh.s16     q10, q6, d0[0]
    vqdmulh.s16     q15, q2, d0[0]
    vqdmulh.s16     q9, q6, d0[2]
    vqdmulh.s16     q14, q2, d0[2]

    vshr.s16        q8, q8, #1
    vshr.s16        q13, q13, #1
    vshr.s16        q10, q10, #1
    vshr.s16        q15, q15, #1
    vshr.s16        q9, q9, #1              ;d18:temp1 = ( c1 * x_c3)>>16;  d19:temp1 =  (d1 * x_c3)>>16
    vshr.s16        q14, q14, #1            ;d28:temp1 = ( c1 * x_c3)>>16;  d29:temp1 =  (d1 * x_c3)>>16
    vadd.s16        q10, q6, q10            ;d20:temp2 = ((c1 * x_c1)>>16) + c1;  d21:temp2 = ((d1 * x_c1)>>16) + d1
    vadd.s16        q15, q2, q15            ;d30:temp2 = ((c1 * x_c1)>>16) + c1;  d31:temp2 = ((d1 * x_c1)>>16) + d1

    vadd.s16        d2, d14, d16            ;op[0] = ((temp1 * x_c2 )>>16) + temp1
    vadd.s16        d3, d24, d26            ;op[0] = ((temp1 * x_c2 )>>16) + temp1
    vadd.s16        d6, d15, d17            ;op[2] = ((temp2 * x_c2 )>>16) + temp2
    vadd.s16        d7, d25, d27            ;op[2] = ((temp2 * x_c2 )>>16) + temp2
    vadd.s16        d4, d18, d21            ;op[1] = temp1 + temp2  -- q is not necessary, just for protection
    vadd.s16        d5, d28, d31            ;op[1] = temp1 + temp2  -- q is not necessary, just for protection
    vsub.s16        d8, d19, d20            ;op[3] = temp1 - temp2
    vsub.s16        d9, d29, d30            ;op[3] = temp1 - temp2

    ;Second for-loop
    ;transpose d2, d4, d6, d8. Then, d2=ip[0], d4=ip[4], d6=ip[8], d8=ip[12]
    ;transpose d3, d5, d7, d9. Then, d3=ip[0], d5=ip[4], d7=ip[8], d9=ip[12]
    vtrn.32         d2, d6
    vtrn.32         d3, d7
    vtrn.32         d4, d8
    vtrn.32         d5, d9
    vtrn.16         d2, d4
    vtrn.16         d3, d5
    vtrn.16         d6, d8
    vtrn.16         d7, d9

    vadd.s16        d10, d2, d8             ;a1 = ip[0]+ip[12]
    vadd.s16        d11, d4, d6             ;b1 = ip[4]+ip[8]
    vsub.s16        d12, d4, d6             ;c1 = ip[4]-ip[8]
    vsub.s16        d13, d2, d8             ;d1 = ip[0]-ip[12]
    vadd.s16        d2, d3, d9
    vadd.s16        d4, d5, d7
    vsub.s16        d24, d5, d7
    vsub.s16        d25, d3, d9

    vadd.s16        d14, d10, d11           ;temp1 = a1 + b1
    vsub.s16        d15, d10, d11           ;temp2 = a1 - b1
    vadd.s16        d22, d2, d4
    vsub.s16        d23, d2, d4

    vqdmulh.s16     q8, q7, d0[1]
    vqdmulh.s16     q13, q11, d0[1]
    vqdmulh.s16     q10, q6, d0[0]
    vqdmulh.s16     q15, q12, d0[0]
    vqdmulh.s16     q9, q6, d0[2]
    vqdmulh.s16     q14, q12, d0[2]

    vshr.s16        q8, q8, #1
    vshr.s16        q13, q13, #1
    vshr.s16        q10, q10, #1
    vshr.s16        q15, q15, #1
    vshr.s16        q9, q9, #1              ;d18:temp1 = ( c1 * x_c3)>>16;  d19:temp1 =  (d1 * x_c3)>>16
    vshr.s16        q14, q14, #1            ;d28:temp1 = ( c1 * x_c3)>>16;  d29:temp1 =  (d1 * x_c3)>>16
    vadd.s16        q10, q6, q10            ;d20:temp2 = ((c1 * x_c1)>>16) + c1;  d21:temp2 = ((d1 * x_c1)>>16) + d1
    vadd.s16        q15, q12, q15           ;d30:temp2 = ((c1 * x_c1)>>16) + c1;  d31:temp2 = ((d1 * x_c1)>>16) + d1

    vadd.s16        d2, d14, d16            ;a2 = ((temp1 * x_c2 )>>16) + temp1
    vadd.s16        d6, d22, d26            ;a2 = ((temp1 * x_c2 )>>16) + temp1
    vadd.s16        d4, d15, d17            ;c2 = ((temp2 * x_c2 )>>16) + temp2
    vadd.s16        d8, d23, d27            ;c2 = ((temp2 * x_c2 )>>16) + temp2
    vadd.s16        d3, d18, d21            ;b2 = temp1 + temp2  -- q is not necessary, just for protection
    vadd.s16        d7, d28, d31            ;b2 = temp1 + temp2  -- q is not necessary, just for protection
    vsub.s16        d5, d19, d20            ;d2 = temp1 - temp2
    vsub.s16        d9, d29, d30            ;d2 = temp1 - temp2

    vclt.s16        q5, q1, #0
    vclt.s16        q6, q2, #0
    vclt.s16        q7, q3, #0
    vclt.s16        q8, q4, #0

    vsub.s16        q1, q1, q5
    vsub.s16        q2, q2, q6
    vsub.s16        q3, q3, q7
    vsub.s16        q4, q4, q8

    vshr.s16        q1, q1, #1
    vshr.s16        q2, q2, #1
    vshr.s16        q3, q3, #1
    vshr.s16        q4, q4, #1

    vst1.16         {q1, q2}, [r1]!
    vst1.16         {q3, q4}, [r1]

    bx              lr

    ENDP

;-----------------

_ffdct8_coeff_
    DCD     ffdct8_coeff
ffdct8_coeff
; 60547 =  0xEC83
; 46341 =  0xB505
; 25080 =  0x61F8
    DCD     0xB505EC83, 0x000061F8

    END
