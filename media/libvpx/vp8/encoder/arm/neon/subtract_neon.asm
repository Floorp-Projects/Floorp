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

    INCLUDE vp8_asm_enc_offsets.asm

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
;void vp8_subtract_mby_neon(short *diff, unsigned char *src, int src_stride
;                           unsigned char *pred, int pred_stride)
|vp8_subtract_mby_neon| PROC
    push            {r4-r7}
    mov             r12, #4
    ldr             r4, [sp, #16]           ; pred_stride
    mov             r6, #32                 ; "diff" stride x2
    add             r5, r0, #16             ; second diff pointer

subtract_mby_loop
    vld1.8          {q0}, [r1], r2          ;load src
    vld1.8          {q1}, [r3], r4          ;load pred
    vld1.8          {q2}, [r1], r2
    vld1.8          {q3}, [r3], r4
    vld1.8          {q4}, [r1], r2
    vld1.8          {q5}, [r3], r4
    vld1.8          {q6}, [r1], r2
    vld1.8          {q7}, [r3], r4

    vsubl.u8        q8, d0, d2
    vsubl.u8        q9, d1, d3
    vsubl.u8        q10, d4, d6
    vsubl.u8        q11, d5, d7
    vsubl.u8        q12, d8, d10
    vsubl.u8        q13, d9, d11
    vsubl.u8        q14, d12, d14
    vsubl.u8        q15, d13, d15

    vst1.16         {q8}, [r0], r6          ;store diff
    vst1.16         {q9}, [r5], r6
    vst1.16         {q10}, [r0], r6
    vst1.16         {q11}, [r5], r6
    vst1.16         {q12}, [r0], r6
    vst1.16         {q13}, [r5], r6
    vst1.16         {q14}, [r0], r6
    vst1.16         {q15}, [r5], r6

    subs            r12, r12, #1
    bne             subtract_mby_loop

    pop             {r4-r7}
    bx              lr
    ENDP

;=================================
;void vp8_subtract_mbuv_c(short *diff, unsigned char *usrc, unsigned char *vsrc,
;                         int src_stride, unsigned char *upred,
;                         unsigned char *vpred, int pred_stride)

|vp8_subtract_mbuv_neon| PROC
    push            {r4-r7}
    ldr             r4, [sp, #16]       ; upred
    ldr             r5, [sp, #20]       ; vpred
    ldr             r6, [sp, #24]       ; pred_stride
    add             r0, r0, #512        ; short *udiff = diff + 256;
    mov             r12, #32            ; "diff" stride x2
    add             r7, r0, #16         ; second diff pointer

;u
    vld1.8          {d0}, [r1], r3      ;load usrc
    vld1.8          {d1}, [r4], r6      ;load upred
    vld1.8          {d2}, [r1], r3
    vld1.8          {d3}, [r4], r6
    vld1.8          {d4}, [r1], r3
    vld1.8          {d5}, [r4], r6
    vld1.8          {d6}, [r1], r3
    vld1.8          {d7}, [r4], r6
    vld1.8          {d8}, [r1], r3
    vld1.8          {d9}, [r4], r6
    vld1.8          {d10}, [r1], r3
    vld1.8          {d11}, [r4], r6
    vld1.8          {d12}, [r1], r3
    vld1.8          {d13}, [r4], r6
    vld1.8          {d14}, [r1], r3
    vld1.8          {d15}, [r4], r6

    vsubl.u8        q8, d0, d1
    vsubl.u8        q9, d2, d3
    vsubl.u8        q10, d4, d5
    vsubl.u8        q11, d6, d7
    vsubl.u8        q12, d8, d9
    vsubl.u8        q13, d10, d11
    vsubl.u8        q14, d12, d13
    vsubl.u8        q15, d14, d15

    vst1.16         {q8}, [r0], r12     ;store diff
    vst1.16         {q9}, [r7], r12
    vst1.16         {q10}, [r0], r12
    vst1.16         {q11}, [r7], r12
    vst1.16         {q12}, [r0], r12
    vst1.16         {q13}, [r7], r12
    vst1.16         {q14}, [r0], r12
    vst1.16         {q15}, [r7], r12

;v
    vld1.8          {d0}, [r2], r3      ;load vsrc
    vld1.8          {d1}, [r5], r6      ;load vpred
    vld1.8          {d2}, [r2], r3
    vld1.8          {d3}, [r5], r6
    vld1.8          {d4}, [r2], r3
    vld1.8          {d5}, [r5], r6
    vld1.8          {d6}, [r2], r3
    vld1.8          {d7}, [r5], r6
    vld1.8          {d8}, [r2], r3
    vld1.8          {d9}, [r5], r6
    vld1.8          {d10}, [r2], r3
    vld1.8          {d11}, [r5], r6
    vld1.8          {d12}, [r2], r3
    vld1.8          {d13}, [r5], r6
    vld1.8          {d14}, [r2], r3
    vld1.8          {d15}, [r5], r6

    vsubl.u8        q8, d0, d1
    vsubl.u8        q9, d2, d3
    vsubl.u8        q10, d4, d5
    vsubl.u8        q11, d6, d7
    vsubl.u8        q12, d8, d9
    vsubl.u8        q13, d10, d11
    vsubl.u8        q14, d12, d13
    vsubl.u8        q15, d14, d15

    vst1.16         {q8}, [r0], r12     ;store diff
    vst1.16         {q9}, [r7], r12
    vst1.16         {q10}, [r0], r12
    vst1.16         {q11}, [r7], r12
    vst1.16         {q12}, [r0], r12
    vst1.16         {q13}, [r7], r12
    vst1.16         {q14}, [r0], r12
    vst1.16         {q15}, [r7], r12

    pop             {r4-r7}
    bx              lr

    ENDP

    END
