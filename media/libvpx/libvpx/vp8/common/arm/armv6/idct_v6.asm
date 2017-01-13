;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_short_idct4x4llm_v6_dual|

    AREA    |.text|, CODE, READONLY


; void vp8_short_idct4x4llm_c(short *input, unsigned char *pred, int pitch,
;                             unsigned char *dst, int stride)
; r0    short* input
; r1    unsigned char* pred
; r2    int pitch
; r3    unsigned char* dst
; sp    int stride

|vp8_short_idct4x4llm_v6_dual| PROC
    stmdb   sp!, {r4-r11, lr}

    sub     sp, sp, #4

    mov     r4, #0x00008A00         ; sin
    orr     r4, r4, #0x0000008C     ; sinpi8sqrt2

    mov     r5, #0x00004E00         ; cos
    orr     r5, r5, #0x0000007B     ; cospi8sqrt2minus1
    orr     r5, r5, #1<<31          ; loop counter on top bit

loop1_dual
    ldr     r6, [r0, #(4*2)]        ; i5 | i4
    ldr     r12, [r0, #(12*2)]      ; i13|i12
    ldr     r14, [r0, #(8*2)]       ; i9 | i8

    smulbt  r9, r5, r6              ; (ip[5] * cospi8sqrt2minus1) >> 16
    smulbb  r7, r5, r6              ; (ip[4] * cospi8sqrt2minus1) >> 16
    smulwt  r10, r4, r6             ; (ip[5] * sinpi8sqrt2) >> 16
    smulwb  r8, r4, r6              ; (ip[4] * sinpi8sqrt2) >> 16

    smulbt  r11, r5, r12            ; (ip[13] * cospi8sqrt2minus1) >> 16
    pkhtb   r7, r9, r7, asr #16     ; 5c | 4c
    pkhbt   r8, r8, r10, lsl #16    ; 5s | 4s
    uadd16  r6, r6, r7              ; 5c+5 | 4c+4

    smulwt  r7, r4, r12             ; (ip[13] * sinpi8sqrt2) >> 16
    smulbb  r9, r5, r12             ; (ip[12] * cospi8sqrt2minus1) >> 16
    smulwb  r10, r4, r12            ; (ip[12] * sinpi8sqrt2) >> 16

    subs    r5, r5, #1<<31          ; i--

    pkhtb   r9, r11, r9, asr #16    ; 13c | 12c
    ldr     r11, [r0]               ; i1 | i0
    pkhbt   r10, r10, r7, lsl #16   ; 13s | 12s
    uadd16  r7, r12, r9             ; 13c+13 | 12c+12

    usub16  r7, r8, r7              ; c
    uadd16  r6, r6, r10             ; d
    uadd16  r10, r11, r14           ; a
    usub16  r8, r11, r14            ; b

    uadd16  r9, r10, r6             ; a+d
    usub16  r10, r10, r6            ; a-d
    uadd16  r6, r8, r7              ; b+c
    usub16  r7, r8, r7              ; b-c

    ; use input buffer to store intermediate results
    str      r6, [r0, #(4*2)]       ; o5 | o4
    str      r7, [r0, #(8*2)]       ; o9 | o8
    str      r10,[r0, #(12*2)]      ; o13|o12
    str      r9, [r0], #4           ; o1 | o0

    bcs loop1_dual

    sub     r0, r0, #8              ; reset input/output
    str     r0, [sp]

loop2_dual

    ldr     r6, [r0, #(4*2)]        ; i5 | i4
    ldr     r12,[r0, #(2*2)]        ; i3 | i2
    ldr     r14,[r0, #(6*2)]        ; i7 | i6
    ldr     r0, [r0, #(0*2)]        ; i1 | i0

    smulbt  r9, r5, r6              ; (ip[5] * cospi8sqrt2minus1) >> 16
    smulbt  r7, r5, r0              ; (ip[1] * cospi8sqrt2minus1) >> 16
    smulwt  r10, r4, r6             ; (ip[5] * sinpi8sqrt2) >> 16
    smulwt  r8, r4, r0              ; (ip[1] * sinpi8sqrt2) >> 16

    pkhbt   r11, r6, r0, lsl #16    ; i0 | i4
    pkhtb   r7, r7, r9, asr #16     ; 1c | 5c
    pkhtb   r0, r0, r6, asr #16     ; i1 | i5
    pkhbt   r8, r10, r8, lsl #16    ; 1s | 5s = temp1

    uadd16  r0, r7, r0              ; 1c+1 | 5c+5 = temp2
    pkhbt   r9, r14, r12, lsl #16   ; i2 | i6
    uadd16  r10, r11, r9            ; a
    usub16  r9, r11, r9             ; b
    pkhtb   r6, r12, r14, asr #16   ; i3 | i7

    subs    r5, r5, #1<<31          ; i--

    smulbt  r7, r5, r6              ; (ip[3] * cospi8sqrt2minus1) >> 16
    smulwt  r11, r4, r6             ; (ip[3] * sinpi8sqrt2) >> 16
    smulbb  r12, r5, r6             ; (ip[7] * cospi8sqrt2minus1) >> 16
    smulwb  r14, r4, r6             ; (ip[7] * sinpi8sqrt2) >> 16

    pkhtb   r7, r7, r12, asr #16    ; 3c | 7c
    pkhbt   r11, r14, r11, lsl #16  ; 3s | 7s = temp1

    uadd16  r6, r7, r6              ; 3c+3 | 7c+7 = temp2
    usub16  r12, r8, r6             ; c (o1 | o5)
    uadd16  r6, r11, r0             ; d (o3 | o7)
    uadd16  r7, r10, r6             ; a+d

    mov     r8, #4                  ; set up 4's
    orr     r8, r8, #0x40000        ; 4|4

    usub16  r6, r10, r6             ; a-d
    uadd16  r6, r6, r8              ; a-d+4, 3|7
    uadd16  r7, r7, r8              ; a+d+4, 0|4
    uadd16  r10, r9, r12            ; b+c
    usub16  r0, r9, r12             ; b-c
    uadd16  r10, r10, r8            ; b+c+4, 1|5
    uadd16  r8, r0, r8              ; b-c+4, 2|6

    ldr     lr, [sp, #40]           ; dst stride

    ldrb    r0, [r1]                ; pred p0
    ldrb    r11, [r1, #1]           ; pred p1
    ldrb    r12, [r1, #2]           ; pred p2

    add     r0, r0, r7, asr #19     ; p0 + o0
    add     r11, r11, r10, asr #19  ; p1 + o1
    add     r12, r12, r8, asr #19   ; p2 + o2

    usat    r0, #8, r0              ; d0 = clip8(p0 + o0)
    usat    r11, #8, r11            ; d1 = clip8(p1 + o1)
    usat    r12, #8, r12            ; d2 = clip8(p2 + o2)

    add     r0, r0, r11, lsl #8     ; |--|--|d1|d0|

    ldrb    r11, [r1, #3]           ; pred p3

    add     r0, r0, r12, lsl #16    ; |--|d2|d1|d0|

    add     r11, r11, r6, asr #19   ; p3 + o3

    sxth    r7, r7                  ;
    sxth    r10, r10                ;

    usat    r11, #8, r11            ; d3 = clip8(p3 + o3)

    sxth    r8, r8                  ;
    sxth    r6, r6                  ;

    add     r0, r0, r11, lsl #24    ; |d3|d2|d1|d0|

    ldrb    r12, [r1, r2]!          ; pred p4
    str     r0, [r3], lr
    ldrb    r11, [r1, #1]           ; pred p5

    add     r12, r12, r7, asr #3    ; p4 + o4
    add     r11, r11, r10, asr #3   ; p5 + o5

    usat    r12, #8, r12            ; d4 = clip8(p4 + o4)
    usat    r11, #8, r11            ; d5 = clip8(p5 + o5)

    ldrb    r7, [r1, #2]            ; pred p6
    ldrb    r10, [r1, #3]           ; pred p6

    add     r12, r12, r11, lsl #8   ; |--|--|d5|d4|

    add     r7, r7, r8, asr #3      ; p6 + o6
    add     r10, r10, r6, asr #3    ; p7 + o7

    ldr     r0, [sp]                ; load input pointer

    usat    r7, #8, r7              ; d6 = clip8(p6 + o6)
    usat    r10, #8, r10            ; d7 = clip8(p7 + o7)

    add     r12, r12, r7, lsl #16   ; |--|d6|d5|d4|
    add     r12, r12, r10, lsl #24  ; |d7|d6|d5|d4|

    str     r12, [r3], lr
    add     r0, r0, #16
    add     r1, r1, r2              ; pred + pitch

    bcs loop2_dual

    add     sp, sp, #4              ; idct_output buffer
    ldmia   sp!, {r4 - r11, pc}

    ENDP

    END
