;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT |vp8_short_walsh4x4_armv6|

    ARM
    REQUIRE8
    PRESERVE8

    AREA    |.text|, CODE, READONLY  ; name this block of code

;short vp8_short_walsh4x4_armv6(short *input, short *output, int pitch)
|vp8_short_walsh4x4_armv6| PROC

    stmdb       sp!, {r4 - r11, lr}

    mov         r12, r2              ; ugh. not clean
    ldr         r2, [r0]             ; [1  |  0]
    ldr         r3, [r0, #4]         ; [3  |  2]
    ldr         r4, [r0, r12]!       ; [5  |  4]
    ldr         r5, [r0, #4]         ; [7  |  6]
    ldr         r6, [r0, r12]!       ; [9  |  8]
    ldr         r7, [r0, #4]         ; [11 | 10]
    ldr         r8, [r0, r12]!       ; [13 | 12]
    ldr         r9, [r0, #4]         ; [15 | 14]

    qsubaddx    r10, r2, r3          ; [c1|a1] [1-2   |   0+3]
    qaddsubx    r11, r2, r3          ; [b1|d1] [1+2   |   0-3]
    qsubaddx    r12, r4, r5          ; [c1|a1] [5-6   |   4+7]
    qaddsubx    lr, r4, r5           ; [b1|d1] [5+6   |   4-7]

    qaddsubx    r2, r10, r11         ; [1 | 2] [c1+d1 | a1-b1]
    qaddsubx    r3, r11, r10         ; [0 | 3] [b1+a1 | d1-c1]
    qaddsubx    r4, r12, lr          ; [5 | 6] [c1+d1 | a1-b1]
    qaddsubx    r5, lr, r12          ; [4 | 7] [b1+a1 | d1-c1]

    qsubaddx    r10, r6, r7          ; [c1|a1] [9-10  |  8+11]
    qaddsubx    r11, r6, r7          ; [b1|d1] [9+10  |  8-11]
    qsubaddx    r12, r8, r9          ; [c1|a1] [13-14 | 12+15]
    qaddsubx    lr, r8, r9           ; [b1|d1] [13+14 | 12-15]

    qaddsubx    r6, r10, r11         ; [9 |10] [c1+d1 | a1-b1]
    qaddsubx    r7, r11, r10         ; [8 |11] [b1+a1 | d1-c1]
    qaddsubx    r8, r12, lr          ; [13|14] [c1+d1 | a1-b1]
    qaddsubx    r9, lr, r12          ; [12|15] [b1+a1 | d1-c1]

    ; first transform complete

    qadd16      r10, r3, r9          ; a1 [0+12  |  3+15]
    qadd16      r11, r5, r7          ; b1 [4+8   |  7+11]
    qsub16      r12, r5, r7          ; c1 [4-8   |  7-11]
    qsub16      lr, r3, r9           ; d1 [0-12  |  3-15]

    qadd16      r3, r10, r11         ; a2 [a1+b1] [0 | 3]
    qadd16      r5, r12, lr          ; b2 [c1+d1] [4 | 7]
    qsub16      r7, r10, r11         ; c2 [a1-b1] [8 |11]
    qsub16      r9, lr, r12          ; d2 [d1-c1] [12|15]

    qadd16      r10, r2, r8          ; a1 [1+13  |  2+14]
    qadd16      r11, r4, r6          ; b1 [5+9   |  6+10]
    qsub16      r12, r4, r6          ; c1 [5-9   |  6-10]
    qsub16      lr, r2, r8           ; d1 [1-13  |  2-14]

    qadd16      r2, r10, r11         ; a2 [a1+b1] [1 | 2]
    qadd16      r4, r12, lr          ; b2 [c1+d1] [5 | 6]
    qsub16      r6, r10, r11         ; c2 [a1-b1] [9 |10]
    qsub16      r8, lr, r12          ; d2 [d1-c1] [13|14]

    ; [a-d]2 += ([a-d]2 > 0)

    asrs        r10, r3, #16
    addpl       r10, r10, #1         ; [~0]
    asrs        r11, r2, #16
    addpl       r11, r11, #1         ; [~1]
    lsl         r11, r11, #15        ; [1  |  x]
    pkhtb       r10, r11, r10, asr #1; [1  |  0]
    str         r10, [r1], #4

    lsls        r11, r2, #16
    addpl       r11, r11, #0x10000   ; [~2]
    lsls        r12, r3, #16
    addpl       r12, r12, #0x10000   ; [~3]
    asr         r12, r12, #1         ; [3  |  x]
    pkhtb       r11, r12, r11, asr #17; [3  |  2]
    str         r11, [r1], #4

    asrs        r2, r5, #16
    addpl       r2, r2, #1           ; [~4]
    asrs        r3, r4, #16
    addpl       r3, r3, #1           ; [~5]
    lsl         r3, r3, #15          ; [5  |  x]
    pkhtb       r2, r3, r2, asr #1   ; [5  |  4]
    str         r2, [r1], #4

    lsls        r2, r4, #16
    addpl       r2, r2, #0x10000     ; [~6]
    lsls        r3, r5, #16
    addpl       r3, r3, #0x10000     ; [~7]
    asr         r3, r3, #1           ; [7  |  x]
    pkhtb       r2, r3, r2, asr #17  ; [7  |  6]
    str         r2, [r1], #4

    asrs        r2, r7, #16
    addpl       r2, r2, #1           ; [~8]
    asrs        r3, r6, #16
    addpl       r3, r3, #1           ; [~9]
    lsl         r3, r3, #15          ; [9  |  x]
    pkhtb       r2, r3, r2, asr #1   ; [9  |  8]
    str         r2, [r1], #4

    lsls        r2, r6, #16
    addpl       r2, r2, #0x10000     ; [~10]
    lsls        r3, r7, #16
    addpl       r3, r3, #0x10000     ; [~11]
    asr         r3, r3, #1           ; [11 |  x]
    pkhtb       r2, r3, r2, asr #17  ; [11 | 10]
    str         r2, [r1], #4

    asrs        r2, r9, #16
    addpl       r2, r2, #1           ; [~12]
    asrs        r3, r8, #16
    addpl       r3, r3, #1           ; [~13]
    lsl         r3, r3, #15          ; [13 |  x]
    pkhtb       r2, r3, r2, asr #1   ; [13 | 12]
    str         r2, [r1], #4

    lsls        r2, r8, #16
    addpl       r2, r2, #0x10000     ; [~14]
    lsls        r3, r9, #16
    addpl       r3, r3, #0x10000     ; [~15]
    asr         r3, r3, #1           ; [15 |  x]
    pkhtb       r2, r3, r2, asr #17  ; [15 | 14]
    str         r2, [r1]

    ldmia       sp!, {r4 - r11, pc}
    ENDP        ; |vp8_short_walsh4x4_armv6|

    END
