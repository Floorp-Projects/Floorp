;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_subtract_mby_armv6|
    EXPORT  |vp8_subtract_mbuv_armv6|
    EXPORT  |vp8_subtract_b_armv6|

    INCLUDE asm_enc_offsets.asm

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    BLOCK *be
; r1    BLOCKD *bd
; r2    int pitch
|vp8_subtract_b_armv6| PROC

    stmfd   sp!, {r4-r9}

    ldr     r4, [r0, #vp8_block_base_src]
    ldr     r5, [r0, #vp8_block_src]
    ldr     r6, [r0, #vp8_block_src_diff]

    ldr     r3, [r4]
    ldr     r7, [r0, #vp8_block_src_stride]
    add     r3, r3, r5          ; src = *base_src + src
    ldr     r8, [r1, #vp8_blockd_predictor]

    mov     r9, #4              ; loop count

loop_block

    ldr     r0, [r3], r7        ; src
    ldr     r1, [r8], r2        ; pred

    uxtb16  r4, r0              ; [s2 | s0]
    uxtb16  r5, r1              ; [p2 | p0]
    uxtb16  r0, r0, ror #8      ; [s3 | s1]
    uxtb16  r1, r1, ror #8      ; [p3 | p1]

    usub16  r4, r4, r5          ; [d2 | d0]
    usub16  r5, r0, r1          ; [d3 | d1]

    subs    r9, r9, #1          ; decrement loop counter

    pkhbt   r0, r4, r5, lsl #16 ; [d1 | d0]
    pkhtb   r1, r5, r4, asr #16 ; [d3 | d2]

    str     r0, [r6, #0]        ; diff
    str     r1, [r6, #4]        ; diff

    add     r6, r6, r2, lsl #1  ; update diff pointer
    bne     loop_block

    ldmfd   sp!, {r4-r9}
    mov     pc, lr

    ENDP


; r0    short *diff
; r1    unsigned char *usrc
; r2    unsigned char *vsrc
; r3    int src_stride
; sp    unsigned char *upred
; sp    unsigned char *vpred
; sp    int pred_stride
|vp8_subtract_mbuv_armv6| PROC

    stmfd   sp!, {r4-r11}

    add     r0, r0, #512        ; set *diff point to Cb
    mov     r4, #8              ; loop count
    ldr     r5, [sp, #32]       ; upred
    ldr     r12, [sp, #40]      ; pred_stride

    ; Subtract U block
loop_u
    ldr     r6, [r1]            ; usrc      (A)
    ldr     r7, [r5]            ; upred     (A)

    uxtb16  r8, r6              ; [s2 | s0] (A)
    uxtb16  r9, r7              ; [p2 | p0] (A)
    uxtb16  r10, r6, ror #8     ; [s3 | s1] (A)
    uxtb16  r11, r7, ror #8     ; [p3 | p1] (A)

    usub16  r6, r8, r9          ; [d2 | d0] (A)
    usub16  r7, r10, r11        ; [d3 | d1] (A)

    ldr     r10, [r1, #4]       ; usrc      (B)
    ldr     r11, [r5, #4]       ; upred     (B)

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (A)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (A)

    str     r8, [r0], #4        ; diff      (A)
    uxtb16  r8, r10             ; [s2 | s0] (B)
    str     r9, [r0], #4        ; diff      (A)

    uxtb16  r9, r11             ; [p2 | p0] (B)
    uxtb16  r10, r10, ror #8    ; [s3 | s1] (B)
    uxtb16  r11, r11, ror #8    ; [p3 | p1] (B)

    usub16  r6, r8, r9          ; [d2 | d0] (B)
    usub16  r7, r10, r11        ; [d3 | d1] (B)

    add     r1, r1, r3          ; update usrc pointer
    add     r5, r5, r12         ; update upred pointer

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (B)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (B)

    str     r8, [r0], #4        ; diff      (B)
    subs    r4, r4, #1          ; update loop counter
    str     r9, [r0], #4        ; diff      (B)

    bne     loop_u

    ldr     r5, [sp, #36]       ; vpred
    mov     r4, #8              ; loop count

    ; Subtract V block
loop_v
    ldr     r6, [r2]            ; vsrc      (A)
    ldr     r7, [r5]            ; vpred     (A)

    uxtb16  r8, r6              ; [s2 | s0] (A)
    uxtb16  r9, r7              ; [p2 | p0] (A)
    uxtb16  r10, r6, ror #8     ; [s3 | s1] (A)
    uxtb16  r11, r7, ror #8     ; [p3 | p1] (A)

    usub16  r6, r8, r9          ; [d2 | d0] (A)
    usub16  r7, r10, r11        ; [d3 | d1] (A)

    ldr     r10, [r2, #4]       ; vsrc      (B)
    ldr     r11, [r5, #4]       ; vpred     (B)

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (A)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (A)

    str     r8, [r0], #4        ; diff      (A)
    uxtb16  r8, r10             ; [s2 | s0] (B)
    str     r9, [r0], #4        ; diff      (A)

    uxtb16  r9, r11             ; [p2 | p0] (B)
    uxtb16  r10, r10, ror #8    ; [s3 | s1] (B)
    uxtb16  r11, r11, ror #8    ; [p3 | p1] (B)

    usub16  r6, r8, r9          ; [d2 | d0] (B)
    usub16  r7, r10, r11        ; [d3 | d1] (B)

    add     r2, r2, r3          ; update vsrc pointer
    add     r5, r5, r12         ; update vpred pointer

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (B)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (B)

    str     r8, [r0], #4        ; diff      (B)
    subs    r4, r4, #1          ; update loop counter
    str     r9, [r0], #4        ; diff      (B)

    bne     loop_v

    ldmfd   sp!, {r4-r11}
    bx      lr

    ENDP


; r0    short *diff
; r1    unsigned char *src
; r2    int src_stride
; r3    unsigned char *pred
; sp    int pred_stride
|vp8_subtract_mby_armv6| PROC

    stmfd   sp!, {r4-r11}
    ldr     r12, [sp, #32]      ; pred_stride
    mov     r4, #16
loop
    ldr     r6, [r1]            ; src       (A)
    ldr     r7, [r3]            ; pred      (A)

    uxtb16  r8, r6              ; [s2 | s0] (A)
    uxtb16  r9, r7              ; [p2 | p0] (A)
    uxtb16  r10, r6, ror #8     ; [s3 | s1] (A)
    uxtb16  r11, r7, ror #8     ; [p3 | p1] (A)

    usub16  r6, r8, r9          ; [d2 | d0] (A)
    usub16  r7, r10, r11        ; [d3 | d1] (A)

    ldr     r10, [r1, #4]       ; src       (B)
    ldr     r11, [r3, #4]       ; pred      (B)

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (A)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (A)

    str     r8, [r0], #4        ; diff      (A)
    uxtb16  r8, r10             ; [s2 | s0] (B)
    str     r9, [r0], #4        ; diff      (A)

    uxtb16  r9, r11             ; [p2 | p0] (B)
    uxtb16  r10, r10, ror #8    ; [s3 | s1] (B)
    uxtb16  r11, r11, ror #8    ; [p3 | p1] (B)

    usub16  r6, r8, r9          ; [d2 | d0] (B)
    usub16  r7, r10, r11        ; [d3 | d1] (B)

    ldr     r10, [r1, #8]       ; src       (C)
    ldr     r11, [r3, #8]       ; pred      (C)

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (B)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (B)

    str     r8, [r0], #4        ; diff      (B)
    uxtb16  r8, r10             ; [s2 | s0] (C)
    str     r9, [r0], #4        ; diff      (B)

    uxtb16  r9, r11             ; [p2 | p0] (C)
    uxtb16  r10, r10, ror #8    ; [s3 | s1] (C)
    uxtb16  r11, r11, ror #8    ; [p3 | p1] (C)

    usub16  r6, r8, r9          ; [d2 | d0] (C)
    usub16  r7, r10, r11        ; [d3 | d1] (C)

    ldr     r10, [r1, #12]      ; src       (D)
    ldr     r11, [r3, #12]      ; pred      (D)

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (C)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (C)

    str     r8, [r0], #4        ; diff      (C)
    uxtb16  r8, r10             ; [s2 | s0] (D)
    str     r9, [r0], #4        ; diff      (C)

    uxtb16  r9, r11             ; [p2 | p0] (D)
    uxtb16  r10, r10, ror #8    ; [s3 | s1] (D)
    uxtb16  r11, r11, ror #8    ; [p3 | p1] (D)

    usub16  r6, r8, r9          ; [d2 | d0] (D)
    usub16  r7, r10, r11        ; [d3 | d1] (D)

    add     r1, r1, r2          ; update src pointer
    add     r3, r3, r12         ; update pred pointer

    pkhbt   r8, r6, r7, lsl #16 ; [d1 | d0] (D)
    pkhtb   r9, r7, r6, asr #16 ; [d3 | d2] (D)

    str     r8, [r0], #4        ; diff      (D)
    subs    r4, r4, #1          ; update loop counter
    str     r9, [r0], #4        ; diff      (D)

    bne     loop

    ldmfd   sp!, {r4-r11}
    bx      lr

    ENDP

    END

