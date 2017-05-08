;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;


    EXPORT  |aom_sad16x16_media|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    const unsigned char *src_ptr
; r1    int  src_stride
; r2    const unsigned char *ref_ptr
; r3    int  ref_stride
|aom_sad16x16_media| PROC
    stmfd   sp!, {r4-r12, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]
    pld     [r0, r1, lsl #1]
    pld     [r2, r3, lsl #1]

    mov     r4, #0              ; sad = 0;
    mov     r5, #8              ; loop count

loop
    ; 1st row
    ldr     r6, [r0, #0x0]      ; load 4 src pixels (1A)
    ldr     r8, [r2, #0x0]      ; load 4 ref pixels (1A)
    ldr     r7, [r0, #0x4]      ; load 4 src pixels (1A)
    ldr     r9, [r2, #0x4]      ; load 4 ref pixels (1A)
    ldr     r10, [r0, #0x8]     ; load 4 src pixels (1B)
    ldr     r11, [r0, #0xC]     ; load 4 src pixels (1B)

    usada8  r4, r8, r6, r4      ; calculate sad for 4 pixels
    usad8   r8, r7, r9          ; calculate sad for 4 pixels

    ldr     r12, [r2, #0x8]     ; load 4 ref pixels (1B)
    ldr     lr, [r2, #0xC]      ; load 4 ref pixels (1B)

    add     r0, r0, r1          ; set src pointer to next row
    add     r2, r2, r3          ; set dst pointer to next row

    pld     [r0, r1, lsl #1]
    pld     [r2, r3, lsl #1]

    usada8  r4, r10, r12, r4    ; calculate sad for 4 pixels
    usada8  r8, r11, lr, r8     ; calculate sad for 4 pixels

    ldr     r6, [r0, #0x0]      ; load 4 src pixels (2A)
    ldr     r7, [r0, #0x4]      ; load 4 src pixels (2A)
    add     r4, r4, r8          ; add partial sad values

    ; 2nd row
    ldr     r8, [r2, #0x0]      ; load 4 ref pixels (2A)
    ldr     r9, [r2, #0x4]      ; load 4 ref pixels (2A)
    ldr     r10, [r0, #0x8]     ; load 4 src pixels (2B)
    ldr     r11, [r0, #0xC]     ; load 4 src pixels (2B)

    usada8  r4, r6, r8, r4      ; calculate sad for 4 pixels
    usad8   r8, r7, r9          ; calculate sad for 4 pixels

    ldr     r12, [r2, #0x8]     ; load 4 ref pixels (2B)
    ldr     lr, [r2, #0xC]      ; load 4 ref pixels (2B)

    add     r0, r0, r1          ; set src pointer to next row
    add     r2, r2, r3          ; set dst pointer to next row

    usada8  r4, r10, r12, r4    ; calculate sad for 4 pixels
    usada8  r8, r11, lr, r8     ; calculate sad for 4 pixels

    pld     [r0, r1, lsl #1]
    pld     [r2, r3, lsl #1]

    subs    r5, r5, #1          ; decrement loop counter
    add     r4, r4, r8          ; add partial sad values

    bne     loop

    mov     r0, r4              ; return sad
    ldmfd   sp!, {r4-r12, pc}

    ENDP

    END

