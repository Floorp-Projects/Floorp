;
;  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT |vp8_fast_fdct4x4_armv6|

    ARM
    REQUIRE8
    PRESERVE8

    AREA    |.text|, CODE, READONLY
; void vp8_short_fdct4x4_c(short *input, short *output, int pitch)
|vp8_fast_fdct4x4_armv6| PROC

    stmfd       sp!, {r4 - r12, lr}

    ; PART 1

    ; coeffs 0-3
    ldrd        r4, r5, [r0]        ; [i1 | i0] [i3 | i2]

    ldr         r10, c7500
    ldr         r11, c14500
    ldr         r12, c0x22a453a0    ; [2217*4 | 5352*4]
    ldr         lr, c0x00080008
    ror         r5, r5, #16         ; [i2 | i3]

    qadd16      r6, r4, r5          ; [i1+i2 | i0+i3] = [b1 | a1] without shift
    qsub16      r7, r4, r5          ; [i1-i2 | i0-i3] = [c1 | d1] without shift

    add         r0, r0, r2          ; update input pointer

    qadd16      r7, r7, r7          ; 2*[c1|d1] --> we can use smlad and smlsd
                                    ; with 2217*4 and 5352*4 without losing the
                                    ; sign bit (overflow)

    smuad       r4, r6, lr          ; o0 = (i1+i2)*8 + (i0+i3)*8
    smusd       r5, r6, lr          ; o2 = (i1+i2)*8 - (i0+i3)*8

    smlad       r6, r7, r12, r11    ; o1 = (c1 * 2217 + d1 * 5352 +  14500)
    smlsdx      r7, r7, r12, r10    ; o3 = (d1 * 2217 - c1 * 5352 +   7500)

    ldrd        r8, r9, [r0]        ; [i5 | i4] [i7 | i6]

    pkhbt       r3, r4, r6, lsl #4  ; [o1 | o0], keep in register for PART 2
    pkhbt       r6, r5, r7, lsl #4  ; [o3 | o2]

    str         r6, [r1, #4]

    ; coeffs 4-7
    ror         r9, r9, #16         ; [i6 | i7]

    qadd16      r6, r8, r9          ; [i5+i6 | i4+i7] = [b1 | a1] without shift
    qsub16      r7, r8, r9          ; [i5-i6 | i4-i7] = [c1 | d1] without shift

    add         r0, r0, r2          ; update input pointer

    qadd16      r7, r7, r7          ; 2x[c1|d1] --> we can use smlad and smlsd
                                    ; with 2217*4 and 5352*4 without losing the
                                    ; sign bit (overflow)

    smuad       r9, r6, lr          ; o4 = (i5+i6)*8 + (i4+i7)*8
    smusd       r8, r6, lr          ; o6 = (i5+i6)*8 - (i4+i7)*8

    smlad       r6, r7, r12, r11    ; o5 = (c1 * 2217 + d1 * 5352 +  14500)
    smlsdx      r7, r7, r12, r10    ; o7 = (d1 * 2217 - c1 * 5352 +   7500)

    ldrd        r4, r5, [r0]        ; [i9 | i8] [i11 | i10]

    pkhbt       r9, r9, r6, lsl #4  ; [o5 | o4], keep in register for PART 2
    pkhbt       r6, r8, r7, lsl #4  ; [o7 | o6]

    str         r6, [r1, #12]

    ; coeffs 8-11
    ror         r5, r5, #16         ; [i10 | i11]

    qadd16      r6, r4, r5          ; [i9+i10 | i8+i11]=[b1 | a1] without shift
    qsub16      r7, r4, r5          ; [i9-i10 | i8-i11]=[c1 | d1] without shift

    add         r0, r0, r2          ; update input pointer

    qadd16      r7, r7, r7          ; 2x[c1|d1] --> we can use smlad and smlsd
                                    ; with 2217*4 and 5352*4 without losing the
                                    ; sign bit (overflow)

    smuad       r2, r6, lr          ; o8 = (i9+i10)*8 + (i8+i11)*8
    smusd       r8, r6, lr          ; o10 = (i9+i10)*8 - (i8+i11)*8

    smlad       r6, r7, r12, r11    ; o9 = (c1 * 2217 + d1 * 5352 +  14500)
    smlsdx      r7, r7, r12, r10    ; o11 = (d1 * 2217 - c1 * 5352 +   7500)

    ldrd        r4, r5, [r0]        ; [i13 | i12] [i15 | i14]

    pkhbt       r2, r2, r6, lsl #4  ; [o9 | o8], keep in register for PART 2
    pkhbt       r6, r8, r7, lsl #4  ; [o11 | o10]

    str         r6, [r1, #20]

    ; coeffs 12-15
    ror         r5, r5, #16         ; [i14 | i15]

    qadd16      r6, r4, r5          ; [i13+i14 | i12+i15]=[b1|a1] without shift
    qsub16      r7, r4, r5          ; [i13-i14 | i12-i15]=[c1|d1] without shift

    qadd16      r7, r7, r7          ; 2x[c1|d1] --> we can use smlad and smlsd
                                    ; with 2217*4 and 5352*4 without losing the
                                    ; sign bit (overflow)

    smuad       r4, r6, lr          ; o12 = (i13+i14)*8 + (i12+i15)*8
    smusd       r5, r6, lr          ; o14 = (i13+i14)*8 - (i12+i15)*8

    smlad       r6, r7, r12, r11    ; o13 = (c1 * 2217 + d1 * 5352 +  14500)
    smlsdx      r7, r7, r12, r10    ; o15 = (d1 * 2217 - c1 * 5352 +   7500)

    pkhbt       r0, r4, r6, lsl #4  ; [o13 | o12], keep in register for PART 2
    pkhbt       r6, r5, r7, lsl #4  ; [o15 | o14]

    str         r6, [r1, #28]


    ; PART 2 -------------------------------------------------
    ldr         r11, c12000
    ldr         r10, c51000
    ldr         lr, c0x00070007

    qadd16      r4, r3, r0          ; a1 = [i1+i13 | i0+i12]
    qadd16      r5, r9, r2          ; b1 = [i5+i9  |  i4+i8]
    qsub16      r6, r9, r2          ; c1 = [i5-i9  |  i4-i8]
    qsub16      r7, r3, r0          ; d1 = [i1-i13 | i0-i12]

    qadd16      r4, r4, lr          ; a1 + 7

    add         r0, r11, #0x10000   ; add (d!=0)

    qadd16      r2, r4, r5          ; a1 + b1 + 7
    qsub16      r3, r4, r5          ; a1 - b1 + 7

    ldr         r12, c0x08a914e8    ; [2217 | 5352]

    lsl         r8, r2, #16         ; prepare bottom halfword for scaling
    asr         r2, r2, #4          ; scale top halfword
    lsl         r9, r3, #16         ; prepare bottom halfword for scaling
    asr         r3, r3, #4          ; scale top halfword
    pkhtb       r4, r2, r8, asr #20 ; pack and scale bottom halfword
    pkhtb       r5, r3, r9, asr #20 ; pack and scale bottom halfword

    smulbt      r2, r6, r12         ; [ ------ | c1*2217]
    str         r4, [r1, #0]        ; [     o1 |      o0]
    smultt      r3, r6, r12         ; [c1*2217 | ------ ]
    str         r5, [r1, #16]       ; [     o9 |      o8]

    smlabb      r8, r7, r12, r2     ; [ ------ | d1*5352]
    smlatb      r9, r7, r12, r3     ; [d1*5352 | ------ ]

    smulbb      r2, r6, r12         ; [ ------ | c1*5352]
    smultb      r3, r6, r12         ; [c1*5352 | ------ ]

    lsls        r6, r7, #16         ; d1 != 0 ?
    addeq       r8, r8, r11         ; c1_b*2217+d1_b*5352+12000 + (d==0)
    addne       r8, r8, r0          ; c1_b*2217+d1_b*5352+12000 + (d!=0)
    asrs        r6, r7, #16
    addeq       r9, r9, r11         ; c1_t*2217+d1_t*5352+12000 + (d==0)
    addne       r9, r9, r0          ; c1_t*2217+d1_t*5352+12000 + (d!=0)

    smlabt      r4, r7, r12, r10    ; [ ------ | d1*2217] + 51000
    smlatt      r5, r7, r12, r10    ; [d1*2217 | ------ ] + 51000

    pkhtb       r9, r9, r8, asr #16

    sub         r4, r4, r2
    sub         r5, r5, r3

    ldr         r3, [r1, #4]        ; [i3 | i2]

    pkhtb       r5, r5, r4, asr #16 ; [o13|o12]

    str         r9, [r1, #8]        ; [o5 | 04]

    ldr         r9, [r1, #12]       ; [i7 | i6]
    ldr         r8, [r1, #28]       ; [i15|i14]
    ldr         r2, [r1, #20]       ; [i11|i10]
    str         r5, [r1, #24]       ; [o13|o12]

    qadd16      r4, r3, r8          ; a1 = [i3+i15 | i2+i14]
    qadd16      r5, r9, r2          ; b1 = [i7+i11 | i6+i10]

    qadd16      r4, r4, lr          ; a1 + 7

    qsub16      r6, r9, r2          ; c1 = [i7-i11 | i6-i10]
    qadd16      r2, r4, r5          ; a1 + b1 + 7
    qsub16      r7, r3, r8          ; d1 = [i3-i15 | i2-i14]
    qsub16      r3, r4, r5          ; a1 - b1 + 7

    lsl         r8, r2, #16         ; prepare bottom halfword for scaling
    asr         r2, r2, #4          ; scale top halfword
    lsl         r9, r3, #16         ; prepare bottom halfword for scaling
    asr         r3, r3, #4          ; scale top halfword
    pkhtb       r4, r2, r8, asr #20 ; pack and scale bottom halfword
    pkhtb       r5, r3, r9, asr #20 ; pack and scale bottom halfword

    smulbt      r2, r6, r12         ; [ ------ | c1*2217]
    str         r4, [r1, #4]        ; [     o3 |      o2]
    smultt      r3, r6, r12         ; [c1*2217 | ------ ]
    str         r5, [r1, #20]       ; [    o11 |     o10]

    smlabb      r8, r7, r12, r2     ; [ ------ | d1*5352]
    smlatb      r9, r7, r12, r3     ; [d1*5352 | ------ ]

    smulbb      r2, r6, r12         ; [ ------ | c1*5352]
    smultb      r3, r6, r12         ; [c1*5352 | ------ ]

    lsls        r6, r7, #16         ; d1 != 0 ?
    addeq       r8, r8, r11         ; c1_b*2217+d1_b*5352+12000 + (d==0)
    addne       r8, r8, r0          ; c1_b*2217+d1_b*5352+12000 + (d!=0)

    asrs        r6, r7, #16
    addeq       r9, r9, r11         ; c1_t*2217+d1_t*5352+12000 + (d==0)
    addne       r9, r9, r0          ; c1_t*2217+d1_t*5352+12000 + (d!=0)

    smlabt      r4, r7, r12, r10    ; [ ------ | d1*2217] + 51000
    smlatt      r5, r7, r12, r10    ; [d1*2217 | ------ ] + 51000

    pkhtb       r9, r9, r8, asr #16

    sub         r4, r4, r2
    sub         r5, r5, r3

    str         r9, [r1, #12]       ; [o7 | o6]
    pkhtb       r5, r5, r4, asr #16 ; [o15|o14]

    str         r5, [r1, #28]       ; [o15|o14]

    ldmfd       sp!, {r4 - r12, pc}

    ENDP

; Used constants
c7500
    DCD     7500
c14500
    DCD     14500
c0x22a453a0
    DCD     0x22a453a0
c0x00080008
    DCD     0x00080008
c12000
    DCD     12000
c51000
    DCD     51000
c0x00070007
    DCD     0x00070007
c0x08a914e8
    DCD     0x08a914e8

    END
