;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


;                   r0  r1  r2  r3  r4  r5  r6  r7  r8  r9  r10 r11 r12     r14
    EXPORT  |vp8_short_idct4x4llm_1_v6|
    EXPORT  |vp8_short_idct4x4llm_v6|
    EXPORT  |vp8_short_idct4x4llm_v6_scott|
    EXPORT  |vp8_short_idct4x4llm_v6_dual|

    AREA    |.text|, CODE, READONLY

;********************************************************************************
;*  void short_idct4x4llm_1_v6(INT16 * input, INT16 * output, INT32 pitch)
;*      r0  INT16 * input
;*      r1  INT16 * output
;*      r2  INT32 pitch
;*  bench:  3/5
;********************************************************************************

|vp8_short_idct4x4llm_1_v6| PROC         ;   cycles  in  out pit
            ;
    ldrsh   r0, [r0]    ; load input[0] 1, r0 un 2
    add r0, r0, #4  ;   1   +4
    stmdb   sp!, {r4, r5, lr}   ; make room for wide writes 1                   backup
    mov r0, r0, asr #3  ; (input[0] + 4) >> 3   1, r0 req`d ^1  >> 3
    pkhbt   r4, r0, r0, lsl #16 ; pack r0 into r4   1, r0 req`d ^1                  pack
    mov r5, r4  ; expand                        expand

    strd    r4, [r1], r2    ; *output = r0, post inc    1
    strd    r4, [r1], r2    ;   1
    strd    r4, [r1], r2    ;   1
    strd    r4, [r1]    ;   1
            ;
    ldmia   sp!, {r4, r5, pc}   ; replace vars, return                      restore
    ENDP        ; |vp8_short_idct4x4llm_1_v6|
;********************************************************************************
;********************************************************************************
;********************************************************************************

;********************************************************************************
;*  void short_idct4x4llm_v6(INT16 * input, INT16 * output, INT32 pitch)
;*      r0  INT16 * input
;*      r1  INT16 * output
;*      r2  INT32 pitch
;*  bench:
;********************************************************************************

|vp8_short_idct4x4llm_v6| PROC           ;   cycles  in  out pit
            ;
    stmdb   sp!, {r4-r11, lr}   ; backup registers  1                   backup
            ;
    mov r4, #0x00004E00 ;   1                   cst
    orr r4, r4, #0x0000007B ; cospi8sqrt2minus1
    mov r5, #0x00008A00 ;   1                       cst
    orr r5, r5, #0x0000008C ; sinpi8sqrt2
            ;
    mov r6, #4  ; i=4   1                           i
