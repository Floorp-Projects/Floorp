;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_fast_quantize_b_neon|
    EXPORT  |vp8_fast_quantize_b_pair_neon|

    INCLUDE asm_enc_offsets.asm

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=4

;vp8_fast_quantize_b_pair_neon(BLOCK *b1, BLOCK *b2, BLOCKD *d1, BLOCKD *d2);
|vp8_fast_quantize_b_pair_neon| PROC

    stmfd           sp!, {r4-r9}
    vstmdb          sp!, {q4-q7}

    ldr             r4, [r0, #vp8_block_coeff]
    ldr             r5, [r0, #vp8_block_quant_fast]
    ldr             r6, [r0, #vp8_block_round]

    vld1.16         {q0, q1}, [r4@128]  ; load z

    ldr             r7, [r2, #vp8_blockd_qcoeff]

    vabs.s16        q4, q0              ; calculate x = abs(z)
    vabs.s16        q5, q1

    ;right shift 15 to get sign, all 0 if it is positive, all 1 if it is negative
    vshr.s16        q2, q0, #15         ; sz
    vshr.s16        q3, q1, #15

    vld1.s16        {q6, q7}, [r6@128]  ; load round_ptr [0-15]
    vld1.s16        {q8, q9}, [r5@128]  ; load quant_ptr [0-15]

    ldr             r4, [r1, #vp8_block_coeff]

    vadd.s16        q4, q6              ; x + Round
    vadd.s16        q5, q7

    vld1.16         {q0, q1}, [r4@128]  ; load z2

    vqdmulh.s16     q4, q8              ; y = ((Round+abs(z)) * Quant) >> 16
    vqdmulh.s16     q5, q9

    vabs.s16        q10, q0             ; calculate x2 = abs(z_2)
    vabs.s16        q11, q1
    vshr.s16        q12, q0, #15        ; sz2
    vshr.s16        q13, q1, #15

    ;modify data to have its original sign
    veor.s16        q4, q2              ; y^sz
    veor.s16        q5, q3

    vadd.s16        q10, q6             ; x2 + Round
    vadd.s16        q11, q7

    ldr             r8, [r2, #vp8_blockd_dequant]

    vqdmulh.s16     q10, q8             ; y2 = ((Round+abs(z)) * Quant) >> 16
    vqdmulh.s16     q11, q9

    vshr.s16        q4, #1              ; right shift 1 after vqdmulh
    vshr.s16        q5, #1

    vld1.s16        {q6, q7}, [r8@128]  ;load dequant_ptr[i]

    vsub.s16        q4, q2              ; x1=(y^sz)-sz = (y^sz)-(-1) (2's complement)
    vsub.s16        q5, q3

    vshr.s16        q10, #1             ; right shift 1 after vqdmulh
    vshr.s16        q11, #1

    ldr             r9, [r2, #vp8_blockd_dqcoeff]

    veor.s16        q10, q12            ; y2^sz2
    veor.s16        q11, q13

    vst1.s16        {q4, q5}, [r7]      ; store: qcoeff = x1


    vsub.s16        q10, q12            ; x2=(y^sz)-sz = (y^sz)-(-1) (2's complement)
    vsub.s16        q11, q13

    ldr             r6, [r3, #vp8_blockd_qcoeff]

    vmul.s16        q2, q6, q4          ; x * Dequant
    vmul.s16        q3, q7, q5

    adr             r0, inv_zig_zag     ; load ptr of inverse zigzag table

    vceq.s16        q8, q8              ; set q8 to all 1

    vst1.s16        {q10, q11}, [r6]    ; store: qcoeff = x2

    vmul.s16        q12, q6, q10        ; x2 * Dequant
    vmul.s16        q13, q7, q11

    vld1.16         {q6, q7}, [r0@128]  ; load inverse scan order

    vtst.16         q14, q4, q8         ; now find eob
    vtst.16         q15, q5, q8         ; non-zero element is set to all 1

    vst1.s16        {q2, q3}, [r9]      ; store dqcoeff = x * Dequant

    ldr             r7, [r3, #vp8_blockd_dqcoeff]

    vand            q0, q6, q14         ; get all valid numbers from scan array
    vand            q1, q7, q15

    vst1.s16        {q12, q13}, [r7]    ; store dqcoeff = x * Dequant

    vtst.16         q2, q10, q8         ; now find eob
    vtst.16         q3, q11, q8         ; non-zero element is set to all 1

    vmax.u16        q0, q0, q1          ; find maximum value in q0, q1

    vand            q10, q6, q2         ; get all valid numbers from scan array
    vand            q11, q7, q3
    vmax.u16        q10, q10, q11       ; find maximum value in q10, q11

    vmax.u16        d0, d0, d1
    vmax.u16        d20, d20, d21
    vmovl.u16       q0, d0
    vmovl.u16       q10, d20

    vmax.u32        d0, d0, d1
    vmax.u32        d20, d20, d21
    vpmax.u32       d0, d0, d0
    vpmax.u32       d20, d20, d20

    ldr             r4, [r2, #vp8_blockd_eob]
    ldr             r5, [r3, #vp8_blockd_eob]

    vst1.8          {d0[0]}, [r4]       ; store eob
    vst1.8          {d20[0]}, [r5]      ; store eob

    vldmia          sp!, {q4-q7}
    ldmfd           sp!, {r4-r9}
    bx              lr

    ENDP

;void vp8_fast_quantize_b_c(BLOCK *b, BLOCKD *d)
|vp8_fast_quantize_b_neon| PROC

    stmfd           sp!, {r4-r7}

    ldr             r3, [r0, #vp8_block_coeff]
    ldr             r4, [r0, #vp8_block_quant_fast]
    ldr             r5, [r0, #vp8_block_round]

    vld1.16         {q0, q1}, [r3@128]  ; load z
    vorr.s16        q14, q0, q1         ; check if all zero (step 1)
    ldr             r6, [r1, #vp8_blockd_qcoeff]
    ldr             r7, [r1, #vp8_blockd_dqcoeff]
    vorr.s16        d28, d28, d29       ; check if all zero (step 2)

    vabs.s16        q12, q0             ; calculate x = abs(z)
    vabs.s16        q13, q1

    ;right shift 15 to get sign, all 0 if it is positive, all 1 if it is negative
    vshr.s16        q2, q0, #15         ; sz
    vmov            r2, r3, d28         ; check if all zero (step 3)
    vshr.s16        q3, q1, #15

    vld1.s16        {q14, q15}, [r5@128]; load round_ptr [0-15]
    vld1.s16        {q8, q9}, [r4@128]  ; load quant_ptr [0-15]

    vadd.s16        q12, q14            ; x + Round
    vadd.s16        q13, q15

    adr             r0, inv_zig_zag     ; load ptr of inverse zigzag table

    vqdmulh.s16     q12, q8             ; y = ((Round+abs(z)) * Quant) >> 16
    vqdmulh.s16     q13, q9

    vld1.16         {q10, q11}, [r0@128]; load inverse scan order

    vceq.s16        q8, q8              ; set q8 to all 1

    ldr             r4, [r1, #vp8_blockd_dequant]

    vshr.s16        q12, #1             ; right shift 1 after vqdmulh
    vshr.s16        q13, #1

    ldr             r5, [r1, #vp8_blockd_eob]

    orr             r2, r2, r3          ; check if all zero (step 4)
    cmp             r2, #0              ; check if all zero (step 5)
    beq             zero_output         ; check if all zero (step 6)

    ;modify data to have its original sign
    veor.s16        q12, q2             ; y^sz
    veor.s16        q13, q3

    vsub.s16        q12, q2             ; x1=(y^sz)-sz = (y^sz)-(-1) (2's complement)
    vsub.s16        q13, q3

    vld1.s16        {q2, q3}, [r4@128]  ; load dequant_ptr[i]

    vtst.16         q14, q12, q8        ; now find eob
    vtst.16         q15, q13, q8        ; non-zero element is set to all 1

    vst1.s16        {q12, q13}, [r6@128]; store: qcoeff = x1

    vand            q10, q10, q14       ; get all valid numbers from scan array
    vand            q11, q11, q15


    vmax.u16        q0, q10, q11        ; find maximum value in q0, q1
    vmax.u16        d0, d0, d1
    vmovl.u16       q0, d0

    vmul.s16        q2, q12             ; x * Dequant
    vmul.s16        q3, q13

    vmax.u32        d0, d0, d1
    vpmax.u32       d0, d0, d0

    vst1.s16        {q2, q3}, [r7@128]  ; store dqcoeff = x * Dequant

    vst1.8          {d0[0]}, [r5]       ; store eob

    ldmfd           sp!, {r4-r7}
    bx              lr

zero_output
    strb            r2, [r5]            ; store eob
    vst1.s16        {q0, q1}, [r6@128]  ; qcoeff = 0
    vst1.s16        {q0, q1}, [r7@128]  ; dqcoeff = 0

    ldmfd           sp!, {r4-r7}
    bx              lr

    ENDP

; default inverse zigzag table is defined in vp8/common/entropy.c
    ALIGN 16    ; enable use of @128 bit aligned loads
inv_zig_zag
    DCW 0x0001, 0x0002, 0x0006, 0x0007
    DCW 0x0003, 0x0005, 0x0008, 0x000d
    DCW 0x0004, 0x0009, 0x000c, 0x000e
    DCW 0x000a, 0x000b, 0x000f, 0x0010

    END

