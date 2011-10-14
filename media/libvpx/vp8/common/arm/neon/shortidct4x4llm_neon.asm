;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_short_idct4x4llm_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;*************************************************************
;void vp8_short_idct4x4llm_c(short *input, short *output, int pitch)
;r0 short * input
;r1 short * output
;r2 int pitch
;*************************************************************
;static const int cospi8sqrt2minus1=20091;
;static const int sinpi8sqrt2      =35468;
;static const int rounding = 0;
;Optimization note: The resulted data from dequantization are signed 13-bit data that is
;in the range of [-4096, 4095]. This allows to use "vqdmulh"(neon) instruction since
;it won't go out of range (13+16+1=30bits<32bits). This instruction gives the high half
;result of the multiplication that is needed in IDCT.

|vp8_short_idct4x4llm_neon| PROC
    adr             r12, idct_coeff
    vld1.16         {q1, q2}, [r0]
    vld1.16         {d0}, [r12]

    vswp            d3, d4                  ;q2(vp[4] vp[12])

    vqdmulh.s16     q3, q2, d0[2]
    vqdmulh.s16     q4, q2, d0[0]

    vqadd.s16       d12, d2, d3             ;a1
    vqsub.s16       d13, d2, d3             ;b1

    vshr.s16        q3, q3, #1
    vshr.s16        q4, q4, #1

    vqadd.s16       q3, q3, q2              ;modify since sinpi8sqrt2 > 65536/2 (negtive number)
    vqadd.s16       q4, q4, q2

    ;d6 - c1:temp1
    ;d7 - d1:temp2
    ;d8 - d1:temp1
    ;d9 - c1:temp2

    vqsub.s16       d10, d6, d9             ;c1
    vqadd.s16       d11, d7, d8             ;d1

    vqadd.s16       d2, d12, d11
    vqadd.s16       d3, d13, d10
    vqsub.s16       d4, d13, d10
    vqsub.s16       d5, d12, d11

    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vswp            d3, d4

    vqdmulh.s16     q3, q2, d0[2]
    vqdmulh.s16     q4, q2, d0[0]

    vqadd.s16       d12, d2, d3             ;a1
    vqsub.s16       d13, d2, d3             ;b1

    vshr.s16        q3, q3, #1
    vshr.s16        q4, q4, #1

    vqadd.s16       q3, q3, q2              ;modify since sinpi8sqrt2 > 65536/2 (negtive number)
    vqadd.s16       q4, q4, q2

    vqsub.s16       d10, d6, d9             ;c1
    vqadd.s16       d11, d7, d8             ;d1

    vqadd.s16       d2, d12, d11
    vqadd.s16       d3, d13, d10
    vqsub.s16       d4, d13, d10
    vqsub.s16       d5, d12, d11

    vrshr.s16       d2, d2, #3
    vrshr.s16       d3, d3, #3
    vrshr.s16       d4, d4, #3
    vrshr.s16       d5, d5, #3

    add             r3, r1, r2
    add             r12, r3, r2
    add             r0, r12, r2

    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vst1.16         {d2}, [r1]
    vst1.16         {d3}, [r3]
    vst1.16         {d4}, [r12]
    vst1.16         {d5}, [r0]

    bx             lr

    ENDP

;-----------------

idct_coeff
    DCD     0x4e7b4e7b, 0x8a8c8a8c

;20091, 20091, 35468, 35468

    END
