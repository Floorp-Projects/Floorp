;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_copy_mem16x16_v6|
    ; ARM
    ; REQUIRE8
    ; PRESERVE8

    AREA    Block, CODE, READONLY ; name this block of code
;void copy_mem16x16_v6( unsigned char *src, int src_stride, unsigned char *dst, int dst_stride)
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
|vp8_copy_mem16x16_v6| PROC
    stmdb       sp!, {r4 - r7}
    ;push   {r4-r7}

    ;preload
    pld     [r0]
    pld     [r0, r1]
    pld     [r0, r1, lsl #1]

    ands    r4, r0, #15
    beq     copy_mem16x16_fast

    ands    r4, r0, #7
    beq     copy_mem16x16_8

    ands    r4, r0, #3
    beq     copy_mem16x16_4

    ;copy one byte each time
    ldrb    r4, [r0]
    ldrb    r5, [r0, #1]
    ldrb    r6, [r0, #2]
    ldrb    r7, [r0, #3]

    mov     r12, #16

copy_mem16x16_1_loop
    strb    r4, [r2]
    strb    r5, [r2, #1]
    strb    r6, [r2, #2]
    strb    r7, [r2, #3]

    ldrb    r4, [r0, #4]
    ldrb    r5, [r0, #5]
    ldrb    r6, [r0, #6]
    ldrb    r7, [r0, #7]

    subs    r12, r12, #1

    strb    r4, [r2, #4]
    strb    r5, [r2, #5]
    strb    r6, [r2, #6]
    strb    r7, [r2, #7]

    ldrb    r4, [r0, #8]
    ldrb    r5, [r0, #9]
    ldrb    r6, [r0, #10]
    ldrb    r7, [r0, #11]

    strb    r4, [r2, #8]
    strb    r5, [r2, #9]
    strb    r6, [r2, #10]
    strb    r7, [r2, #11]

    ldrb    r4, [r0, #12]
    ldrb    r5, [r0, #13]
    ldrb    r6, [r0, #14]
    ldrb    r7, [r0, #15]

    add     r0, r0, r1

    strb    r4, [r2, #12]
    strb    r5, [r2, #13]
    strb    r6, [r2, #14]
    strb    r7, [r2, #15]

    add     r2, r2, r3

    ldrneb  r4, [r0]
    ldrneb  r5, [r0, #1]
    ldrneb  r6, [r0, #2]
    ldrneb  r7, [r0, #3]

    bne     copy_mem16x16_1_loop

    ldmia       sp!, {r4 - r7}
    ;pop        {r4-r7}
    mov     pc, lr

;copy 4 bytes each time
copy_mem16x16_4
    ldr     r4, [r0]
    ldr     r5, [r0, #4]
    ldr     r6, [r0, #8]
    ldr     r7, [r0, #12]

    mov     r12, #16

copy_mem16x16_4_loop
    subs    r12, r12, #1
    add     r0, r0, r1

    str     r4, [r2]
    str     r5, [r2, #4]
    str     r6, [r2, #8]
    str     r7, [r2, #12]

    add     r2, r2, r3

    ldrne   r4, [r0]
    ldrne   r5, [r0, #4]
    ldrne   r6, [r0, #8]
    ldrne   r7, [r0, #12]

    bne     copy_mem16x16_4_loop

    ldmia       sp!, {r4 - r7}
    ;pop        {r4-r7}
    mov     pc, lr

;copy 8 bytes each time
copy_mem16x16_8
    sub     r1, r1, #16
    sub     r3, r3, #16

    mov     r12, #16

copy_mem16x16_8_loop
    ldmia   r0!, {r4-r5}
    ;ldm        r0, {r4-r5}
    ldmia   r0!, {r6-r7}

    add     r0, r0, r1

    stmia   r2!, {r4-r5}
    subs    r12, r12, #1
    ;stm        r2, {r4-r5}
    stmia   r2!, {r6-r7}

    add     r2, r2, r3

    bne     copy_mem16x16_8_loop

    ldmia       sp!, {r4 - r7}
    ;pop        {r4-r7}
    mov     pc, lr

;copy 16 bytes each time
copy_mem16x16_fast
    ;sub        r1, r1, #16
    ;sub        r3, r3, #16

    mov     r12, #16

copy_mem16x16_fast_loop
    ldmia   r0, {r4-r7}
    ;ldm        r0, {r4-r7}
    add     r0, r0, r1

    subs    r12, r12, #1
    stmia   r2, {r4-r7}
    ;stm        r2, {r4-r7}
    add     r2, r2, r3

    bne     copy_mem16x16_fast_loop

    ldmia       sp!, {r4 - r7}
    ;pop        {r4-r7}
    mov     pc, lr

    ENDP  ; |vp8_copy_mem16x16_v6|

    END
