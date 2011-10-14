;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_fast_fdct4x4_neon|

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

|vp8_fast_fdct4x4_neon| PROC
    vld1.16         {d2}, [r0], r2              ;load input
    ldr             r12, _ffdct_coeff_
    vld1.16         {d3}, [r0], r2
    vld1.16         {d4}, [r0], r2
    vld1.16         {d0}, [r12]
    vld1.16         {d5}, [r0], r2

    ;First for-loop
    ;transpose d2, d3, d4, d5. Then, d2=ip[0], d3=ip[1], d4=ip[2], d5=ip[3]
    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vadd.s16        d6, d2, d5              ;ip[0]+ip[3]
    vadd.s16        d7, d3, d4              ;ip[1]+ip[2]
    vsub.s16        d8, d3, d4              ;ip[1]-ip[2]
    vsub.s16        d9, d2, d5              ;ip[0]-ip[3]
    vshl.i16        q3, q3, #1              ; a1, b1
    vshl.i16        q4, q4, #1              ; c1, d1

    vadd.s16        d10, d6, d7             ;temp1 = a1 + b1
    vsub.s16        d11, d6, d7             ;temp2 = a1 - b1

    vqdmulh.s16     q6, q5, d0[1]
    vqdmulh.s16     q8, q4, d0[0]
    vqdmulh.s16     q7, q4, d0[2]

    vshr.s16        q6, q6, #1
    vshr.s16        q8, q8, #1
    vshr.s16        q7, q7, #1              ;d14:temp1 = ( c1 * x_c3)>>16;  d15:temp1 =  (d1 * x_c3)>>16
    vadd.s16        q8, q4, q8              ;d16:temp2 = ((c1 * x_c1)>>16) + c1;  d17:temp2 = ((d1 * x_c1)>>16) + d1

    vadd.s16        d2, d10, d12            ;op[0] = ((temp1 * x_c2 )>>16) + temp1
    vadd.s16        d4, d11, d13            ;op[2] = ((temp2 * x_c2 )>>16) + temp2
    vadd.s16        d3, d14, d17            ;op[1] = temp1 + temp2  -- q is not necessary, just for protection
    vsub.s16        d5, d15, d16            ;op[3] = temp1 - temp2

    ;Second for-loop
    ;transpose d2, d3, d4, d5. Then, d2=ip[0], d3=ip[4], d4=ip[8], d5=ip[12]
    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vadd.s16        d6, d2, d5              ;a1 = ip[0]+ip[12]
    vadd.s16        d7, d3, d4              ;b1 = ip[4]+ip[8]
    vsub.s16        d8, d3, d4              ;c1 = ip[4]-ip[8]
    vsub.s16        d9, d2, d5              ;d1 = ip[0]-ip[12]

    vadd.s16        d10, d6, d7             ;temp1 = a1 + b1
    vsub.s16        d11, d6, d7             ;temp2 = a1 - b1


    vqdmulh.s16     q6, q5, d0[1]
    vqdmulh.s16     q8, q4, d0[0]
    vqdmulh.s16     q7, q4, d0[2]

    vshr.s16        q6, q6, #1
    vshr.s16        q8, q8, #1
    vshr.s16        q7, q7, #1              ;d14:temp1 = ( c1 * x_c3)>>16;  d15:temp1 =  (d1 * x_c3)>>16
    vadd.s16        q8, q4, q8              ;d16:temp2 = ((c1 * x_c1)>>16) + c1;  d17:temp2 = ((d1 * x_c1)>>16) + d1

    vadd.s16        d2, d10, d12            ;a2 = ((temp1 * x_c2 )>>16) + temp1
    vadd.s16        d4, d11, d13            ;c2 = ((temp2 * x_c2 )>>16) + temp2
    vadd.s16        d3, d14, d17            ;b2 = temp1 + temp2  -- q is not necessary, just for protection
    vsub.s16        d5, d15, d16            ;d2 = temp1 - temp2

    vclt.s16        q3, q1, #0
    vclt.s16        q4, q2, #0

    vsub.s16        q1, q1, q3
    vsub.s16        q2, q2, q4

    vshr.s16        q1, q1, #1
    vshr.s16        q2, q2, #1

    vst1.16         {q1, q2}, [r1]

    bx              lr

    ENDP

;-----------------

_ffdct_coeff_
    DCD     ffdct_coeff
ffdct_coeff
; 60547 =  0xEC83
; 46341 =  0xB505
; 25080 =  0x61F8
    DCD     0xB505EC83, 0x000061F8

    END
