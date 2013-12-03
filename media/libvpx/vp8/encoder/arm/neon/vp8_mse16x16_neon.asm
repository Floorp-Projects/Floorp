;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_mse16x16_neon|
    EXPORT  |vp8_get4x4sse_cs_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;============================
; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
;note: in this function, sum is never used. So, we can remove this part of calculation
;from vp8_variance().

|vp8_mse16x16_neon| PROC
    vmov.i8         q7, #0                      ;q7, q8, q9, q10 - sse
    vmov.i8         q8, #0
    vmov.i8         q9, #0
    vmov.i8         q10, #0

    mov             r12, #8

mse16x16_neon_loop
    vld1.8          {q0}, [r0], r1              ;Load up source and reference
    vld1.8          {q2}, [r2], r3
    vld1.8          {q1}, [r0], r1
    vld1.8          {q3}, [r2], r3

    vsubl.u8        q11, d0, d4
    vsubl.u8        q12, d1, d5
    vsubl.u8        q13, d2, d6
    vsubl.u8        q14, d3, d7

    vmlal.s16       q7, d22, d22
    vmlal.s16       q8, d23, d23

    subs            r12, r12, #1

    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25
    vmlal.s16       q7, d26, d26
    vmlal.s16       q8, d27, d27
    vmlal.s16       q9, d28, d28
    vmlal.s16       q10, d29, d29

    bne             mse16x16_neon_loop

    vadd.u32        q7, q7, q8
    vadd.u32        q9, q9, q10

    ldr             r12, [sp]               ;load *sse from stack

    vadd.u32        q10, q7, q9
    vpaddl.u32      q1, q10
    vadd.u64        d0, d2, d3

    vst1.32         {d0[0]}, [r12]
    vmov.32         r0, d0[0]

    bx              lr

    ENDP


;=============================
; r0    unsigned char *src_ptr,
; r1    int  source_stride,
; r2    unsigned char *ref_ptr,
; r3    int  recon_stride
|vp8_get4x4sse_cs_neon| PROC
    vld1.8          {d0}, [r0], r1              ;Load up source and reference
    vld1.8          {d4}, [r2], r3
    vld1.8          {d1}, [r0], r1
    vld1.8          {d5}, [r2], r3
    vld1.8          {d2}, [r0], r1
    vld1.8          {d6}, [r2], r3
    vld1.8          {d3}, [r0], r1
    vld1.8          {d7}, [r2], r3

    vsubl.u8        q11, d0, d4
    vsubl.u8        q12, d1, d5
    vsubl.u8        q13, d2, d6
    vsubl.u8        q14, d3, d7

    vmull.s16       q7, d22, d22
    vmull.s16       q8, d24, d24
    vmull.s16       q9, d26, d26
    vmull.s16       q10, d28, d28

    vadd.u32        q7, q7, q8
    vadd.u32        q9, q9, q10
    vadd.u32        q9, q7, q9

    vpaddl.u32      q1, q9
    vadd.u64        d0, d2, d3

    vmov.32         r0, d0[0]
    bx              lr

    ENDP

    END
