;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_recon16x16mb_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char  *pred_ptr,
; r1    short *diff_ptr,
; r2    unsigned char *dst_ptr,
; r3    int ystride,
; stack unsigned char *udst_ptr,
; stack unsigned char *vdst_ptr

|vp8_recon16x16mb_neon| PROC
    mov             r12, #4             ;loop counter for Y loop

recon16x16mb_loop_y
    vld1.u8         {q12, q13}, [r0]!   ;load data from pred_ptr
    vld1.16         {q8, q9}, [r1]!     ;load data from diff_ptr
    vld1.u8         {q14, q15}, [r0]!
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
    vld1.16         {q14, q15}, [r1]!

    pld             [r0]
    pld             [r1]
    pld             [r1, #64]

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
    vst1.u8         {q0}, [r2], r3      ;store result
    vqmovun.s16     d6, q6
    vst1.u8         {q1}, [r2], r3
    vqmovun.s16     d7, q7
    vst1.u8         {q2}, [r2], r3
    subs            r12, r12, #1

    moveq           r12, #2             ;loop counter for UV loop

    vst1.u8         {q3}, [r2], r3
    bne             recon16x16mb_loop_y

    mov             r3, r3, lsr #1      ;uv_stride = ystride>>1
    ldr             r2, [sp]            ;load upred_ptr

recon16x16mb_loop_uv
    vld1.u8         {q12, q13}, [r0]!   ;load data from pred_ptr
    vld1.16         {q8, q9}, [r1]!     ;load data from diff_ptr
    vld1.u8         {q14, q15}, [r0]!
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
    vld1.16         {q14, q15}, [r1]!

    vadd.s16        q0, q0, q8          ;add Diff data and Pred data together
    vadd.s16        q1, q1, q9
    vadd.s16        q2, q2, q10
    vadd.s16        q3, q3, q11
    vadd.s16        q4, q4, q12
    vadd.s16        q5, q5, q13
    vadd.s16        q6, q6, q14

    vqmovun.s16     d0, q0              ;CLAMP() saturation
    vadd.s16        q7, q7, q15
    vqmovun.s16     d1, q1
    vqmovun.s16     d2, q2
    vqmovun.s16     d3, q3
    vst1.u8         {d0}, [r2], r3      ;store result
    vqmovun.s16     d4, q4
    vst1.u8         {d1}, [r2], r3
    vqmovun.s16     d5, q5
    vst1.u8         {d2}, [r2], r3
    vqmovun.s16     d6, q6
    vst1.u8         {d3}, [r2], r3
    vqmovun.s16     d7, q7
    vst1.u8         {d4}, [r2], r3
    subs            r12, r12, #1

    vst1.u8         {d5}, [r2], r3
    vst1.u8         {d6}, [r2], r3
    vst1.u8         {d7}, [r2], r3

    ldrne           r2, [sp, #4]        ;load vpred_ptr
    bne             recon16x16mb_loop_uv

    bx             lr

    ENDP
    END
