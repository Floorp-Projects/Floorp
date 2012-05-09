;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT |vp8_short_inv_walsh4x4_v6|

    ARM
    REQUIRE8
    PRESERVE8

    AREA    |.text|, CODE, READONLY  ; name this block of code

;short vp8_short_inv_walsh4x4_v6(short *input, short *mb_dqcoeff)
|vp8_short_inv_walsh4x4_v6| PROC

    stmdb       sp!, {r4 - r12, lr}

    ldr         r2, [r0, #0]         ; [1  |  0]
    ldr         r3, [r0, #4]         ; [3  |  2]
    ldr         r4, [r0, #8]         ; [5  |  4]
    ldr         r5, [r0, #12]        ; [7  |  6]
    ldr         r6, [r0, #16]        ; [9  |  8]
    ldr         r7, [r0, #20]        ; [11 | 10]
    ldr         r8, [r0, #24]        ; [13 | 12]
    ldr         r9, [r0, #28]        ; [15 | 14]

    qadd16      r10, r2, r8          ; a1 [1+13  |  0+12]
    qadd16      r11, r4, r6          ; b1 [5+9   |  4+8]
    qsub16      r12, r4, r6          ; c1 [5-9   |  4-8]
    qsub16      lr, r2, r8           ; d1 [1-13  |  0-12]

    qadd16      r2, r10, r11         ; a1 + b1 [1  |  0]
    qadd16      r4, r12, lr          ; c1 + d1 [5  |  4]
    qsub16      r6, r10, r11         ; a1 - b1 [9  |  8]
    qsub16      r8, lr, r12          ; d1 - c1 [13 | 12]

    qadd16      r10, r3, r9          ; a1 [3+15  |  2+14]
    qadd16      r11, r5, r7          ; b1 [7+11  |  6+10]
    qsub16      r12, r5, r7          ; c1 [7-11  |  6-10]
    qsub16      lr, r3, r9           ; d1 [3-15  |  2-14]

    qadd16      r3, r10, r11         ; a1 + b1 [3  |  2]
    qadd16      r5, r12, lr          ; c1 + d1 [7  |  6]
    qsub16      r7, r10, r11         ; a1 - b1 [11 | 10]
    qsub16      r9, lr, r12          ; d1 - c1 [15 | 14]

    ; first transform complete

    qsubaddx    r10, r2, r3          ; [c1|a1] [1-2   |   0+3]
    qaddsubx    r11, r2, r3          ; [b1|d1] [1+2   |   0-3]
    qsubaddx    r12, r4, r5          ; [c1|a1] [5-6   |   4+7]
    qaddsubx    lr, r4, r5           ; [b1|d1] [5+6   |   4-7]

    qaddsubx    r2, r10, r11         ; [b2|c2] [c1+d1 | a1-b1]
    qaddsubx    r3, r11, r10         ; [a2|d2] [b1+a1 | d1-c1]
    ldr         r10, c0x00030003
    qaddsubx    r4, r12, lr          ; [b2|c2] [c1+d1 | a1-b1]
    qaddsubx    r5, lr, r12          ; [a2|d2] [b1+a1 | d1-c1]

    qadd16      r2, r2, r10          ; [b2+3|c2+3]
    qadd16      r3, r3, r10          ; [a2+3|d2+3]
    qadd16      r4, r4, r10          ; [b2+3|c2+3]
    qadd16      r5, r5, r10          ; [a2+3|d2+3]

    asr         r12, r3, #19         ; [0]
    strh        r12, [r1], #32
    asr         lr, r2, #19          ; [1]
    strh        lr, [r1], #32
    sxth        r2, r2
    sxth        r3, r3
    asr         r2, r2, #3           ; [2]
    strh        r2, [r1], #32
    asr         r3, r3, #3           ; [3]
    strh        r3, [r1], #32

    asr         r12, r5, #19         ; [4]
    strh        r12, [r1], #32
    asr         lr, r4, #19          ; [5]
    strh        lr, [r1], #32
    sxth        r4, r4
    sxth        r5, r5
    asr         r4, r4, #3           ; [6]
    strh        r4, [r1], #32
    asr         r5, r5, #3           ; [7]
    strh        r5, [r1], #32

    qsubaddx    r2, r6, r7           ; [c1|a1] [9-10  |  8+11]
    qaddsubx    r3, r6, r7           ; [b1|d1] [9+10  |  8-11]
    qsubaddx    r4, r8, r9           ; [c1|a1] [13-14 | 12+15]
    qaddsubx    r5, r8, r9           ; [b1|d1] [13+14 | 12-15]

    qaddsubx    r6, r2, r3           ; [b2|c2] [c1+d1 | a1-b1]
    qaddsubx    r7, r3, r2           ; [a2|d2] [b1+a1 | d1-c1]
    qaddsubx    r8, r4, r5           ; [b2|c2] [c1+d1 | a1-b1]
    qaddsubx    r9, r5, r4           ; [a2|d2] [b1+a1 | d1-c1]

    qadd16      r6, r6, r10          ; [b2+3|c2+3]
    qadd16      r7, r7, r10          ; [a2+3|d2+3]
    qadd16      r8, r8, r10          ; [b2+3|c2+3]
    qadd16      r9, r9, r10          ; [a2+3|d2+3]

    asr         r12, r7, #19         ; [8]
    strh        r12, [r1], #32
    asr         lr, r6, #19          ; [9]
    strh        lr, [r1], #32
    sxth        r6, r6
    sxth        r7, r7
    asr         r6, r6, #3           ; [10]
    strh        r6, [r1], #32
    asr         r7, r7, #3           ; [11]
    strh        r7, [r1], #32

    asr         r12, r9, #19         ; [12]
    strh        r12, [r1], #32
    asr         lr, r8, #19          ; [13]
    strh        lr, [r1], #32
    sxth        r8, r8
    sxth        r9, r9
    asr         r8, r8, #3           ; [14]
    strh        r8, [r1], #32
    asr         r9, r9, #3           ; [15]
    strh        r9, [r1], #32

    ldmia       sp!, {r4 - r12, pc}
    ENDP        ; |vp8_short_inv_walsh4x4_v6|


; Constant Pool
c0x00030003 DCD 0x00030003
    END
