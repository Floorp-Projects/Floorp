;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license and patent
;  grant that can be found in the LICENSE file in the root of the source
;  tree. All contributing project authors may be found in the AUTHORS
;  file in the root of the source tree.
;

    EXPORT |vp8_dequant_idct_add_v6|

    AREA |.text|, CODE, READONLY
;void vp8_dequant_idct_v6(short *input, short *dq,
;                         unsigned char *dest, int stride)
; r0 = q
; r1 = dq
; r2 = dst
; r3 = stride

|vp8_dequant_idct_add_v6| PROC
    stmdb   sp!, {r4-r11, lr}

    ldr     r4, [r0]                ;input
    ldr     r5, [r1], #4            ;dq

    sub     sp, sp, #4
    str     r3, [sp]

    mov     r12, #4

vp8_dequant_add_loop
    smulbb  r6, r4, r5
    smultt  r7, r4, r5

    ldr     r4, [r0, #4]            ;input
    ldr     r5, [r1], #4            ;dq

    strh    r6, [r0], #2
    strh    r7, [r0], #2

    smulbb  r6, r4, r5
    smultt  r7, r4, r5

    subs    r12, r12, #1

    ldrne   r4, [r0, #4]
    ldrne   r5, [r1], #4

    strh    r6, [r0], #2
    strh    r7, [r0], #2

    bne     vp8_dequant_add_loop

    sub     r0, r0, #32
    mov     r1, r0

; short_idct4x4llm_v6_dual
    ldr     r3, cospi8sqrt2minus1
    ldr     r4, sinpi8sqrt2
    ldr     r6, [r0, #8]
    mov     r5, #2
vp8_dequant_idct_loop1_v6
    ldr     r12, [r0, #24]
    ldr     r14, [r0, #16]
    smulwt  r9, r3, r6
    smulwb  r7, r3, r6
    smulwt  r10, r4, r6
    smulwb  r8, r4, r6
    pkhbt   r7, r7, r9, lsl #16
    smulwt  r11, r3, r12
    pkhbt   r8, r8, r10, lsl #16
    uadd16  r6, r6, r7
    smulwt  r7, r4, r12
    smulwb  r9, r3, r12
    smulwb  r10, r4, r12
    subs    r5, r5, #1
    pkhbt   r9, r9, r11, lsl #16
    ldr     r11, [r0], #4
    pkhbt   r10, r10, r7, lsl #16
    uadd16  r7, r12, r9
    usub16  r7, r8, r7
    uadd16  r6, r6, r10
    uadd16  r10, r11, r14
    usub16  r8, r11, r14
    uadd16  r9, r10, r6
    usub16  r10, r10, r6
    uadd16  r6, r8, r7
    usub16  r7, r8, r7
    str     r6, [r1, #8]
    ldrne   r6, [r0, #8]
    str     r7, [r1, #16]
    str     r10, [r1, #24]
    str     r9, [r1], #4
    bne     vp8_dequant_idct_loop1_v6

    mov     r5, #2
    sub     r0, r1, #8
vp8_dequant_idct_loop2_v6
    ldr     r6, [r0], #4
    ldr     r7, [r0], #4
    ldr     r8, [r0], #4
    ldr     r9, [r0], #4
    smulwt  r1, r3, r6
    smulwt  r12, r4, r6
    smulwt  lr, r3, r8
    smulwt  r10, r4, r8
    pkhbt   r11, r8, r6, lsl #16
    pkhbt   r1, lr, r1, lsl #16
    pkhbt   r12, r10, r12, lsl #16
    pkhtb   r6, r6, r8, asr #16
    uadd16  r6, r1, r6
    pkhbt   lr, r9, r7, lsl #16
    uadd16  r10, r11, lr
    usub16  lr, r11, lr
    pkhtb   r8, r7, r9, asr #16
    subs    r5, r5, #1
    smulwt  r1, r3, r8
    smulwb  r7, r3, r8
    smulwt  r11, r4, r8
    smulwb  r9, r4, r8
    pkhbt   r1, r7, r1, lsl #16
    uadd16  r8, r1, r8
    pkhbt   r11, r9, r11, lsl #16
    usub16  r1, r12, r8
    uadd16  r8, r11, r6
    ldr     r9, c0x00040004
    ldr     r12, [sp]               ; get stride from stack
    uadd16  r6, r10, r8
    usub16  r7, r10, r8
    uadd16  r7, r7, r9
    uadd16  r6, r6, r9
    uadd16  r10, r14, r1
    usub16  r1, r14, r1
    uadd16  r10, r10, r9
    uadd16  r1, r1, r9
    ldr     r11, [r2]               ; load input from dst
    mov     r8, r7, asr #3
    pkhtb   r9, r8, r10, asr #19
    mov     r8, r1, asr #3
    pkhtb   r8, r8, r6, asr #19
    uxtb16  lr, r11, ror #8
    qadd16  r9, r9, lr
    uxtb16  lr, r11
    qadd16  r8, r8, lr
    usat16  r9, #8, r9
    usat16  r8, #8, r8
    orr     r9, r8, r9, lsl #8
    ldr     r11, [r2, r12]          ; load input from dst
    mov     r7, r7, lsl #16
    mov     r1, r1, lsl #16
    mov     r10, r10, lsl #16
    mov     r6, r6, lsl #16
    mov     r7, r7, asr #3
    pkhtb   r7, r7, r10, asr #19
    mov     r1, r1, asr #3
    pkhtb   r1, r1, r6, asr #19
    uxtb16  r8, r11, ror #8
    qadd16  r7, r7, r8
    uxtb16  r8, r11
    qadd16  r1, r1, r8
    usat16  r7, #8, r7
    usat16  r1, #8, r1
    orr     r1, r1, r7, lsl #8
    str     r9, [r2], r12           ; store output to dst
    str     r1, [r2], r12           ; store output to dst
    bne     vp8_dequant_idct_loop2_v6

; memset
    sub     r0, r0, #32
    add     sp, sp, #4

    mov     r12, #0
    str     r12, [r0]
    str     r12, [r0, #4]
    str     r12, [r0, #8]
    str     r12, [r0, #12]
    str     r12, [r0, #16]
    str     r12, [r0, #20]
    str     r12, [r0, #24]
    str     r12, [r0, #28]

    ldmia   sp!, {r4 - r11, pc}
    ENDP    ; |vp8_dequant_idct_add_v6|

; Constant Pool
cospi8sqrt2minus1 DCD 0x00004E7B
sinpi8sqrt2       DCD 0x00008A8C
c0x00040004       DCD 0x00040004

    END
