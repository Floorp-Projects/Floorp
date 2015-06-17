;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_variance8x8_armv6|

    ARM

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src_ptr
; r1    int source_stride
; r2    unsigned char *ref_ptr
; r3    int  recon_stride
; stack unsigned int *sse
|vp8_variance8x8_armv6| PROC

    push    {r4-r10, lr}

    pld     [r0, r1, lsl #0]
    pld     [r2, r3, lsl #0]

    mov     r12, #8             ; set loop counter to 8 (=block height)
    mov     r4, #0              ; initialize sum = 0
    mov     r5, #0              ; initialize sse = 0

loop
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

    bne     loop

    ; return stuff
    ldr     r8, [sp, #32]       ; get address of sse
    mul     r1, r4, r4          ; sum * sum
    str     r5, [r8]            ; store sse
    sub     r0, r5, r1, ASR #6  ; return (sse - ((sum * sum) >> 6))

    pop     {r4-r10, pc}

    ENDP

    END
