;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vpx_variance16x16_media|
    EXPORT  |vpx_variance8x8_media|
    EXPORT  |vpx_mse16x16_media|

    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|vpx_variance16x16_media| PROC

    stmfd   sp!, {r4-r12, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r8, #0              ; initialize sum = 0
    mov     r11, #0             ; initialize sse = 0
    mov     r12, #16            ; set loop counter to 16 (=block height)

loop16x16
    ; 1st 4 pixels
    ldr     r4, [r0, #0]        ; load 4 src pixels
    ldr     r5, [r2, #0]        ; load 4 ref pixels

    mov     lr, #0              ; constant zero

    usub8   r6, r4, r5          ; calculate difference
    pld     [r0, r1, lsl #1]
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r9, r5, r4          ; calculate difference with reversed operands
    pld     [r2, r3, lsl #1]
    sel     r6, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels
    ; calculate total sum
    adds    r8, r8, r4          ; add positive differences to sum
    subs    r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r10, r6, ror #8     ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 2nd 4 pixels
    ldr     r4, [r0, #4]        ; load 4 src pixels
    ldr     r5, [r2, #4]        ; load 4 ref pixels
    smlad   r11, r10, r10, r11  ; dual signed multiply, add and accumulate (2)

    usub8   r6, r4, r5          ; calculate difference
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r9, r5, r4          ; calculate difference with reversed operands
    sel     r6, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels

    ; calculate total sum
    add     r8, r8, r4          ; add positive differences to sum
    sub     r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r10, r6, ror #8     ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 3rd 4 pixels
    ldr     r4, [r0, #8]        ; load 4 src pixels
    ldr     r5, [r2, #8]        ; load 4 ref pixels
    smlad   r11, r10, r10, r11  ; dual signed multiply, add and accumulate (2)

    usub8   r6, r4, r5          ; calculate difference
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r9, r5, r4          ; calculate difference with reversed operands
    sel     r6, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels

    ; calculate total sum
    add     r8, r8, r4          ; add positive differences to sum
    sub     r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r10, r6, ror #8     ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)

    ; 4th 4 pixels
    ldr     r4, [r0, #12]       ; load 4 src pixels
    ldr     r5, [r2, #12]       ; load 4 ref pixels
    smlad   r11, r10, r10, r11  ; dual signed multiply, add and accumulate (2)

    usub8   r6, r4, r5          ; calculate difference
    add     r0, r0, r1          ; set src_ptr to next row
    sel     r7, r6, lr          ; select bytes with positive difference
    usub8   r9, r5, r4          ; calculate difference with reversed operands
    add     r2, r2, r3          ; set dst_ptr to next row
    sel     r6, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r4, r7, lr          ; calculate sum of positive differences
    usad8   r5, r6, lr          ; calculate sum of negative differences
    orr     r6, r6, r7          ; differences of all 4 pixels

    ; calculate total sum
    add     r8, r8, r4          ; add positive differences to sum
    sub     r8, r8, r5          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r5, r6              ; byte (two pixels) to halfwords
    uxtb16  r10, r6, ror #8     ; another two pixels to halfwords
    smlad   r11, r5, r5, r11    ; dual signed multiply, add and accumulate (1)
    smlad   r11, r10, r10, r11  ; dual signed multiply, add and accumulate (2)


    subs    r12, r12, #1

    bne     loop16x16

    ; return stuff
    ldr     r6, [sp, #40]       ; get address of sse
    mul     r0, r8, r8          ; sum * sum
    str     r11, [r6]           ; store sse
    sub     r0, r11, r0, lsr #8 ; return (sse - ((sum * sum) >> 8))

    ldmfd   sp!, {r4-r12, pc}

    ENDP

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|vpx_variance8x8_media| PROC

    push    {r4-r10, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r12, #8             ; set loop counter to 8 (=block height)
    mov     r4, #0              ; initialize sum = 0
    mov     r5, #0              ; initialize sse = 0

loop8x8
    ; 1st 4 pixels
    ldr     r6, [r0, #0x0]      ; load 4 src pixels
    ldr     r7, [r2, #0x0]      ; load 4 ref pixels

    mov     lr, #0              ; constant zero

    usub8   r8, r6, r7          ; calculate difference
    pld     [r0, r1, lsl #1]
    sel     r10, r8, lr         ; select bytes with positive difference
    usub8   r9, r7, r6          ; calculate difference with reversed operands
    pld     [r2, r3, lsl #1]
    sel     r8, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r6, r10, lr         ; calculate sum of positive differences
    usad8   r7, r8, lr          ; calculate sum of negative differences
    orr     r8, r8, r10         ; differences of all 4 pixels
    ; calculate total sum
    add    r4, r4, r6           ; add positive differences to sum
    sub    r4, r4, r7           ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r7, r8              ; byte (two pixels) to halfwords
    uxtb16  r10, r8, ror #8     ; another two pixels to halfwords
    smlad   r5, r7, r7, r5      ; dual signed multiply, add and accumulate (1)

    ; 2nd 4 pixels
    ldr     r6, [r0, #0x4]      ; load 4 src pixels
    ldr     r7, [r2, #0x4]      ; load 4 ref pixels
    smlad   r5, r10, r10, r5    ; dual signed multiply, add and accumulate (2)

    usub8   r8, r6, r7          ; calculate difference
    add     r0, r0, r1          ; set src_ptr to next row
    sel     r10, r8, lr         ; select bytes with positive difference
    usub8   r9, r7, r6          ; calculate difference with reversed operands
    add     r2, r2, r3          ; set dst_ptr to next row
    sel     r8, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r6, r10, lr         ; calculate sum of positive differences
    usad8   r7, r8, lr          ; calculate sum of negative differences
    orr     r8, r8, r10         ; differences of all 4 pixels

    ; calculate total sum
    add     r4, r4, r6          ; add positive differences to sum
    sub     r4, r4, r7          ; subtract negative differences from sum

    ; calculate sse
    uxtb16  r7, r8              ; byte (two pixels) to halfwords
    uxtb16  r10, r8, ror #8     ; another two pixels to halfwords
    smlad   r5, r7, r7, r5      ; dual signed multiply, add and accumulate (1)
    subs    r12, r12, #1        ; next row
    smlad   r5, r10, r10, r5    ; dual signed multiply, add and accumulate (2)

    bne     loop8x8

    ; return stuff
    ldr     r8, [sp, #32]       ; get address of sse
    mul     r1, r4, r4          ; sum * sum
    str     r5, [r8]            ; store sse
    sub     r0, r5, r1, ASR #6  ; return (sse - ((sum * sum) >> 6))

    pop     {r4-r10, pc}

    ENDP

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
;
;note: Based on vpx_variance16x16_media. In this function, sum is never used.
;      So, we can remove this part of calculation.

|vpx_mse16x16_media| PROC

    push    {r4-r9, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r12, #16            ; set loop counter to 16 (=block height)
    mov     r4, #0              ; initialize sse = 0

loopmse
    ; 1st 4 pixels
    ldr     r5, [r0, #0x0]      ; load 4 src pixels
    ldr     r6, [r2, #0x0]      ; load 4 ref pixels

    mov     lr, #0              ; constant zero

    usub8   r8, r5, r6          ; calculate difference
    pld     [r0, r1, lsl #1]
    sel     r7, r8, lr          ; select bytes with positive difference
    usub8   r9, r6, r5          ; calculate difference with reversed operands
    pld     [r2, r3, lsl #1]
    sel     r8, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r5, r7, lr          ; calculate sum of positive differences
    usad8   r6, r8, lr          ; calculate sum of negative differences
    orr     r8, r8, r7          ; differences of all 4 pixels

    ldr     r5, [r0, #0x4]      ; load 4 src pixels

    ; calculate sse
    uxtb16  r6, r8              ; byte (two pixels) to halfwords
    uxtb16  r7, r8, ror #8      ; another two pixels to halfwords
    smlad   r4, r6, r6, r4      ; dual signed multiply, add and accumulate (1)

    ; 2nd 4 pixels
    ldr     r6, [r2, #0x4]      ; load 4 ref pixels
    smlad   r4, r7, r7, r4      ; dual signed multiply, add and accumulate (2)

    usub8   r8, r5, r6          ; calculate difference
    sel     r7, r8, lr          ; select bytes with positive difference
    usub8   r9, r6, r5          ; calculate difference with reversed operands
    sel     r8, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r5, r7, lr          ; calculate sum of positive differences
    usad8   r6, r8, lr          ; calculate sum of negative differences
    orr     r8, r8, r7          ; differences of all 4 pixels
    ldr     r5, [r0, #0x8]      ; load 4 src pixels
    ; calculate sse
    uxtb16  r6, r8              ; byte (two pixels) to halfwords
    uxtb16  r7, r8, ror #8      ; another two pixels to halfwords
    smlad   r4, r6, r6, r4      ; dual signed multiply, add and accumulate (1)

    ; 3rd 4 pixels
    ldr     r6, [r2, #0x8]      ; load 4 ref pixels
    smlad   r4, r7, r7, r4      ; dual signed multiply, add and accumulate (2)

    usub8   r8, r5, r6          ; calculate difference
    sel     r7, r8, lr          ; select bytes with positive difference
    usub8   r9, r6, r5          ; calculate difference with reversed operands
    sel     r8, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r5, r7, lr          ; calculate sum of positive differences
    usad8   r6, r8, lr          ; calculate sum of negative differences
    orr     r8, r8, r7          ; differences of all 4 pixels

    ldr     r5, [r0, #0xc]      ; load 4 src pixels

    ; calculate sse
    uxtb16  r6, r8              ; byte (two pixels) to halfwords
    uxtb16  r7, r8, ror #8      ; another two pixels to halfwords
    smlad   r4, r6, r6, r4      ; dual signed multiply, add and accumulate (1)

    ; 4th 4 pixels
    ldr     r6, [r2, #0xc]      ; load 4 ref pixels
    smlad   r4, r7, r7, r4      ; dual signed multiply, add and accumulate (2)

    usub8   r8, r5, r6          ; calculate difference
    add     r0, r0, r1          ; set src_ptr to next row
    sel     r7, r8, lr          ; select bytes with positive difference
    usub8   r9, r6, r5          ; calculate difference with reversed operands
    add     r2, r2, r3          ; set dst_ptr to next row
    sel     r8, r9, lr          ; select bytes with negative difference

    ; calculate partial sums
    usad8   r5, r7, lr          ; calculate sum of positive differences
    usad8   r6, r8, lr          ; calculate sum of negative differences
    orr     r8, r8, r7          ; differences of all 4 pixels

    subs    r12, r12, #1        ; next row

    ; calculate sse
    uxtb16  r6, r8              ; byte (two pixels) to halfwords
    uxtb16  r7, r8, ror #8      ; another two pixels to halfwords
    smlad   r4, r6, r6, r4      ; dual signed multiply, add and accumulate (1)
    smlad   r4, r7, r7, r4      ; dual signed multiply, add and accumulate (2)

    bne     loopmse

    ; return stuff
    ldr     r1, [sp, #28]       ; get address of sse
    mov     r0, r4              ; return sse
    str     r4, [r1]            ; store sse

    pop     {r4-r9, pc}

    ENDP

    END
