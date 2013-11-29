;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_mse16x16_armv6|

    ARM

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
;
;note: Based on vp8_variance16x16_armv6. In this function, sum is never used.
;      So, we can remove this part of calculation.

|vp8_mse16x16_armv6| PROC

    push    {r4-r9, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r12, #16            ; set loop counter to 16 (=block height)
    mov     r4, #0              ; initialize sse = 0

loop
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

    bne     loop

    ; return stuff
    ldr     r1, [sp, #28]       ; get address of sse
    mov     r0, r4              ; return sse
    str     r4, [r1]            ; store sse

    pop     {r4-r9, pc}

    ENDP

    END
