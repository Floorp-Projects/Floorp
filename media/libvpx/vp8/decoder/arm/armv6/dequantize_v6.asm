;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_dequantize_b_loop_v6|

    AREA    |.text|, CODE, READONLY  ; name this block of code
;-------------------------------
;void   vp8_dequantize_b_loop_v6(short *Q, short *DQC, short *DQ);
; r0    short *Q,
; r1    short *DQC
; r2    short *DQ
|vp8_dequantize_b_loop_v6| PROC
    stmdb   sp!, {r4-r9, lr}

    ldr     r3, [r0]                ;load Q
    ldr     r4, [r1]                ;load DQC
    ldr     r5, [r0, #4]
    ldr     r6, [r1, #4]

    mov     r12, #2                 ;loop counter

dequant_loop
    smulbb  r7, r3, r4              ;multiply
    smultt  r8, r3, r4
    smulbb  r9, r5, r6
    smultt  lr, r5, r6

    ldr     r3, [r0, #8]
    ldr     r4, [r1, #8]
    ldr     r5, [r0, #12]
    ldr     r6, [r1, #12]

    strh    r7, [r2], #2            ;store result
    smulbb  r7, r3, r4              ;multiply
    strh    r8, [r2], #2
    smultt  r8, r3, r4
    strh    r9, [r2], #2
    smulbb  r9, r5, r6
    strh    lr, [r2], #2
    smultt  lr, r5, r6

    subs    r12, r12, #1

    add     r0, r0, #16
    add     r1, r1, #16

    ldrne       r3, [r0]
    strh    r7, [r2], #2            ;store result
    ldrne       r4, [r1]
    strh    r8, [r2], #2
    ldrne       r5, [r0, #4]
    strh    r9, [r2], #2
    ldrne       r6, [r1, #4]
    strh    lr, [r2], #2

    bne     dequant_loop

    ldmia   sp!, {r4-r9, pc}
    ENDP    ;|vp8_dequantize_b_loop_v6|

    END
