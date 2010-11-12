;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_copy_mem8x4_v6|
    ; ARM
    ; REQUIRE8
    ; PRESERVE8

    AREA    Block, CODE, READONLY ; name this block of code
;void vp8_copy_mem8x4_v6( unsigned char *src, int src_stride, unsigned char *dst, int dst_stride)
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
|vp8_copy_mem8x4_v6| PROC
    ;push   {r4-r5}
    stmdb  sp!, {r4-r5}

    ;preload
    pld     [r0]
    pld     [r0, r1]
    pld     [r0, r1, lsl #1]

    ands    r4, r0, #7
    beq     copy_mem8x4_fast

    ands    r4, r0, #3
    beq     copy_mem8x4_4

    ;copy 1 byte each time
    ldrb    r4, [r0]
    ldrb    r5, [r0, #1]

    mov     r12, #4

copy_mem8x4_1_loop
    strb    r4, [r2]
    strb    r5, [r2, #1]

    ldrb    r4, [r0, #2]
    ldrb    r5, [r0, #3]

    subs    r12, r12, #1

    strb    r4, [r2, #2]
    strb    r5, [r2, #3]

    ldrb    r4, [r0, #4]
    ldrb    r5, [r0, #5]

    strb    r4, [r2, #4]
    strb    r5, [r2, #5]

    ldrb    r4, [r0, #6]
    ldrb    r5, [r0, #7]

    add     r0, r0, r1

    strb    r4, [r2, #6]
    strb    r5, [r2, #7]

    add     r2, r2, r3

    ldrneb  r4, [r0]
    ldrneb  r5, [r0, #1]

    bne     copy_mem8x4_1_loop

    ldmia       sp!, {r4 - r5}
    ;pop        {r4-r5}
    mov     pc, lr

;copy 4 bytes each time
copy_mem8x4_4
    ldr     r4, [r0]
    ldr     r5, [r0, #4]

    mov     r12, #4

copy_mem8x4_4_loop
    subs    r12, r12, #1
    add     r0, r0, r1

    str     r4, [r2]
    str     r5, [r2, #4]

    add     r2, r2, r3

    ldrne   r4, [r0]
    ldrne   r5, [r0, #4]

    bne     copy_mem8x4_4_loop

    ldmia  sp!, {r4-r5}
    ;pop        {r4-r5}
    mov     pc, lr

;copy 8 bytes each time
copy_mem8x4_fast
    ;sub        r1, r1, #8
    ;sub        r3, r3, #8

    mov     r12, #4

copy_mem8x4_fast_loop
    ldmia   r0, {r4-r5}
    ;ldm        r0, {r4-r5}
    add     r0, r0, r1

    subs    r12, r12, #1
    stmia   r2, {r4-r5}
    ;stm        r2, {r4-r5}
    add     r2, r2, r3

    bne     copy_mem8x4_fast_loop

    ldmia  sp!, {r4-r5}
    ;pop        {r4-r5}
    mov     pc, lr

    ENDP  ; |vp8_copy_mem8x4_v6|

    END
