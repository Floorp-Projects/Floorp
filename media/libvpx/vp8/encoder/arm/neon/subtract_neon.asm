;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT |vp8_subtract_b_neon|
    EXPORT |vp8_subtract_mby_neon|
    EXPORT |vp8_subtract_mbuv_neon|

    INCLUDE asm_enc_offsets.asm

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void vp8_subtract_b_neon(BLOCK *be, BLOCKD *bd, int pitch)
|vp8_subtract_b_neon| PROC

    stmfd   sp!, {r4-r7}

    ldr     r3, [r0, #vp8_block_base_src]
    ldr     r4, [r0, #vp8_block_src]
    ldr     r5, [r0, #vp8_block_src_diff]
    ldr     r3, [r3]
    ldr     r6, [r0, #vp8_block_src_stride]
    add     r3, r3, r4                      ; src = *base_src + src
    ldr     r7, [r1, #vp8_blockd_predictor]

    vld1.8          {d0}, [r3], r6          ;load src
    vld1.8          {d1}, [r7], r2          ;load pred
    vld1.8          {d2}, [r3], r6
    vld1.8          {d3}, [r7], r2
    vld1.8          {d4}, [r3], r6
    vld1.8          {d5}, [r7], r2
    vld1.8          {d6}, [r3], r6
    vld1.8          {d7}, [r7], r2

    vsubl.u8        q10, d0, d1
    vsubl.u8        q11, d2, d3
    vsubl.u8        q12, d4, d5
    vsubl.u8        q13, d6, d7

    mov             r2, r2, lsl #1

    vst1.16         {d20}, [r5], r2         ;store diff
    vst1.16         {d22}, [r5], r2
    vst1.16         {d24}, [r5], r2
    vst1.16         {d26}, [r5], r2

    ldmfd   sp!, {r4-r7}
    bx              lr

    ENDP


;==========================================
;void vp8_subtract_mby_neon(short *diff, unsigned char *src, unsigned char *pred, int stride)
|vp8_subtract_mby_neon| PROC
    mov             r12, #4

subtract_mby_loop
    vld1.8          {q0}, [r1], r3          ;load src
    vld1.8          {q1}, [r2]!             ;load pred
    vld1.8          {q2}, [r1], r3
    vld1.8          {q3}, [r2]!
    vld1.8          {q4}, [r1], r3
    vld1.8          {q5}, [r2]!
    vld1.8          {q6}, [r1], r3
    vld1.8          {q7}, [r2]!

    vsubl.u8        q8, d0, d2
    vsubl.u8        q9, d1, d3
    vsubl.u8        q10, d4, d6
    vsubl.u8        q11, d5, d7
    vsubl.u8        q12, d8, d10
    vsubl.u8        q13, d9, d11
    vsubl.u8        q14, d12, d14
    vsubl.u8        q15, d13, d15

    vst1.16         {q8}, [r0]!             ;store diff
    vst1.16         {q9}, [r0]!
    vst1.16         {q10}, [r0]!
    vst1.16         {q11}, [r0]!
    vst1.16         {q12}, [r0]!
    vst1.16         {q13}, [r0]!
    vst1.16         {q14}, [r0]!
    vst1.16         {q15}, [r0]!

    subs            r12, r12, #1
    bne             subtract_mby_loop

    bx              lr
    ENDP

;=================================
;void vp8_subtract_mbuv_neon(short *diff, unsigned char *usrc, unsigned char *vsrc, unsigned char *pred, int stride)
|vp8_subtract_mbuv_neon| PROC
    ldr             r12, [sp]

;u
    add             r0, r0, #512        ;   short *udiff = diff + 256;
    add             r3, r3, #256        ;   unsigned char *upred = pred + 256;

    vld1.8          {d0}, [r1], r12         ;load src
    vld1.8          {d1}, [r3]!             ;load pred
    vld1.8          {d2}, [r1], r12
    vld1.8          {d3}, [r3]!
    vld1.8          {d4}, [r1], r12
    vld1.8          {d5}, [r3]!
    vld1.8          {d6}, [r1], r12
    vld1.8          {d7}, [r3]!
    vld1.8          {d8}, [r1], r12
    vld1.8          {d9}, [r3]!
    vld1.8          {d10}, [r1], r12
    vld1.8          {d11}, [r3]!
    vld1.8          {d12}, [r1], r12
    vld1.8          {d13}, [r3]!
    vld1.8          {d14}, [r1], r12
    vld1.8          {d15}, [r3]!

    vsubl.u8        q8, d0, d1
    vsubl.u8        q9, d2, d3
    vsubl.u8        q10, d4, d5
    vsubl.u8        q11, d6, d7
    vsubl.u8        q12, d8, d9
    vsubl.u8        q13, d10, d11
    vsubl.u8        q14, d12, d13
    vsubl.u8        q15, d14, d15

    vst1.16         {q8}, [r0]!             ;store diff
    vst1.16         {q9}, [r0]!
    vst1.16         {q10}, [r0]!
    vst1.16         {q11}, [r0]!
    vst1.16         {q12}, [r0]!
    vst1.16         {q13}, [r0]!
    vst1.16         {q14}, [r0]!
    vst1.16         {q15}, [r0]!

;v
    vld1.8          {d0}, [r2], r12         ;load src
    vld1.8          {d1}, [r3]!             ;load pred
    vld1.8          {d2}, [r2], r12
    vld1.8          {d3}, [r3]!
    vld1.8          {d4}, [r2], r12
    vld1.8          {d5}, [r3]!
    vld1.8          {d6}, [r2], r12
    vld1.8          {d7}, [r3]!
    vld1.8          {d8}, [r2], r12
    vld1.8          {d9}, [r3]!
    vld1.8          {d10}, [r2], r12
    vld1.8          {d11}, [r3]!
    vld1.8          {d12}, [r2], r12
    vld1.8          {d13}, [r3]!
    vld1.8          {d14}, [r2], r12
    vld1.8          {d15}, [r3]!

    vsubl.u8        q8, d0, d1
    vsubl.u8        q9, d2, d3
    vsubl.u8        q10, d4, d5
    vsubl.u8        q11, d6, d7
    vsubl.u8        q12, d8, d9
    vsubl.u8        q13, d10, d11
    vsubl.u8        q14, d12, d13
    vsubl.u8        q15, d14, d15

    vst1.16         {q8}, [r0]!             ;store diff
    vst1.16         {q9}, [r0]!
    vst1.16         {q10}, [r0]!
    vst1.16         {q11}, [r0]!
    vst1.16         {q12}, [r0]!
    vst1.16         {q13}, [r0]!
    vst1.16         {q14}, [r0]!
    vst1.16         {q15}, [r0]!

    bx              lr
    ENDP

    END
