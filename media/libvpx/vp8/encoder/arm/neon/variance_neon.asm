;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_variance16x16_neon|
    EXPORT  |vp8_variance16x8_neon|
    EXPORT  |vp8_variance8x16_neon|
    EXPORT  |vp8_variance8x8_neon|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|vp8_variance16x16_neon| PROC
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

    mov             r12, #8

variance16x16_neon_loop
    vld1.8          {q0}, [r0], r1              ;Load up source and reference
    vld1.8          {q2}, [r2], r3
    vld1.8          {q1}, [r0], r1
    vld1.8          {q3}, [r2], r3

    vsubl.u8        q11, d0, d4                 ;calculate diff
    vsubl.u8        q12, d1, d5
    vsubl.u8        q13, d2, d6
    vsubl.u8        q14, d3, d7

    ;VPADAL adds adjacent pairs of elements of a vector, and accumulates
    ;the results into the elements of the destination vector. The explanation
    ;in ARM guide is wrong.
    vpadal.s16      q8, q11                     ;calculate sum
    vmlal.s16       q9, d22, d22                ;calculate sse
    vmlal.s16       q10, d23, d23

    subs            r12, r12, #1

    vpadal.s16      q8, q12
    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25
    vpadal.s16      q8, q13
    vmlal.s16       q9, d26, d26
    vmlal.s16       q10, d27, d27
    vpadal.s16      q8, q14
    vmlal.s16       q9, d28, d28
    vmlal.s16       q10, d29, d29

    bne             variance16x16_neon_loop

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    ldr             r12, [sp]                   ;load *sse from stack

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    ;vmov.32        r0, d0[0]                   ;this instruction costs a lot
    ;vmov.32        r1, d1[0]
    ;mul            r0, r0, r0
    ;str            r1, [r12]
    ;sub            r0, r1, r0, asr #8

    ;sum is in [-255x256, 255x256]. sumxsum is 32-bit. Shift to right should
    ;have sign-bit exension, which is vshr.s. Have to use s32 to make it right.
    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [r12]              ;store sse
    vshr.s32        d10, d10, #8
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    bx              lr

    ENDP

;================================
;unsigned int vp8_variance16x8_c(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride,
;   unsigned int *sse)
|vp8_variance16x8_neon| PROC
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

    mov             r12, #4

variance16x8_neon_loop
    vld1.8          {q0}, [r0], r1              ;Load up source and reference
    vld1.8          {q2}, [r2], r3
    vld1.8          {q1}, [r0], r1
    vld1.8          {q3}, [r2], r3

    vsubl.u8        q11, d0, d4                 ;calculate diff
    vsubl.u8        q12, d1, d5
    vsubl.u8        q13, d2, d6
    vsubl.u8        q14, d3, d7

    vpadal.s16      q8, q11                     ;calculate sum
    vmlal.s16       q9, d22, d22                ;calculate sse
    vmlal.s16       q10, d23, d23

    subs            r12, r12, #1

    vpadal.s16      q8, q12
    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25
    vpadal.s16      q8, q13
    vmlal.s16       q9, d26, d26
    vmlal.s16       q10, d27, d27
    vpadal.s16      q8, q14
    vmlal.s16       q9, d28, d28
    vmlal.s16       q10, d29, d29

    bne             variance16x8_neon_loop

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    ldr             r12, [sp]                   ;load *sse from stack

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [r12]              ;store sse
    vshr.s32        d10, d10, #7
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    bx              lr

    ENDP

;=================================
;unsigned int vp8_variance8x16_c(
;    unsigned char *src_ptr,
;    int  source_stride,
;    unsigned char *ref_ptr,
;    int  recon_stride,
;   unsigned int *sse)

|vp8_variance8x16_neon| PROC
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

    mov             r12, #8

variance8x16_neon_loop
    vld1.8          {d0}, [r0], r1              ;Load up source and reference
    vld1.8          {d4}, [r2], r3
    vld1.8          {d2}, [r0], r1
    vld1.8          {d6}, [r2], r3

    vsubl.u8        q11, d0, d4                 ;calculate diff
    vsubl.u8        q12, d2, d6

    vpadal.s16      q8, q11                     ;calculate sum
    vmlal.s16       q9, d22, d22                ;calculate sse
    vmlal.s16       q10, d23, d23

    subs            r12, r12, #1

    vpadal.s16      q8, q12
    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25

    bne             variance8x16_neon_loop

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    ldr             r12, [sp]                   ;load *sse from stack

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [r12]              ;store sse
    vshr.s32        d10, d10, #7
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    bx              lr

    ENDP

;==================================
; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|vp8_variance8x8_neon| PROC
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

    mov             r12, #2

variance8x8_neon_loop
    vld1.8          {d0}, [r0], r1              ;Load up source and reference
    vld1.8          {d4}, [r2], r3
    vld1.8          {d1}, [r0], r1
    vld1.8          {d5}, [r2], r3
    vld1.8          {d2}, [r0], r1
    vld1.8          {d6}, [r2], r3
    vld1.8          {d3}, [r0], r1
    vld1.8          {d7}, [r2], r3

    vsubl.u8        q11, d0, d4                 ;calculate diff
    vsubl.u8        q12, d1, d5
    vsubl.u8        q13, d2, d6
    vsubl.u8        q14, d3, d7

    vpadal.s16      q8, q11                     ;calculate sum
    vmlal.s16       q9, d22, d22                ;calculate sse
    vmlal.s16       q10, d23, d23

    subs            r12, r12, #1

    vpadal.s16      q8, q12
    vmlal.s16       q9, d24, d24
    vmlal.s16       q10, d25, d25
    vpadal.s16      q8, q13
    vmlal.s16       q9, d26, d26
    vmlal.s16       q10, d27, d27
    vpadal.s16      q8, q14
    vmlal.s16       q9, d28, d28
    vmlal.s16       q10, d29, d29

    bne             variance8x8_neon_loop

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    ldr             r12, [sp]                   ;load *sse from stack

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [r12]              ;store sse
    vshr.s32        d10, d10, #6
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    bx              lr

    ENDP

    END
