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


    EXPORT  |aom_variance_halfpixvar16x16_h_media|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|aom_variance_halfpixvar16x16_h_media| PROC

    stmfd   sp!, {r4-r12, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r8, #0              ; initialize sum = 0
    ldr     r10, c80808080
    mov     r11, #0             ; initialize sse = 0
    mov     r12, #16            ; set loop counter to 16 (=block height)
    mov     lr, #0              ; constant zero
loop
    ; 1st 4 pixels
    ldr     r4, [r0, #0]        ; load 4 src pixels
    ldr     r6, [r0, #1]        ; load 4 src pixels with 1 byte offset
    ldr     r5, [r2, #0]        ; load 4 ref pixels

    ; bilinear interpolation
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10

    usub8   r6, r4, r5          ; calculate difference
    pld     [r0, r1, lsl #1]
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r6, r5, r4          ; calculate difference with reversed operands
    pld     [r2, r3, lsl #1]
    sel     r6, r6, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels
    ; calculate total sum
    adds    r8, r8, r4          ; add positive differences to sum
    subs    r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 2nd 4 pixels
    ldr     r4, [r0, #4]        ; load 4 src pixels
    ldr     r6, [r0, #5]        ; load 4 src pixels with 1 byte offset
    ldr     r5, [r2, #4]        ; load 4 ref pixels

    ; bilinear interpolation
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10

    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)

    usub8   r6, r4, r5          ; calculate difference
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r6, r5, r4          ; calculate difference with reversed operands
    sel     r6, r6, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels

    ; calculate total sum
    add     r8, r8, r4          ; add positive differences to sum
    sub     r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 3rd 4 pixels
    ldr     r4, [r0, #8]        ; load 4 src pixels
    ldr     r6, [r0, #9]        ; load 4 src pixels with 1 byte offset
    ldr     r5, [r2, #8]        ; load 4 ref pixels

    ; bilinear interpolation
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10

    smlad   r11, r7, r7, r11  ; dual signed multiply, add and accumulate (2)

    usub8   r6, r4, r5          ; calculate difference
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r6, r5, r4          ; calculate difference with reversed operands
    sel     r6, r6, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels

    ; calculate total sum
    add     r8, r8, r4          ; add positive differences to sum
    sub     r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 4th 4 pixels
    ldr     r4, [r0, #12]       ; load 4 src pixels
    ldr     r6, [r0, #13]       ; load 4 src pixels with 1 byte offset
    ldr     r5, [r2, #12]       ; load 4 ref pixels

    ; bilinear interpolation
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10

    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)

    usub8   r6, r4, r5          ; calculate difference
    add     r0, r0, r1          ; set src_ptr to next row
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r6, r5, r4          ; calculate difference with reversed operands
    add     r2, r2, r3          ; set dst_ptr to next row
    sel     r6, r6, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels

    ; calculate total sum
    add     r8, r8, r4          ; add positive differences to sum
    sub     r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)
    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)

    subs    r12, r12, #1

    bne     loop

    ; return stuff
    ldr     r6, [sp, #40]       ; get address of sse
    mul     r0, r8, r8          ; sum * sum
    str     r11, [r6]           ; store sse
    sub     r0, r11, r0, lsr #8 ; return (sse - ((sum * sum) >> 8))

    ldmfd   sp!, {r4-r12, pc}

    ENDP

c80808080
    DCD     0x80808080

    END

