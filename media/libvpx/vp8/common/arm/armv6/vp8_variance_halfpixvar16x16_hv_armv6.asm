;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_variance_halfpixvar16x16_hv_armv6|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|vp8_variance_halfpixvar16x16_hv_armv6| PROC

    stmfd   sp!, {r4-r12, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r8, #0              ; initialize sum = 0
    ldr     r10, c80808080
    mov     r11, #0             ; initialize sse = 0
    mov     r12, #16            ; set loop counter to 16 (=block height)
    mov     lr, #0              ; constant zero
loop
    add     r9, r0, r1          ; pointer to pixels on the next row
    ; 1st 4 pixels
    ldr     r4, [r0, #0]        ; load source pixels a, row N
    ldr     r6, [r0, #1]        ; load source pixels b, row N
    ldr     r5, [r9, #0]        ; load source pixels c, row N+1
    ldr     r7, [r9, #1]        ; load source pixels d, row N+1

    ; x = (a + b + 1) >> 1, interpolate pixels horizontally on row N
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10
    ; y = (c + d + 1) >> 1, interpolate pixels horizontally on row N+1
    mvn     r7, r7
    uhsub8  r5, r5, r7
    eor     r5, r5, r10
    ; z = (x + y + 1) >> 1, interpolate half pixel values vertically
    mvn     r5, r5
    uhsub8  r4, r4, r5
    ldr     r5, [r2, #0]        ; load 4 ref pixels
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
    subs    r8, r8, r5          ; substract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 2nd 4 pixels
    ldr     r4, [r0, #4]        ; load source pixels a, row N
    ldr     r6, [r0, #5]        ; load source pixels b, row N
    ldr     r5, [r9, #4]        ; load source pixels c, row N+1

    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)

    ldr     r7, [r9, #5]        ; load source pixels d, row N+1

    ; x = (a + b + 1) >> 1, interpolate pixels horizontally on row N
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10
    ; y = (c + d + 1) >> 1, interpolate pixels horizontally on row N+1
    mvn     r7, r7
    uhsub8  r5, r5, r7
    eor     r5, r5, r10
    ; z = (x + y + 1) >> 1, interpolate half pixel values vertically
    mvn     r5, r5
    uhsub8  r4, r4, r5
    ldr     r5, [r2, #4]        ; load 4 ref pixels
    eor     r4, r4, r10

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
    sub     r8, r8, r5          ; substract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 3rd 4 pixels
    ldr     r4, [r0, #8]        ; load source pixels a, row N
    ldr     r6, [r0, #9]        ; load source pixels b, row N
    ldr     r5, [r9, #8]        ; load source pixels c, row N+1

    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)

    ldr     r7, [r9, #9]        ; load source pixels d, row N+1

    ; x = (a + b + 1) >> 1, interpolate pixels horizontally on row N
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10
    ; y = (c + d + 1) >> 1, interpolate pixels horizontally on row N+1
    mvn     r7, r7
    uhsub8  r5, r5, r7
    eor     r5, r5, r10
    ; z = (x + y + 1) >> 1, interpolate half pixel values vertically
    mvn     r5, r5
    uhsub8  r4, r4, r5
    ldr     r5, [r2, #8]        ; load 4 ref pixels
    eor     r4, r4, r10

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
    sub     r8, r8, r5          ; substract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 4th 4 pixels
    ldr     r4, [r0, #12]       ; load source pixels a, row N
    ldr     r6, [r0, #13]       ; load source pixels b, row N
    ldr     r5, [r9, #12]       ; load source pixels c, row N+1
    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)
    ldr     r7, [r9, #13]       ; load source pixels d, row N+1

    ; x = (a + b + 1) >> 1, interpolate pixels horizontally on row N
    mvn     r6, r6
    uhsub8  r4, r4, r6
    eor     r4, r4, r10
    ; y = (c + d + 1) >> 1, interpolate pixels horizontally on row N+1
    mvn     r7, r7
    uhsub8  r5, r5, r7
    eor     r5, r5, r10
    ; z = (x + y + 1) >> 1, interpolate half pixel values vertically
    mvn     r5, r5
    uhsub8  r4, r4, r5
    ldr     r5, [r2, #12]       ; load 4 ref pixels
    eor     r4, r4, r10

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
    sub     r8, r8, r5          ; substract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r7, r6, ror #8      ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)
    subs    r12, r12, #1
    smlad   r11, r7, r7, r11    ; dual signed multiply, add and accumulate (2)

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
