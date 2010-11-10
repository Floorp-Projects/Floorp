;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_recon_b_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char  *pred_ptr,
; r1    short *diff_ptr,
; r2    unsigned char *dst_ptr,
; r3    int stride

|vp8_recon_b_neon| PROC
    mov             r12, #16

    vld1.u8         {d28}, [r0], r12    ;load 4 data/line from pred_ptr
    vld1.16         {q10, q11}, [r1]!   ;load data from diff_ptr
    vld1.u8         {d29}, [r0], r12
    vld1.16         {q11, q12}, [r1]!
    vld1.u8         {d30}, [r0], r12
    vld1.16         {q12, q13}, [r1]!
    vld1.u8         {d31}, [r0], r12
    vld1.16         {q13}, [r1]

    vmovl.u8        q0, d28             ;modify Pred data from 8 bits to 16 bits
    vmovl.u8        q1, d29             ;Pred data in d0, d2, d4, d6
    vmovl.u8        q2, d30
    vmovl.u8        q3, d31

    vadd.s16        d0, d0, d20         ;add Diff data and Pred data together
    vadd.s16        d2, d2, d22
    vadd.s16        d4, d4, d24
    vadd.s16        d6, d6, d26

    vqmovun.s16     d0, q0              ;CLAMP() saturation
    vqmovun.s16     d1, q1
    vqmovun.s16     d2, q2
    vqmovun.s16     d3, q3
    add             r1, r2, r3

    vst1.32         {d0[0]}, [r2]       ;store result
    vst1.32         {d1[0]}, [r1], r3
    add             r2, r1, r3
    vst1.32         {d2[0]}, [r1]
    vst1.32         {d3[0]}, [r2], r3

    bx             lr

    ENDP
    END