loop1           ;
    ldrsh   r12, [r0, #8]   ; input[4]  1, r12 unavail 2                                                    [4]
    ldrsh   r3, [r0, #24]   ; input[12] 1, r3 unavail 2             [12]
    ldrsh   r8, [r0, #16]   ; input[8]  1, r8 unavail 2                                 [8]
    ldrsh   r7, [r0], #0x2  ; input[0]  1, r7 unavail 2 ++                          [0]
    smulwb  r10, r5, r12    ; ([4] * sinpi8sqrt2) >> 16 1, r10 un 2, r12/r5 ^1                                          t1
    smulwb  r11, r4, r3 ; ([12] * cospi8sqrt2minus1) >> 16  1, r11 un 2, r3/r4 ^1                                               t2
    add r9, r7, r8  ; a1 = [0] + [8]    1                                       a1
    sub r7, r7, r8  ; b1 = [0] - [8]    1                               b1
    add r11, r3, r11    ; temp2 1
    rsb r11, r11, r10   ; c1 = temp1 - temp2    1                                               c1
    smulwb  r3, r5, r3  ; ([12] * sinpi8sqrt2) >> 16    1, r3 un 2, r3/r5 ^ 1               t2
    smulwb  r10, r4, r12    ; ([4] * cospi8sqrt2minus1) >> 16   1, r10 un 2, r12/r4 ^1                                          t1
    add r8, r7, r11 ; b1 + c1   1                                   b+c
    strh    r8, [r1, r2]    ; out[pitch] = b1+c1    1
    sub r7, r7, r11 ; b1 - c1   1                               b-c
    add r10, r12, r10   ; temp1 1
    add r3, r10, r3 ; d1 = temp1 + temp2    1               d1
    add r10, r9, r3 ; a1 + d1   1                                           a+d
    sub r3, r9, r3  ; a1 - d1   1               a-d
    add r8, r2, r2  ; pitch * 2 1                                   p*2
    strh    r7, [r1, r8]    ; out[pitch*2] = b1-c1  1
    add r7, r2, r2, lsl #1  ; pitch * 3 1                               p*3
    strh    r3, [r1, r7]    ; out[pitch*3] = a1-d1  1
    subs    r6, r6, #1  ; i--   1                           --
    strh    r10, [r1], #0x2 ; out[0] = a1+d1    1       ++
    bne loop1   ; if i>0, continue
            ;
    sub r1, r1, #8  ; set up out for next loop  1       -4
            ; for this iteration, input=prev output
    mov r6, #4  ; i=4   1                           i
;   b   returnfull
loop2           ;
    ldrsh   r11, [r1, #2]   ; input[1]  1, r11 un 2                                             [1]
    ldrsh   r8, [r1, #6]    ; input[3]  1, r8 un 2                                  [3]
    ldrsh   r3, [r1, #4]    ; input[2]  1, r3 un 2              [2]
    ldrsh   r0, [r1]    ; input[0]  1, r0 un 2  [0]
    smulwb  r9, r5, r11 ; ([1] * sinpi8sqrt2) >> 16 1, r9 un 2, r5/r11 ^1                                       t1
    smulwb  r10, r4, r8 ; ([3] * cospi8sqrt2minus1) >> 16   1, r10 un 2, r4/r8 ^1                                           t2
    add r7, r0, r3  ; a1 = [0] + [2]    1                               a1
    sub r0, r0, r3  ; b1 = [0] - [2]    1   b1
    add r10, r8, r10    ; temp2 1
    rsb r9, r10, r9 ; c1 = temp1 - temp2    1                                       c1
    smulwb  r8, r5, r8  ; ([3] * sinpi8sqrt2) >> 16 1, r8 un 2, r5/r8 ^1                                    t2
    smulwb  r10, r4, r11    ; ([1] * cospi8sqrt2minus1) >> 16   1, r10 un 2, r4/r11 ^1                                          t1
    add r3, r0, r9  ; b1+c1 1               b+c
    add r3, r3, #4  ; b1+c1+4   1               +4
    add r10, r11, r10   ; temp1 1
    mov r3, r3, asr #3  ; b1+c1+4 >> 3  1, r3 ^1                >>3
    strh    r3, [r1, #2]    ; out[1] = b1+c1    1
    add r10, r10, r8    ; d1 = temp1 + temp2    1                                           d1
    add r3, r7, r10 ; a1+d1 1               a+d
    add r3, r3, #4  ; a1+d1+4   1               +4
    sub r7, r7, r10 ; a1-d1 1                               a-d
    add r7, r7, #4  ; a1-d1+4   1                               +4
    mov r3, r3, asr #3  ; a1+d1+4 >> 3  1, r3 ^1                >>3
    mov r7, r7, asr #3  ; a1-d1+4 >> 3  1, r7 ^1                                >>3
    strh    r7, [r1, #6]    ; out[3] = a1-d1    1
    sub r0, r0, r9  ; b1-c1 1   b-c
    add r0, r0, #4  ; b1-c1+4   1   +4
    subs    r6, r6, #1  ; i--   1                           --
    mov r0, r0, asr #3  ; b1-c1+4 >> 3  1, r0 ^1    >>3
    strh    r0, [r1, #4]    ; out[2] = b1-c1    1
    strh    r3, [r1], r2    ; out[0] = a1+d1    1
;   add r1, r1, r2  ; out += pitch  1       ++
    bne loop2   ; if i>0, continue
returnfull          ;
    ldmia   sp!, {r4 - r11, pc} ; replace vars, return                      restore
    ENDP

;********************************************************************************
;********************************************************************************
;********************************************************************************

;********************************************************************************
;*  void short_idct4x4llm_v6_scott(INT16 * input, INT16 * output, INT32 pitch)
;*      r0  INT16 * input
;*      r1  INT16 * output
;*      r2  INT32 pitch
;*  bench:
;********************************************************************************

|vp8_short_idct4x4llm_v6_scott| PROC         ;   cycles  in  out pit
;   mov r0, #0  ;
;   ldr r0, [r0]    ;
    stmdb   sp!, {r4 - r11, lr} ; backup registers  1                   backup
            ;
    mov r3, #0x00004E00 ;                   cos
    orr r3, r3, #0x0000007B ; cospi8sqrt2minus1
    mov r4, #0x00008A00 ;                       sin
    orr r4, r4, #0x0000008C ; sinpi8sqrt2
            ;
    mov r5, #0x2    ; i                         i
            ;
short_idct4x4llm_v6_scott_loop1          ;
    ldr r10, [r0, #(4*2)]   ; i5 | i4                                               5,4
    ldr r11, [r0, #(12*2)]  ; i13 | i12                                                 13,12
            ;
    smulwb  r6, r4, r10 ; ((ip[4] * sinpi8sqrt2) >> 16)                             lt1
    smulwb  r7, r3, r11 ; ((ip[12] * cospi8sqrt2minus1) >> 16)                                  lt2
            ;
    smulwb  r12, r3, r10    ; ((ip[4] * cospi8sqrt2misu1) >> 16)                                                        l2t2
    smulwb  r14, r4, r11    ; ((ip[12] * sinpi8sqrt2) >> 16)                                                                l2t1
            ;
    add r6, r6, r7  ; partial c1                                lt1-lt2
    add r12, r12, r14   ; partial d1                                                        l2t2+l2t1
            ;
    smulwt  r14, r4, r10    ; ((ip[5] * sinpi8sqrt2) >> 16)                                                             ht1
    smulwt  r7, r3, r11 ; ((ip[13] * cospi8sqrt2minus1) >> 16)                                  ht2
            ;
    smulwt  r8, r3, r10 ; ((ip[5] * cospi8sqrt2minus1) >> 16)                                       h2t1
    smulwt  r9, r4, r11 ; ((ip[13] * sinpi8sqrt2) >> 16)                                            h2t2
            ;
    add r7, r14, r7 ; partial c1_2                                  ht1+ht2
    sub r8, r8, r9  ; partial d1_2                                      h2t1-h2t2
            ;
    pkhbt   r6, r6, r7, lsl #16 ; partial c1_2 | partial c1_1                               pack
    pkhbt   r12, r12, r8, lsl #16   ; partial d1_2 | partial d1_1                                                       pack
            ;
    usub16  r6, r6, r10 ; c1_2 | c1_1                               c
    uadd16  r12, r12, r11   ; d1_2 | d1_1                                                       d
            ;
    ldr r10, [r0, #0]   ; i1 | i0                                               1,0
    ldr r11, [r0, #(8*2)]   ; i9 | i10                                                  9,10
            ;
;;;;;;  add r0, r0, #0x4    ;       +4
;;;;;;  add r1, r1, #0x4    ;           +4
            ;
    uadd16  r8, r10, r11    ; i1 + i9 | i0 + i8 aka a1                                      a
    usub16  r9, r10, r11    ; i1 - i9 | i0 - i8 aka b1                                          b
            ;
    uadd16  r7, r8, r12 ; a1 + d1 pair                                  a+d
    usub16  r14, r8, r12    ; a1 - d1 pair                                                              a-d
            ;
    str r7, [r1]    ; op[0] = a1 + d1
    str r14, [r1, r2]   ; op[pitch*3] = a1 - d1
            ;
    add r0, r0, #0x4    ; op[pitch] = b1 + c1       ++
    add r1, r1, #0x4    ; op[pitch*2] = b1 - c1         ++
            ;
    subs    r5, r5, #0x1    ;                           --
    bne short_idct4x4llm_v6_scott_loop1  ;
            ;
    sub r1, r1, #16 ; reset output ptr
    mov r5, #0x4    ;
    mov r0, r1  ; input = output
            ;
short_idct4x4llm_v6_scott_loop2          ;
            ;
    subs    r5, r5, #0x1    ;
    bne short_idct4x4llm_v6_scott_loop2  ;
            ;
    ldmia   sp!, {r4 - r11, pc} ;
    ENDP        ;
            ;
;********************************************************************************
;********************************************************************************
;********************************************************************************

;********************************************************************************
;*  void short_idct4x4llm_v6_dual(INT16 * input, INT16 * output, INT32 pitch)
;*      r0  INT16 * input
;*      r1  INT16 * output
;*      r2  INT32 pitch
;*  bench:
;********************************************************************************

|vp8_short_idct4x4llm_v6_dual| PROC          ;   cycles  in  out pit
            ;
    stmdb   sp!, {r4-r11, lr}   ; backup registers  1                   backup
    mov r3, #0x00004E00 ;                   cos
    orr r3, r3, #0x0000007B ; cospi8sqrt2minus1
    mov r4, #0x00008A00 ;                       sin
    orr r4, r4, #0x0000008C ; sinpi8sqrt2
    mov r5, #0x2    ; i=2                           i
loop1_dual
    ldr r6, [r0, #(4*2)]    ; i5 | i4                               5|4
    ldr r12, [r0, #(12*2)]  ; i13 | i12                                                     13|12
    ldr r14, [r0, #(8*2)]   ; i9 | i8                                                               9|8

    smulwt  r9, r3, r6  ; (ip[5] * cospi8sqrt2minus1) >> 16                                         5c
    smulwb  r7, r3, r6  ; (ip[4] * cospi8sqrt2minus1) >> 16                                 4c
    smulwt  r10, r4, r6 ; (ip[5] * sinpi8sqrt2) >> 16                                               5s
    smulwb  r8, r4, r6  ; (ip[4] * sinpi8sqrt2) >> 16                                       4s
    pkhbt   r7, r7, r9, lsl #16 ; 5c | 4c
    smulwt  r11, r3, r12    ; (ip[13] * cospi8sqrt2minus1) >> 16                                                    13c
    pkhbt   r8, r8, r10, lsl #16    ; 5s | 4s
    uadd16  r6, r6, r7  ; 5c+5 | 4c+4
    smulwt  r7, r4, r12 ; (ip[13] * sinpi8sqrt2) >> 16                                  13s
    smulwb  r9, r3, r12 ; (ip[12] * cospi8sqrt2minus1) >> 16                                            12c
    smulwb  r10, r4, r12    ; (ip[12] * sinpi8sqrt2) >> 16                                              12s
    subs    r5, r5, #0x1    ; i--                           --
    pkhbt   r9, r9, r11, lsl #16    ; 13c | 12c
    ldr r11, [r0], #0x4 ; i1 | i0       ++                                          1|0
    pkhbt   r10, r10, r7, lsl #16   ; 13s | 12s
    uadd16  r7, r12, r9 ; 13c+13 | 12c+12
    usub16  r7, r8, r7  ; c                                 c
    uadd16  r6, r6, r10 ; d                             d
    uadd16  r10, r11, r14   ; a                                             a
    usub16  r8, r11, r14    ; b                                     b
    uadd16  r9, r10, r6 ; a+d                                           a+d
    usub16  r10, r10, r6    ; a-d                                               a-d
    uadd16  r6, r8, r7  ; b+c                               b+c
    usub16  r7, r8, r7  ; b-c                                   b-c
    str r6, [r1, r2]    ; o5 | o4
    add r6, r2, r2  ; pitch * 2                             p2
    str r7, [r1, r6]    ; o9 | o8
    add r6,  r6, r2 ; pitch * 3                             p3
    str r10, [r1, r6]   ; o13 | o12
    str r9, [r1], #0x4  ; o1 | o0           ++
    bne loop1_dual  ;
    mov r5, #0x2    ; i=2                           i
    sub r0, r1, #8  ; reset input/output        i/o
loop2_dual
    ldr r6, [r0, r2]    ; i5 | i4                               5|4
    ldr r1, [r0]    ; i1 | i0           1|0
    ldr r12, [r0, #0x4] ; i3 | i2                                                       3|2
    add r14, r2, #0x4   ; pitch + 2                                                             p+2
    ldr r14, [r0, r14]  ; i7 | i6                                                               7|6
    smulwt  r9, r3, r6  ; (ip[5] * cospi8sqrt2minus1) >> 16                                         5c
    smulwt  r7, r3, r1  ; (ip[1] * cospi8sqrt2minus1) >> 16                                 1c
    smulwt  r10, r4, r6 ; (ip[5] * sinpi8sqrt2) >> 16                                               5s
    smulwt  r8, r4, r1  ; (ip[1] * sinpi8sqrt2) >> 16                                       1s
    pkhbt   r11, r6, r1, lsl #16    ; i0 | i4                                                   0|4
    pkhbt   r7, r9, r7, lsl #16 ; 1c | 5c
    pkhbt   r8, r10, r8, lsl #16    ; 1s | 5s = temp1 ©                                     tc1
    pkhtb   r1, r1, r6, asr #16 ; i1 | i5           1|5
    uadd16  r1, r7, r1  ; 1c+1 | 5c+5 = temp2 (d)           td2
    pkhbt   r9, r14, r12, lsl #16   ; i2 | i6                                           2|6
    uadd16  r10, r11, r9    ; a                                             a
    usub16  r9, r11, r9 ; b                                         b
    pkhtb   r6, r12, r14, asr #16   ; i3 | i7                               3|7
    subs    r5, r5, #0x1    ; i--                           --
    smulwt  r7, r3, r6  ; (ip[3] * cospi8sqrt2minus1) >> 16                                 3c
    smulwt  r11, r4, r6 ; (ip[3] * sinpi8sqrt2) >> 16                                                   3s
    smulwb  r12, r3, r6 ; (ip[7] * cospi8sqrt2minus1) >> 16                                                     7c
    smulwb  r14, r4, r6 ; (ip[7] * sinpi8sqrt2) >> 16                                                               7s

    pkhbt   r7, r12, r7, lsl #16    ; 3c | 7c
    pkhbt   r11, r14, r11, lsl #16  ; 3s | 7s = temp1 (d)                                                   td1
    uadd16  r6, r7, r6  ; 3c+3 | 7c+7 = temp2  (c)                              tc2
    usub16  r12, r8, r6 ; c (o1 | o5)                                                       c
    uadd16  r6, r11, r1 ; d (o3 | o7)                               d
    uadd16  r7, r10, r6 ; a+d                                   a+d
    mov r8, #0x4    ; set up 4's                                        4
    orr r8, r8, #0x40000    ;                                       4|4
    usub16  r6, r10, r6 ; a-d                               a-d
    uadd16  r6, r6, r8  ; a-d+4                             3|7
    uadd16  r7, r7, r8  ; a+d+4                                 0|4
    uadd16  r10, r9, r12    ; b+c                                               b+c
    usub16  r1, r9, r12 ; b-c           b-c
    uadd16  r10, r10, r8    ; b+c+4                                             1|5
    uadd16  r1, r1, r8  ; b-c+4         2|6
    mov r8, r10, asr #19    ; o1 >> 3
    strh    r8, [r0, #2]    ; o1
    mov r8, r1, asr #19 ; o2 >> 3
    strh    r8, [r0, #4]    ; o2
    mov r8, r6, asr #19 ; o3 >> 3
    strh    r8, [r0, #6]    ; o3
    mov r8, r7, asr #19 ; o0 >> 3
    strh    r8, [r0], r2    ; o0        +p
    sxth    r10, r10    ;
    mov r8, r10, asr #3 ; o5 >> 3
    strh    r8, [r0, #2]    ; o5
    sxth    r1, r1  ;
    mov r8, r1, asr #3  ; o6 >> 3
    strh    r8, [r0, #4]    ; o6
    sxth    r6, r6  ;
    mov r8, r6, asr #3  ; o7 >> 3
    strh    r8, [r0, #6]    ; o7
    sxth    r7, r7  ;
    mov r8, r7, asr #3  ; o4 >> 3
    strh    r8, [r0], r2    ; o4        +p
;;;;;   subs    r5, r5, #0x1    ; i--                           --
    bne loop2_dual  ;
            ;
    ldmia   sp!, {r4 - r11, pc} ; replace vars, return                      restore
    ENDP

    END
