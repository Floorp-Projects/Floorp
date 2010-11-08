;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_dequant_idct_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;void vp8_dequant_idct_neon(short *input, short *dq, unsigned char *pred,
;                           unsigned char *dest, int pitch, int stride)
; r0    short *input,
; r1    short *dq,
; r2    unsigned char *pred
; r3    unsigned char *dest
; sp    int pitch
; sp+4  int stride

|vp8_dequant_idct_add_neon| PROC
    vld1.16         {q3, q4}, [r0]
    vld1.16         {q5, q6}, [r1]
    ldr             r1, [sp]                ; pitch
    vld1.32         {d14[0]}, [r2], r1
    vld1.32         {d14[1]}, [r2], r1
    vld1.32         {d15[0]}, [r2], r1
    vld1.32         {d15[1]}, [r2]

    ldr             r1, [sp, #4]            ; stride

    ldr             r12, _CONSTANTS_

    vmul.i16        q1, q3, q5              ;input for short_idct4x4llm_neon
    vmul.i16        q2, q4, q6

;|short_idct4x4llm_neon| PROC
    vld1.16         {d0}, [r12]
    vswp            d3, d4                  ;q2(vp[4] vp[12])

    vqdmulh.s16     q3, q2, d0[2]
    vqdmulh.s16     q4, q2, d0[0]

    vqadd.s16       d12, d2, d3             ;a1
    vqsub.s16       d13, d2, d3             ;b1

    vshr.s16        q3, q3, #1
    vshr.s16        q4, q4, #1

    vqadd.s16       q3, q3, q2
    vqadd.s16       q4, q4, q2

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

; memset(input, 0, 32) -- 32bytes
    vmov.i16        q14, #0

    vswp            d3, d4
    vqdmulh.s16     q3, q2, d0[2]
    vqdmulh.s16     q4, q2, d0[0]

    vqadd.s16       d12, d2, d3             ;a1
    vqsub.s16       d13, d2, d3             ;b1

    vmov            q15, q14

    vshr.s16        q3, q3, #1
    vshr.s16        q4, q4, #1

    vqadd.s16       q3, q3, q2
    vqadd.s16       q4, q4, q2

    vqsub.s16       d10, d6, d9             ;c1
    vqadd.s16       d11, d7, d8             ;d1

    vqadd.s16       d2, d12, d11
    vqadd.s16       d3, d13, d10
    vqsub.s16       d4, d13, d10
    vqsub.s16       d5, d12, d11

    vst1.16         {q14, q15}, [r0]

    vrshr.s16       d2, d2, #3
    vrshr.s16       d3, d3, #3
    vrshr.s16       d4, d4, #3
    vrshr.s16       d5, d5, #3

    vtrn.32         d2, d4
    vtrn.32         d3, d5
    vtrn.16         d2, d3
    vtrn.16         d4, d5

    vaddw.u8        q1, q1, d14
    vaddw.u8        q2, q2, d15

    vqmovun.s16     d0, q1
    vqmovun.s16     d1, q2

    vst1.32         {d0[0]}, [r3], r1
    vst1.32         {d0[1]}, [r3], r1
    vst1.32         {d1[0]}, [r3], r1
    vst1.32         {d1[1]}, [r3]

    bx             lr

    ENDP           ; |vp8_dequant_idct_add_neon|

; Constant Pool
_CONSTANTS_       DCD cospi8sqrt2minus1
cospi8sqrt2minus1 DCD 0x4e7b4e7b
sinpi8sqrt2       DCD 0x8a8c8a8c

    END
