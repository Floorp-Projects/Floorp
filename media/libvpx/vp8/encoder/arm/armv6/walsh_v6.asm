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
; r0    short *input,
; r1    short *output,
; r2    int pitch
|vp8_short_walsh4x4_armv6| PROC

    stmdb       sp!, {r4 - r11, lr}

    ldrd        r4, r5, [r0], r2
    ldr         lr, c00040004
    ldrd        r6, r7, [r0], r2

    ; 0-3
    qadd16      r3, r4, r5          ; [d1|a1] [1+3   |   0+2]
    qsub16      r4, r4, r5          ; [c1|b1] [1-3   |   0-2]

    ldrd        r8, r9, [r0], r2
    ; 4-7
    qadd16      r5, r6, r7          ; [d1|a1] [5+7   |   4+6]
    qsub16      r6, r6, r7          ; [c1|b1] [5-7   |   4-6]

    ldrd        r10, r11, [r0]
    ; 8-11
    qadd16      r7, r8, r9          ; [d1|a1] [9+11  |  8+10]
    qsub16      r8, r8, r9          ; [c1|b1] [9-11  |  8-10]

    ; 12-15
    qadd16      r9, r10, r11        ; [d1|a1] [13+15 | 12+14]
    qsub16      r10, r10, r11       ; [c1|b1] [13-15 | 12-14]


    lsls        r2, r3, #16
    smuad       r11, r3, lr         ; A0 = a1<<2 + d1<<2
    addne       r11, r11, #1        ; A0 += (a1!=0)

    lsls        r2, r7, #16
    smuad       r12, r7, lr         ; C0 = a1<<2 + d1<<2
    addne       r12, r12, #1        ; C0 += (a1!=0)

    add         r0, r11, r12        ; a1_0 = A0 + C0
    sub         r11, r11, r12       ; b1_0 = A0 - C0

    lsls        r2, r5, #16
    smuad       r12, r5, lr         ; B0 = a1<<2 + d1<<2
    addne       r12, r12, #1        ; B0 += (a1!=0)

    lsls        r2, r9, #16
    smuad       r2, r9, lr          ; D0 = a1<<2 + d1<<2
    addne       r2, r2, #1          ; D0 += (a1!=0)

    add         lr, r12, r2         ; d1_0 = B0 + D0
    sub         r12, r12, r2        ; c1_0 = B0 - D0

    ; op[0,4,8,12]
    adds        r2, r0, lr          ; a2 = a1_0 + d1_0
    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    subs        r0, r0, lr          ; d2 = a1_0 - d1_0
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1]            ; op[0]

    addmi       r0, r0, #1          ; += a2 < 0
    add         r0, r0, #3          ; += 3
    ldr         lr, c00040004
    mov         r0, r0, asr #3      ; >> 3
    strh        r0, [r1, #24]       ; op[12]

    adds        r2, r11, r12        ; b2 = b1_0 + c1_0
    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    subs        r0, r11, r12        ; c2 = b1_0 - c1_0
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #8]        ; op[4]

    addmi       r0, r0, #1          ; += a2 < 0
    add         r0, r0, #3          ; += 3
    smusd       r3, r3, lr          ; A3 = a1<<2 - d1<<2
    smusd       r7, r7, lr          ; C3 = a1<<2 - d1<<2
    mov         r0, r0, asr #3      ; >> 3
    strh        r0, [r1, #16]       ; op[8]


    ; op[3,7,11,15]
    add         r0, r3, r7          ; a1_3 = A3 + C3
    sub         r3, r3, r7          ; b1_3 = A3 - C3

    smusd       r5, r5, lr          ; B3 = a1<<2 - d1<<2
    smusd       r9, r9, lr          ; D3 = a1<<2 - d1<<2
    add         r7, r5, r9          ; d1_3 = B3 + D3
    sub         r5, r5, r9          ; c1_3 = B3 - D3

    adds        r2, r0, r7          ; a2 = a1_3 + d1_3
    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    adds        r9, r3, r5          ; b2 = b1_3 + c1_3
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #6]        ; op[3]

    addmi       r9, r9, #1          ; += a2 < 0
    add         r9, r9, #3          ; += 3
    subs        r2, r3, r5          ; c2 = b1_3 - c1_3
    mov         r9, r9, asr #3      ; >> 3
    strh        r9, [r1, #14]       ; op[7]

    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    subs        r9, r0, r7          ; d2 = a1_3 - d1_3
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #22]       ; op[11]

    addmi       r9, r9, #1          ; += a2 < 0
    add         r9, r9, #3          ; += 3
    smuad       r3, r4, lr          ; A1 = b1<<2 + c1<<2
    smuad       r5, r8, lr          ; C1 = b1<<2 + c1<<2
    mov         r9, r9, asr #3      ; >> 3
    strh        r9, [r1, #30]       ; op[15]

    ; op[1,5,9,13]
    add         r0, r3, r5          ; a1_1 = A1 + C1
    sub         r3, r3, r5          ; b1_1 = A1 - C1

    smuad       r7, r6, lr          ; B1 = b1<<2 + c1<<2
    smuad       r9, r10, lr         ; D1 = b1<<2 + c1<<2
    add         r5, r7, r9          ; d1_1 = B1 + D1
    sub         r7, r7, r9          ; c1_1 = B1 - D1

    adds        r2, r0, r5          ; a2 = a1_1 + d1_1
    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    adds        r9, r3, r7          ; b2 = b1_1 + c1_1
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #2]        ; op[1]

    addmi       r9, r9, #1          ; += a2 < 0
    add         r9, r9, #3          ; += 3
    subs        r2, r3, r7          ; c2 = b1_1 - c1_1
    mov         r9, r9, asr #3      ; >> 3
    strh        r9, [r1, #10]       ; op[5]

    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    subs        r9, r0, r5          ; d2 = a1_1 - d1_1
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #18]       ; op[9]

    addmi       r9, r9, #1          ; += a2 < 0
    add         r9, r9, #3          ; += 3
    smusd       r4, r4, lr          ; A2 = b1<<2 - c1<<2
    smusd       r8, r8, lr          ; C2 = b1<<2 - c1<<2
    mov         r9, r9, asr #3      ; >> 3
    strh        r9, [r1, #26]       ; op[13]


    ; op[2,6,10,14]
    add         r11, r4, r8         ; a1_2 = A2 + C2
    sub         r12, r4, r8         ; b1_2 = A2 - C2

    smusd       r6, r6, lr          ; B2 = b1<<2 - c1<<2
    smusd       r10, r10, lr        ; D2 = b1<<2 - c1<<2
    add         r4, r6, r10         ; d1_2 = B2 + D2
    sub         r8, r6, r10         ; c1_2 = B2 - D2

    adds        r2, r11, r4         ; a2 = a1_2 + d1_2
    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    adds        r9, r12, r8         ; b2 = b1_2 + c1_2
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #4]        ; op[2]

    addmi       r9, r9, #1          ; += a2 < 0
    add         r9, r9, #3          ; += 3
    subs        r2, r12, r8         ; c2 = b1_2 - c1_2
    mov         r9, r9, asr #3      ; >> 3
    strh        r9, [r1, #12]       ; op[6]

    addmi       r2, r2, #1          ; += a2 < 0
    add         r2, r2, #3          ; += 3
    subs        r9, r11, r4         ; d2 = a1_2 - d1_2
    mov         r2, r2, asr #3      ; >> 3
    strh        r2, [r1, #20]       ; op[10]

    addmi       r9, r9, #1          ; += a2 < 0
    add         r9, r9, #3          ; += 3
    mov         r9, r9, asr #3      ; >> 3
    strh        r9, [r1, #28]       ; op[14]


    ldmia       sp!, {r4 - r11, pc}
    ENDP        ; |vp8_short_walsh4x4_armv6|

c00040004
    DCD         0x00040004

    END
