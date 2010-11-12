;
;  Copyright (c) 2010 The Webm project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |idct_dequant_dc_full_2x_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;void idct_dequant_dc_full_2x_neon(short *q, short *dq, unsigned char *pre,
;                                  unsigned char *dst, int stride, short *dc);
; r0    *q,
; r1    *dq,
; r2    *pre
; r3    *dst
; sp    stride
; sp+4  *dc
|idct_dequant_dc_full_2x_neon| PROC
    vld1.16         {q0, q1}, [r1]          ; dq (same l/r)
    vld1.16         {q2, q3}, [r0]          ; l q
    mov             r1, #16                 ; pitch
    add             r0, r0, #32
    vld1.16         {q4, q5}, [r0]          ; r q
    add             r12, r2, #4
    ; interleave the predictors
    vld1.32         {d28[0]}, [r2], r1      ; l pre
    vld1.32         {d28[1]}, [r12], r1     ; r pre
    vld1.32         {d29[0]}, [r2], r1
    vld1.32         {d29[1]}, [r12], r1
    vld1.32         {d30[0]}, [r2], r1
    vld1.32         {d30[1]}, [r12], r1
    vld1.32         {d31[0]}, [r2]
    ldr             r1, [sp, #4]
    vld1.32         {d31[1]}, [r12]

    ldr             r2, _CONSTANTS_

    ldrh            r12, [r1], #2           ; lo *dc
    ldrh            r1, [r1]                ; hi *dc

    ; dequant: q[i] = q[i] * dq[i]
    vmul.i16        q2, q2, q0
    vmul.i16        q3, q3, q1
    vmul.i16        q4, q4, q0
    vmul.i16        q5, q5, q1

    ; move dc up to neon and overwrite first element
    vmov.16         d4[0], r12
    vmov.16         d8[0], r1

    vld1.16         {d0}, [r2]

    ; q2: l0r0  q3: l8r8
    ; q4: l4r4  q5: l12r12
    vswp            d5, d8
    vswp            d7, d10

    ; _CONSTANTS_ * 4,12 >> 16
    ; q6:  4 * sinpi : c1/temp1
    ; q7: 12 * sinpi : d1/temp2
    ; q8:  4 * cospi
    ; q9: 12 * cospi
    vqdmulh.s16     q6, q4, d0[2]           ; sinpi8sqrt2
    vqdmulh.s16     q7, q5, d0[2]
    vqdmulh.s16     q8, q4, d0[0]           ; cospi8sqrt2minus1
    vqdmulh.s16     q9, q5, d0[0]

    vqadd.s16       q10, q2, q3             ; a1 = 0 + 8
    vqsub.s16       q11, q2, q3             ; b1 = 0 - 8

    ; vqdmulh only accepts signed values. this was a problem because
    ; our constant had the high bit set, and was treated as a negative value.
    ; vqdmulh also doubles the value before it shifts by 16. we need to
    ; compensate for this. in the case of sinpi8sqrt2, the lowest bit is 0,
    ; so we can shift the constant without losing precision. this avoids
    ; shift again afterward, but also avoids the sign issue. win win!
    ; for cospi8sqrt2minus1 the lowest bit is 1, so we lose precision if we
    ; pre-shift it
    vshr.s16        q8, q8, #1
    vshr.s16        q9, q9, #1

    ; q4:  4 +  4 * cospi : d1/temp1
    ; q5: 12 + 12 * cospi : c1/temp2
    vqadd.s16       q4, q4, q8
    vqadd.s16       q5, q5, q9

    ; c1 = temp1 - temp2
    ; d1 = temp1 + temp2
    vqsub.s16       q2, q6, q5
    vqadd.s16       q3, q4, q7

    ; [0]: a1+d1
    ; [1]: b1+c1
    ; [2]: b1-c1
    ; [3]: a1-d1
    vqadd.s16       q4, q10, q3
    vqadd.s16       q5, q11, q2
    vqsub.s16       q6, q11, q2
    vqsub.s16       q7, q10, q3

    ; rotate
    vtrn.32         q4, q6
    vtrn.32         q5, q7
    vtrn.16         q4, q5
    vtrn.16         q6, q7
    ; idct loop 2
    ; q4: l 0, 4, 8,12 r 0, 4, 8,12
    ; q5: l 1, 5, 9,13 r 1, 5, 9,13
    ; q6: l 2, 6,10,14 r 2, 6,10,14
    ; q7: l 3, 7,11,15 r 3, 7,11,15

    ; q8:  1 * sinpi : c1/temp1
    ; q9:  3 * sinpi : d1/temp2
    ; q10: 1 * cospi
    ; q11: 3 * cospi
    vqdmulh.s16     q8, q5, d0[2]           ; sinpi8sqrt2
    vqdmulh.s16     q9, q7, d0[2]
    vqdmulh.s16     q10, q5, d0[0]          ; cospi8sqrt2minus1
    vqdmulh.s16     q11, q7, d0[0]

    vqadd.s16       q2, q4, q6             ; a1 = 0 + 2
    vqsub.s16       q3, q4, q6             ; b1 = 0 - 2

    ; see note on shifting above
    vshr.s16        q10, q10, #1
    vshr.s16        q11, q11, #1

    ; q10: 1 + 1 * cospi : d1/temp1
    ; q11: 3 + 3 * cospi : c1/temp2
    vqadd.s16       q10, q5, q10
    vqadd.s16       q11, q7, q11

    ; q8: c1 = temp1 - temp2
    ; q9: d1 = temp1 + temp2
    vqsub.s16       q8, q8, q11
    vqadd.s16       q9, q10, q9

    ; a1+d1
    ; b1+c1
    ; b1-c1
    ; a1-d1
    vqadd.s16       q4, q2, q9
    vqadd.s16       q5, q3, q8
    vqsub.s16       q6, q3, q8
    vqsub.s16       q7, q2, q9

    ; +4 >> 3 (rounding)
    vrshr.s16       q4, q4, #3              ; lo
    vrshr.s16       q5, q5, #3
    vrshr.s16       q6, q6, #3              ; hi
    vrshr.s16       q7, q7, #3

    vtrn.32         q4, q6
    vtrn.32         q5, q7
    vtrn.16         q4, q5
    vtrn.16         q6, q7

    ; adding pre
    ; input is still packed. pre was read interleaved
    vaddw.u8        q4, q4, d28
    vaddw.u8        q5, q5, d29
    vaddw.u8        q6, q6, d30
    vaddw.u8        q7, q7, d31

    vmov.i16        q14, #0
    vmov            q15, q14
    vst1.16         {q14, q15}, [r0]        ; write over high input
    sub             r0, r0, #32
    vst1.16         {q14, q15}, [r0]        ; write over low input

    ;saturate and narrow
    vqmovun.s16     d0, q4                  ; lo
    vqmovun.s16     d1, q5
    vqmovun.s16     d2, q6                  ; hi
    vqmovun.s16     d3, q7

    ldr             r1, [sp]                ; stride
    add             r2, r3, #4              ; hi
    vst1.32         {d0[0]}, [r3], r1       ; lo
    vst1.32         {d0[1]}, [r2], r1       ; hi
    vst1.32         {d1[0]}, [r3], r1
    vst1.32         {d1[1]}, [r2], r1
    vst1.32         {d2[0]}, [r3], r1
    vst1.32         {d2[1]}, [r2], r1
    vst1.32         {d3[0]}, [r3]
    vst1.32         {d3[1]}, [r2]

    bx             lr

    ENDP           ; |idct_dequant_dc_full_2x_neon|

; Constant Pool
_CONSTANTS_       DCD cospi8sqrt2minus1
cospi8sqrt2minus1 DCD 0x4e7b
; because the lowest bit in 0x8a8c is 0, we can pre-shift this
sinpi8sqrt2       DCD 0x4546

    END
