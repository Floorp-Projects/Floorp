;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_recon4b_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char  *pred_ptr,
; r1    short *diff_ptr,
; r2    unsigned char *dst_ptr,
; r3    int stride

|vp8_recon4b_neon| PROC
    vld1.u8         {q12, q13}, [r0]!   ;load data from pred_ptr
    vld1.16         {q8, q9}, [r1]!     ;load data from diff_ptr
    vld1.u8         {q14, q15}, [r0]
    vld1.16         {q10, q11}, [r1]!

    vmovl.u8        q0, d24             ;modify Pred data from 8 bits to 16 bits
    vmovl.u8        q1, d25
    vmovl.u8        q2, d26
    vmovl.u8        q3, d27
    vmovl.u8        q4, d28
    vmovl.u8        q5, d29
    vmovl.u8        q6, d30
    vld1.16         {q12, q13}, [r1]!
    vmovl.u8        q7, d31
    vld1.16         {q14, q15}, [r1]

    vadd.s16        q0, q0, q8          ;add Diff data and Pred data together
    vadd.s16        q1, q1, q9
    vadd.s16        q2, q2, q10
    vadd.s16        q3, q3, q11
    vadd.s16        q4, q4, q12
    vadd.s16        q5, q5, q13
    vadd.s16        q6, q6, q14
    vadd.s16        q7, q7, q15

    vqmovun.s16     d0, q0              ;CLAMP() saturation
    vqmovun.s16     d1, q1
    vqmovun.s16     d2, q2
    vqmovun.s16     d3, q3
    vqmovun.s16     d4, q4
    vqmovun.s16     d5, q5
    vqmovun.s16     d6, q6
    vqmovun.s16     d7, q7
    add             r0, r2, r3

    vst1.u8         {q0}, [r2]          ;store result
    vst1.u8         {q1}, [r0], r3
    add             r2, r0, r3
    vst1.u8         {q2}, [r0]
    vst1.u8         {q3}, [r2], r3

    bx             lr

    ENDP
    END
