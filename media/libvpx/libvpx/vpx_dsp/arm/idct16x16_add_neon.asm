;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT  |vpx_idct16x16_256_add_neon_pass1|
    EXPORT  |vpx_idct16x16_256_add_neon_pass2|
    EXPORT  |vpx_idct16x16_10_add_neon_pass1|
    EXPORT  |vpx_idct16x16_10_add_neon_pass2|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

    ; Transpose a 8x8 16bit data matrix. Datas are loaded in q8-q15.
    MACRO
    TRANSPOSE8X8
    vswp            d17, d24
    vswp            d23, d30
    vswp            d21, d28
    vswp            d19, d26
    vtrn.32         q8, q10
    vtrn.32         q9, q11
    vtrn.32         q12, q14
    vtrn.32         q13, q15
    vtrn.16         q8, q9
    vtrn.16         q10, q11
    vtrn.16         q12, q13
    vtrn.16         q14, q15
    MEND

    AREA    Block, CODE, READONLY ; name this block of code
;void |vpx_idct16x16_256_add_neon_pass1|(int16_t *input,
;                                          int16_t *output, int output_stride)
;
; r0  int16_t input
; r1  int16_t *output
; r2  int  output_stride)

; idct16 stage1 - stage6 on all the elements loaded in q8-q15. The output
; will be stored back into q8-q15 registers. This function will touch q0-q7
; registers and use them as buffer during calculation.
|vpx_idct16x16_256_add_neon_pass1| PROC

    ; TODO(hkuang): Find a better way to load the elements.
    ; load elements of 0, 2, 4, 6, 8, 10, 12, 14 into q8 - q15
    vld2.s16        {q8,q9}, [r0]!
    vld2.s16        {q9,q10}, [r0]!
    vld2.s16        {q10,q11}, [r0]!
    vld2.s16        {q11,q12}, [r0]!
    vld2.s16        {q12,q13}, [r0]!
    vld2.s16        {q13,q14}, [r0]!
    vld2.s16        {q14,q15}, [r0]!
    vld2.s16        {q1,q2}, [r0]!
    vmov.s16        q15, q1

    ; generate  cospi_28_64 = 3196
    mov             r3, #0xc00
    add             r3, #0x7c

    ; generate cospi_4_64  = 16069
    mov             r12, #0x3e00
    add             r12, #0xc5

    ; transpose the input data
    TRANSPOSE8X8

    ; stage 3
    vdup.16         d0, r3                    ; duplicate cospi_28_64
    vdup.16         d1, r12                   ; duplicate cospi_4_64

    ; preloading to avoid stall
    ; generate cospi_12_64 = 13623
    mov             r3, #0x3500
    add             r3, #0x37

    ; generate cospi_20_64 = 9102
    mov             r12, #0x2300
    add             r12, #0x8e

    ; step2[4] * cospi_28_64
    vmull.s16       q2, d18, d0
    vmull.s16       q3, d19, d0

    ; step2[4] * cospi_4_64
    vmull.s16       q5, d18, d1
    vmull.s16       q6, d19, d1

    ; temp1 = step2[4] * cospi_28_64 - step2[7] * cospi_4_64
    vmlsl.s16       q2, d30, d1
    vmlsl.s16       q3, d31, d1

    ; temp2 = step2[4] * cospi_4_64 + step2[7] * cospi_28_64
    vmlal.s16       q5, d30, d0
    vmlal.s16       q6, d31, d0

    vdup.16         d2, r3                    ; duplicate cospi_12_64
    vdup.16         d3, r12                   ; duplicate cospi_20_64

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d8, q2, #14               ; >> 14
    vqrshrn.s32     d9, q3, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d14, q5, #14              ; >> 14
    vqrshrn.s32     d15, q6, #14              ; >> 14

    ; preloading to avoid stall
    ; generate cospi_16_64 = 11585
    mov             r3, #0x2d00
    add             r3, #0x41

    ; generate cospi_24_64 = 6270
    mov             r12, #0x1800
    add             r12, #0x7e

    ; step2[5] * cospi_12_64
    vmull.s16       q2, d26, d2
    vmull.s16       q3, d27, d2

    ; step2[5] * cospi_20_64
    vmull.s16       q9, d26, d3
    vmull.s16       q15, d27, d3

    ; temp1 = input[5] * cospi_12_64 - input[3] * cospi_20_64
    vmlsl.s16       q2, d22, d3
    vmlsl.s16       q3, d23, d3

    ; temp2 = step2[5] * cospi_20_64 + step2[6] * cospi_12_64
    vmlal.s16       q9, d22, d2
    vmlal.s16       q15, d23, d2

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d10, q2, #14              ; >> 14
    vqrshrn.s32     d11, q3, #14              ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d12, q9, #14              ; >> 14
    vqrshrn.s32     d13, q15, #14             ; >> 14

    ; stage 4
    vdup.16         d30, r3                   ; cospi_16_64

    ; step1[0] * cospi_16_64
    vmull.s16       q2, d16, d30
    vmull.s16       q11, d17, d30

    ; step1[1] * cospi_16_64
    vmull.s16       q0, d24, d30
    vmull.s16       q1, d25, d30

    ; generate cospi_8_64 = 15137
    mov             r3, #0x3b00
    add             r3, #0x21

    vdup.16         d30, r12                  ; duplicate cospi_24_64
    vdup.16         d31, r3                   ; duplicate cospi_8_64

    ; temp1 = (step1[0] + step1[1]) * cospi_16_64
    vadd.s32        q3, q2, q0
    vadd.s32        q12, q11, q1

    ; temp2 = (step1[0] - step1[1]) * cospi_16_64
    vsub.s32        q13, q2, q0
    vsub.s32        q1, q11, q1

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d16, q3, #14              ; >> 14
    vqrshrn.s32     d17, q12, #14             ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d18, q13, #14             ; >> 14
    vqrshrn.s32     d19, q1, #14              ; >> 14

    ; step1[2] * cospi_24_64 - step1[3] * cospi_8_64;
    ; step1[2] * cospi_8_64
    vmull.s16       q0, d20, d31
    vmull.s16       q1, d21, d31

    ; step1[2] * cospi_24_64
    vmull.s16       q12, d20, d30
    vmull.s16       q13, d21, d30

    ; temp2 = input[1] * cospi_8_64 + input[3] * cospi_24_64
    vmlal.s16       q0, d28, d30
    vmlal.s16       q1, d29, d30

    ; temp1 = input[1] * cospi_24_64 - input[3] * cospi_8_64
    vmlsl.s16       q12, d28, d31
    vmlsl.s16       q13, d29, d31

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d22, q0, #14              ; >> 14
    vqrshrn.s32     d23, q1, #14              ; >> 14

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d20, q12, #14             ; >> 14
    vqrshrn.s32     d21, q13, #14             ; >> 14

    vsub.s16        q13, q4, q5               ; step2[5] = step1[4] - step1[5];
    vadd.s16        q4, q4, q5                ; step2[4] = step1[4] + step1[5];
    vsub.s16        q14, q7, q6               ; step2[6] = -step1[6] + step1[7];
    vadd.s16        q15, q6, q7               ; step2[7] = step1[6] + step1[7];

    ; generate cospi_16_64 = 11585
    mov             r3, #0x2d00
    add             r3, #0x41

    ; stage 5
    vadd.s16        q0, q8, q11               ; step1[0] = step2[0] + step2[3];
    vadd.s16        q1, q9, q10               ; step1[1] = step2[1] + step2[2];
    vsub.s16        q2, q9, q10               ; step1[2] = step2[1] - step2[2];
    vsub.s16        q3, q8, q11               ; step1[3] = step2[0] - step2[3];

    vdup.16         d16, r3;                  ; duplicate cospi_16_64

    ; step2[5] * cospi_16_64
    vmull.s16       q11, d26, d16
    vmull.s16       q12, d27, d16

    ; step2[6] * cospi_16_64
    vmull.s16       q9, d28, d16
    vmull.s16       q10, d29, d16

    ; temp1 = (step2[6] - step2[5]) * cospi_16_64
    vsub.s32        q6, q9, q11
    vsub.s32        q13, q10, q12

    ; temp2 = (step2[5] + step2[6]) * cospi_16_64
    vadd.s32        q9, q9, q11
    vadd.s32        q10, q10, q12

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d10, q6, #14              ; >> 14
    vqrshrn.s32     d11, q13, #14             ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d12, q9, #14              ; >> 14
    vqrshrn.s32     d13, q10, #14             ; >> 14

    ; stage 6
    vadd.s16        q8, q0, q15                ; step2[0] = step1[0] + step1[7];
    vadd.s16        q9, q1, q6                ; step2[1] = step1[1] + step1[6];
    vadd.s16        q10, q2, q5               ; step2[2] = step1[2] + step1[5];
    vadd.s16        q11, q3, q4               ; step2[3] = step1[3] + step1[4];
    vsub.s16        q12, q3, q4               ; step2[4] = step1[3] - step1[4];
    vsub.s16        q13, q2, q5               ; step2[5] = step1[2] - step1[5];
    vsub.s16        q14, q1, q6               ; step2[6] = step1[1] - step1[6];
    vsub.s16        q15, q0, q15              ; step2[7] = step1[0] - step1[7];

    ; store the data
    vst1.64         {d16}, [r1], r2
    vst1.64         {d17}, [r1], r2
    vst1.64         {d18}, [r1], r2
    vst1.64         {d19}, [r1], r2
    vst1.64         {d20}, [r1], r2
    vst1.64         {d21}, [r1], r2
    vst1.64         {d22}, [r1], r2
    vst1.64         {d23}, [r1], r2
    vst1.64         {d24}, [r1], r2
    vst1.64         {d25}, [r1], r2
    vst1.64         {d26}, [r1], r2
    vst1.64         {d27}, [r1], r2
    vst1.64         {d28}, [r1], r2
    vst1.64         {d29}, [r1], r2
    vst1.64         {d30}, [r1], r2
    vst1.64         {d31}, [r1], r2

    bx              lr
    ENDP  ; |vpx_idct16x16_256_add_neon_pass1|

;void vpx_idct16x16_256_add_neon_pass2(int16_t *src,
;                                        int16_t *output,
;                                        int16_t *pass1Output,
;                                        int16_t skip_adding,
;                                        uint8_t *dest,
;                                        int dest_stride)
;
; r0  int16_t *src
; r1  int16_t *output,
; r2  int16_t *pass1Output,
; r3  int16_t skip_adding,
; r4  uint8_t *dest,
; r5  int dest_stride)

; idct16 stage1 - stage7 on all the elements loaded in q8-q15. The output
; will be stored back into q8-q15 registers. This function will touch q0-q7
; registers and use them as buffer during calculation.
|vpx_idct16x16_256_add_neon_pass2| PROC
    push            {r3-r9}

    ; TODO(hkuang): Find a better way to load the elements.
    ; load elements of 1, 3, 5, 7, 9, 11, 13, 15 into q8 - q15
    vld2.s16        {q8,q9}, [r0]!
    vld2.s16        {q9,q10}, [r0]!
    vld2.s16        {q10,q11}, [r0]!
    vld2.s16        {q11,q12}, [r0]!
    vld2.s16        {q12,q13}, [r0]!
    vld2.s16        {q13,q14}, [r0]!
    vld2.s16        {q14,q15}, [r0]!
    vld2.s16        {q0,q1}, [r0]!
    vmov.s16        q15, q0;

    ; generate  cospi_30_64 = 1606
    mov             r3, #0x0600
    add             r3, #0x46

    ; generate cospi_2_64  = 16305
    mov             r12, #0x3f00
    add             r12, #0xb1

    ; transpose the input data
    TRANSPOSE8X8

    ; stage 3
    vdup.16         d12, r3                   ; duplicate cospi_30_64
    vdup.16         d13, r12                  ; duplicate cospi_2_64

    ; preloading to avoid stall
    ; generate cospi_14_64 = 12665
    mov             r3, #0x3100
    add             r3, #0x79

    ; generate cospi_18_64 = 10394
    mov             r12, #0x2800
    add             r12, #0x9a

    ; step1[8] * cospi_30_64
    vmull.s16       q2, d16, d12
    vmull.s16       q3, d17, d12

    ; step1[8] * cospi_2_64
    vmull.s16       q1, d16, d13
    vmull.s16       q4, d17, d13

    ; temp1 = step1[8] * cospi_30_64 - step1[15] * cospi_2_64
    vmlsl.s16       q2, d30, d13
    vmlsl.s16       q3, d31, d13

    ; temp2 = step1[8] * cospi_2_64 + step1[15] * cospi_30_64
    vmlal.s16       q1, d30, d12
    vmlal.s16       q4, d31, d12

    vdup.16         d30, r3                   ; duplicate cospi_14_64
    vdup.16         d31, r12                  ; duplicate cospi_18_64

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d0, q2, #14               ; >> 14
    vqrshrn.s32     d1, q3, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d14, q1, #14              ; >> 14
    vqrshrn.s32     d15, q4, #14              ; >> 14

    ; preloading to avoid stall
    ; generate cospi_22_64 = 7723
    mov             r3, #0x1e00
    add             r3, #0x2b

    ; generate cospi_10_64 = 14449
    mov             r12, #0x3800
    add             r12, #0x71

    ; step1[9] * cospi_14_64
    vmull.s16       q2, d24, d30
    vmull.s16       q3, d25, d30

    ; step1[9] * cospi_18_64
    vmull.s16       q4, d24, d31
    vmull.s16       q5, d25, d31

    ; temp1 = step1[9] * cospi_14_64 - step1[14] * cospi_18_64
    vmlsl.s16       q2, d22, d31
    vmlsl.s16       q3, d23, d31

    ; temp2 = step1[9] * cospi_18_64 + step1[14] * cospi_14_64
    vmlal.s16       q4, d22, d30
    vmlal.s16       q5, d23, d30

    vdup.16         d30, r3                   ; duplicate cospi_22_64
    vdup.16         d31, r12                  ; duplicate cospi_10_64

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d2, q2, #14               ; >> 14
    vqrshrn.s32     d3, q3, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d12, q4, #14              ; >> 14
    vqrshrn.s32     d13, q5, #14              ; >> 14

    ; step1[10] * cospi_22_64
    vmull.s16       q11, d20, d30
    vmull.s16       q12, d21, d30

    ; step1[10] * cospi_10_64
    vmull.s16       q4, d20, d31
    vmull.s16       q5, d21, d31

    ; temp1 = step1[10] * cospi_22_64 - step1[13] * cospi_10_64
    vmlsl.s16       q11, d26, d31
    vmlsl.s16       q12, d27, d31

    ; temp2 = step1[10] * cospi_10_64 + step1[13] * cospi_22_64
    vmlal.s16       q4, d26, d30
    vmlal.s16       q5, d27, d30

    ; preloading to avoid stall
    ; generate cospi_6_64 = 15679
    mov             r3, #0x3d00
    add             r3, #0x3f

    ; generate cospi_26_64 = 4756
    mov             r12, #0x1200
    add             r12, #0x94

    vdup.16         d30, r3                   ; duplicate cospi_6_64
    vdup.16         d31, r12                  ; duplicate cospi_26_64

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d4, q11, #14              ; >> 14
    vqrshrn.s32     d5, q12, #14              ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d11, q5, #14              ; >> 14
    vqrshrn.s32     d10, q4, #14              ; >> 14

    ; step1[11] * cospi_6_64
    vmull.s16       q10, d28, d30
    vmull.s16       q11, d29, d30

    ; step1[11] * cospi_26_64
    vmull.s16       q12, d28, d31
    vmull.s16       q13, d29, d31

    ; temp1 = step1[11] * cospi_6_64 - step1[12] * cospi_26_64
    vmlsl.s16       q10, d18, d31
    vmlsl.s16       q11, d19, d31

    ; temp2 = step1[11] * cospi_26_64 + step1[12] * cospi_6_64
    vmlal.s16       q12, d18, d30
    vmlal.s16       q13, d19, d30

    vsub.s16        q9, q0, q1                ; step1[9]=step2[8]-step2[9]
    vadd.s16        q0, q0, q1                ; step1[8]=step2[8]+step2[9]

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d6, q10, #14              ; >> 14
    vqrshrn.s32     d7, q11, #14              ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d8, q12, #14              ; >> 14
    vqrshrn.s32     d9, q13, #14              ; >> 14

    ; stage 3
    vsub.s16        q10, q3, q2               ; step1[10]=-step2[10]+step2[11]
    vadd.s16        q11, q2, q3               ; step1[11]=step2[10]+step2[11]
    vadd.s16        q12, q4, q5               ; step1[12]=step2[12]+step2[13]
    vsub.s16        q13, q4, q5               ; step1[13]=step2[12]-step2[13]
    vsub.s16        q14, q7, q6               ; step1[14]=-step2[14]+tep2[15]
    vadd.s16        q7, q6, q7                ; step1[15]=step2[14]+step2[15]

    ; stage 4
    ; generate cospi_24_64 = 6270
    mov             r3, #0x1800
    add             r3, #0x7e

    ; generate cospi_8_64 = 15137
    mov             r12, #0x3b00
    add             r12, #0x21

    ; -step1[9] * cospi_8_64 + step1[14] * cospi_24_64
    vdup.16         d30, r12                  ; duplicate cospi_8_64
    vdup.16         d31, r3                   ; duplicate cospi_24_64

    ; step1[9] * cospi_24_64
    vmull.s16       q2, d18, d31
    vmull.s16       q3, d19, d31

    ; step1[14] * cospi_24_64
    vmull.s16       q4, d28, d31
    vmull.s16       q5, d29, d31

    ; temp2 = step1[9] * cospi_24_64 + step1[14] * cospi_8_64
    vmlal.s16       q2, d28, d30
    vmlal.s16       q3, d29, d30

    ; temp1 = -step1[9] * cospi_8_64 + step1[14] * cospi_24_64
    vmlsl.s16       q4, d18, d30
    vmlsl.s16       q5, d19, d30

    rsb             r12, #0
    vdup.16         d30, r12                  ; duplicate -cospi_8_64

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d12, q2, #14              ; >> 14
    vqrshrn.s32     d13, q3, #14              ; >> 14

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d2, q4, #14               ; >> 14
    vqrshrn.s32     d3, q5, #14               ; >> 14

    vmov.s16        q3, q11
    vmov.s16        q4, q12

    ; - step1[13] * cospi_8_64
    vmull.s16       q11, d26, d30
    vmull.s16       q12, d27, d30

    ; -step1[10] * cospi_8_64
    vmull.s16       q8, d20, d30
    vmull.s16       q9, d21, d30

    ; temp2 = -step1[10] * cospi_8_64 + step1[13] * cospi_24_64
    vmlsl.s16       q11, d20, d31
    vmlsl.s16       q12, d21, d31

    ; temp1 = -step1[10] * cospi_8_64 + step1[13] * cospi_24_64
    vmlal.s16       q8, d26, d31
    vmlal.s16       q9, d27, d31

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d4, q11, #14              ; >> 14
    vqrshrn.s32     d5, q12, #14              ; >> 14

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d10, q8, #14              ; >> 14
    vqrshrn.s32     d11, q9, #14              ; >> 14

    ; stage 5
    vadd.s16        q8, q0, q3                ; step1[8] = step2[8]+step2[11];
    vadd.s16        q9, q1, q2                ; step1[9] = step2[9]+step2[10];
    vsub.s16        q10, q1, q2               ; step1[10] = step2[9]-step2[10];
    vsub.s16        q11, q0, q3               ; step1[11] = step2[8]-step2[11];
    vsub.s16        q12, q7, q4               ; step1[12] =-step2[12]+step2[15];
    vsub.s16        q13, q6, q5               ; step1[13] =-step2[13]+step2[14];
    vadd.s16        q14, q6, q5               ; step1[14] =step2[13]+step2[14];
    vadd.s16        q15, q7, q4               ; step1[15] =step2[12]+step2[15];

    ; stage 6.
    ; generate cospi_16_64 = 11585
    mov             r12, #0x2d00
    add             r12, #0x41

    vdup.16         d14, r12                  ; duplicate cospi_16_64

    ; step1[13] * cospi_16_64
    vmull.s16       q3, d26, d14
    vmull.s16       q4, d27, d14

    ; step1[10] * cospi_16_64
    vmull.s16       q0, d20, d14
    vmull.s16       q1, d21, d14

    ; temp1 = (-step1[10] + step1[13]) * cospi_16_64
    vsub.s32        q5, q3, q0
    vsub.s32        q6, q4, q1

    ; temp2 = (step1[10] + step1[13]) * cospi_16_64
    vadd.s32        q10, q3, q0
    vadd.s32        q4, q4, q1

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d4, q5, #14               ; >> 14
    vqrshrn.s32     d5, q6, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d10, q10, #14             ; >> 14
    vqrshrn.s32     d11, q4, #14              ; >> 14

    ; step1[11] * cospi_16_64
    vmull.s16       q0, d22, d14
    vmull.s16       q1, d23, d14

    ; step1[12] * cospi_16_64
    vmull.s16       q13, d24, d14
    vmull.s16       q6, d25, d14

    ; temp1 = (-step1[11] + step1[12]) * cospi_16_64
    vsub.s32        q10, q13, q0
    vsub.s32        q4, q6, q1

    ; temp2 = (step1[11] + step1[12]) * cospi_16_64
    vadd.s32        q13, q13, q0
    vadd.s32        q6, q6, q1

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d6, q10, #14              ; >> 14
    vqrshrn.s32     d7, q4, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d8, q13, #14              ; >> 14
    vqrshrn.s32     d9, q6, #14               ; >> 14

    mov              r4, #16                  ; pass1Output stride
    ldr              r3, [sp]                 ; load skip_adding
    cmp              r3, #0                   ; check if need adding dest data
    beq              skip_adding_dest

    ldr              r7, [sp, #28]            ; dest used to save element 0-7
    mov              r9, r7                   ; save dest pointer for later use
    ldr              r8, [sp, #32]            ; load dest_stride

    ; stage 7
    ; load the data in pass1
    vld1.s16        {q0}, [r2], r4            ; load data step2[0]
    vld1.s16        {q1}, [r2], r4            ; load data step2[1]
    vld1.s16        {q10}, [r2], r4           ; load data step2[2]
    vld1.s16        {q11}, [r2], r4           ; load data step2[3]
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vadd.s16        q12, q0, q15              ; step2[0] + step2[15]
    vadd.s16        q13, q1, q14              ; step2[1] + step2[14]
    vrshr.s16       q12, q12, #6              ; ROUND_POWER_OF_TWO
    vrshr.s16       q13, q13, #6              ; ROUND_POWER_OF_TWO
    vaddw.u8        q12, q12, d12             ; + dest[j * dest_stride + i]
    vaddw.u8        q13, q13, d13             ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q12                  ; clip pixel
    vqmovun.s16     d13, q13                  ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vst1.64         {d13}, [r9], r8           ; store the data
    vsub.s16        q14, q1, q14              ; step2[1] - step2[14]
    vsub.s16        q15, q0, q15              ; step2[0] - step2[15]
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vadd.s16        q12, q10, q5              ; step2[2] + step2[13]
    vadd.s16        q13, q11, q4              ; step2[3] + step2[12]
    vrshr.s16       q12, q12, #6              ; ROUND_POWER_OF_TWO
    vrshr.s16       q13, q13, #6              ; ROUND_POWER_OF_TWO
    vaddw.u8        q12, q12, d12             ; + dest[j * dest_stride + i]
    vaddw.u8        q13, q13, d13             ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q12                  ; clip pixel
    vqmovun.s16     d13, q13                  ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vst1.64         {d13}, [r9], r8           ; store the data
    vsub.s16        q4, q11, q4               ; step2[3] - step2[12]
    vsub.s16        q5, q10, q5               ; step2[2] - step2[13]
    vld1.s16        {q0}, [r2], r4            ; load data step2[4]
    vld1.s16        {q1}, [r2], r4            ; load data step2[5]
    vld1.s16        {q10}, [r2], r4           ; load data step2[6]
    vld1.s16        {q11}, [r2], r4           ; load data step2[7]
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vadd.s16        q12, q0, q3               ; step2[4] + step2[11]
    vadd.s16        q13, q1, q2               ; step2[5] + step2[10]
    vrshr.s16       q12, q12, #6              ; ROUND_POWER_OF_TWO
    vrshr.s16       q13, q13, #6              ; ROUND_POWER_OF_TWO
    vaddw.u8        q12, q12, d12             ; + dest[j * dest_stride + i]
    vaddw.u8        q13, q13, d13             ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q12                  ; clip pixel
    vqmovun.s16     d13, q13                  ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vst1.64         {d13}, [r9], r8           ; store the data
    vsub.s16        q2, q1, q2                ; step2[5] - step2[10]
    vsub.s16        q3, q0, q3                ; step2[4] - step2[11]
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vadd.s16        q12, q10, q9              ; step2[6] + step2[9]
    vadd.s16        q13, q11, q8              ; step2[7] + step2[8]
    vrshr.s16       q12, q12, #6              ; ROUND_POWER_OF_TWO
    vrshr.s16       q13, q13, #6              ; ROUND_POWER_OF_TWO
    vaddw.u8        q12, q12, d12             ; + dest[j * dest_stride + i]
    vaddw.u8        q13, q13, d13             ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q12                  ; clip pixel
    vqmovun.s16     d13, q13                  ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vst1.64         {d13}, [r9], r8           ; store the data
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vsub.s16        q8, q11, q8               ; step2[7] - step2[8]
    vsub.s16        q9, q10, q9               ; step2[6] - step2[9]

    ; store the data  output 8,9,10,11,12,13,14,15
    vrshr.s16       q8, q8, #6                ; ROUND_POWER_OF_TWO
    vaddw.u8        q8, q8, d12               ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q8                   ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vrshr.s16       q9, q9, #6
    vaddw.u8        q9, q9, d13               ; + dest[j * dest_stride + i]
    vqmovun.s16     d13, q9                   ; clip pixel
    vst1.64         {d13}, [r9], r8           ; store the data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vrshr.s16       q2, q2, #6
    vaddw.u8        q2, q2, d12               ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q2                   ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vrshr.s16       q3, q3, #6
    vaddw.u8        q3, q3, d13               ; + dest[j * dest_stride + i]
    vqmovun.s16     d13, q3                   ; clip pixel
    vst1.64         {d13}, [r9], r8           ; store the data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vrshr.s16       q4, q4, #6
    vaddw.u8        q4, q4, d12               ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q4                   ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vrshr.s16       q5, q5, #6
    vaddw.u8        q5, q5, d13               ; + dest[j * dest_stride + i]
    vqmovun.s16     d13, q5                   ; clip pixel
    vst1.64         {d13}, [r9], r8           ; store the data
    vld1.64         {d13}, [r7], r8           ; load destinatoin data
    vrshr.s16       q14, q14, #6
    vaddw.u8        q14, q14, d12             ; + dest[j * dest_stride + i]
    vqmovun.s16     d12, q14                  ; clip pixel
    vst1.64         {d12}, [r9], r8           ; store the data
    vld1.64         {d12}, [r7], r8           ; load destinatoin data
    vrshr.s16       q15, q15, #6
    vaddw.u8        q15, q15, d13             ; + dest[j * dest_stride + i]
    vqmovun.s16     d13, q15                  ; clip pixel
    vst1.64         {d13}, [r9], r8           ; store the data
    b               end_idct16x16_pass2

skip_adding_dest
    ; stage 7
    ; load the data in pass1
    mov              r5, #24
    mov              r3, #8

    vld1.s16        {q0}, [r2], r4            ; load data step2[0]
    vld1.s16        {q1}, [r2], r4            ; load data step2[1]
    vadd.s16        q12, q0, q15              ; step2[0] + step2[15]
    vadd.s16        q13, q1, q14              ; step2[1] + step2[14]
    vld1.s16        {q10}, [r2], r4           ; load data step2[2]
    vld1.s16        {q11}, [r2], r4           ; load data step2[3]
    vst1.64         {d24}, [r1], r3           ; store output[0]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[1]
    vst1.64         {d27}, [r1], r5
    vadd.s16        q12, q10, q5              ; step2[2] + step2[13]
    vadd.s16        q13, q11, q4              ; step2[3] + step2[12]
    vsub.s16        q14, q1, q14              ; step2[1] - step2[14]
    vsub.s16        q15, q0, q15              ; step2[0] - step2[15]
    vst1.64         {d24}, [r1], r3           ; store output[2]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[3]
    vst1.64         {d27}, [r1], r5
    vsub.s16        q4, q11, q4               ; step2[3] - step2[12]
    vsub.s16        q5, q10, q5               ; step2[2] - step2[13]
    vld1.s16        {q0}, [r2], r4            ; load data step2[4]
    vld1.s16        {q1}, [r2], r4            ; load data step2[5]
    vadd.s16        q12, q0, q3               ; step2[4] + step2[11]
    vadd.s16        q13, q1, q2               ; step2[5] + step2[10]
    vld1.s16        {q10}, [r2], r4           ; load data step2[6]
    vld1.s16        {q11}, [r2], r4           ; load data step2[7]
    vst1.64         {d24}, [r1], r3           ; store output[4]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[5]
    vst1.64         {d27}, [r1], r5
    vadd.s16        q12, q10, q9              ; step2[6] + step2[9]
    vadd.s16        q13, q11, q8              ; step2[7] + step2[8]
    vsub.s16        q2, q1, q2                ; step2[5] - step2[10]
    vsub.s16        q3, q0, q3                ; step2[4] - step2[11]
    vsub.s16        q8, q11, q8               ; step2[7] - step2[8]
    vsub.s16        q9, q10, q9               ; step2[6] - step2[9]
    vst1.64         {d24}, [r1], r3           ; store output[6]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[7]
    vst1.64         {d27}, [r1], r5

    ; store the data  output 8,9,10,11,12,13,14,15
    vst1.64         {d16}, [r1], r3
    vst1.64         {d17}, [r1], r5
    vst1.64         {d18}, [r1], r3
    vst1.64         {d19}, [r1], r5
    vst1.64         {d4}, [r1], r3
    vst1.64         {d5}, [r1], r5
    vst1.64         {d6}, [r1], r3
    vst1.64         {d7}, [r1], r5
    vst1.64         {d8}, [r1], r3
    vst1.64         {d9}, [r1], r5
    vst1.64         {d10}, [r1], r3
    vst1.64         {d11}, [r1], r5
    vst1.64         {d28}, [r1], r3
    vst1.64         {d29}, [r1], r5
    vst1.64         {d30}, [r1], r3
    vst1.64         {d31}, [r1], r5
end_idct16x16_pass2
    pop             {r3-r9}
    bx              lr
    ENDP  ; |vpx_idct16x16_256_add_neon_pass2|

;void |vpx_idct16x16_10_add_neon_pass1|(int16_t *input,
;                                             int16_t *output, int output_stride)
;
; r0  int16_t input
; r1  int16_t *output
; r2  int  output_stride)

; idct16 stage1 - stage6 on all the elements loaded in q8-q15. The output
; will be stored back into q8-q15 registers. This function will touch q0-q7
; registers and use them as buffer during calculation.
|vpx_idct16x16_10_add_neon_pass1| PROC

    ; TODO(hkuang): Find a better way to load the elements.
    ; load elements of 0, 2, 4, 6, 8, 10, 12, 14 into q8 - q15
    vld2.s16        {q8,q9}, [r0]!
    vld2.s16        {q9,q10}, [r0]!
    vld2.s16        {q10,q11}, [r0]!
    vld2.s16        {q11,q12}, [r0]!
    vld2.s16        {q12,q13}, [r0]!
    vld2.s16        {q13,q14}, [r0]!
    vld2.s16        {q14,q15}, [r0]!
    vld2.s16        {q1,q2}, [r0]!
    vmov.s16        q15, q1

    ; generate  cospi_28_64*2 = 6392
    mov             r3, #0x1800
    add             r3, #0xf8

    ; generate cospi_4_64*2  = 32138
    mov             r12, #0x7d00
    add             r12, #0x8a

    ; transpose the input data
    TRANSPOSE8X8

    ; stage 3
    vdup.16         q0, r3                    ; duplicate cospi_28_64*2
    vdup.16         q1, r12                   ; duplicate cospi_4_64*2

    ; The following instructions use vqrdmulh to do the
    ; dct_const_round_shift(step2[4] * cospi_28_64). vvqrdmulh will multiply,
    ; double, and return the high 16 bits, effectively giving >> 15. Doubling
    ; the constant will change this to >> 14.
    ; dct_const_round_shift(step2[4] * cospi_28_64);
    vqrdmulh.s16    q4, q9, q0

    ; preloading to avoid stall
    ; generate cospi_16_64*2 = 23170
    mov             r3, #0x5a00
    add             r3, #0x82

    ; dct_const_round_shift(step2[4] * cospi_4_64);
    vqrdmulh.s16    q7, q9, q1

    ; stage 4
    vdup.16         q1, r3                    ; cospi_16_64*2

    ; generate cospi_16_64 = 11585
    mov             r3, #0x2d00
    add             r3, #0x41

    vdup.16         d4, r3;                   ; duplicate cospi_16_64

    ; dct_const_round_shift(step1[0] * cospi_16_64)
    vqrdmulh.s16    q8, q8, q1

    ; step2[6] * cospi_16_64
    vmull.s16       q9, d14, d4
    vmull.s16       q10, d15, d4

    ; step2[5] * cospi_16_64
    vmull.s16       q12, d9, d4
    vmull.s16       q11, d8, d4

    ; temp1 = (step2[6] - step2[5]) * cospi_16_64
    vsub.s32        q15, q10, q12
    vsub.s32        q6, q9, q11

    ; temp2 = (step2[5] + step2[6]) * cospi_16_64
    vadd.s32        q9, q9, q11
    vadd.s32        q10, q10, q12

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d11, q15, #14             ; >> 14
    vqrshrn.s32     d10, q6, #14              ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d12, q9, #14              ; >> 14
    vqrshrn.s32     d13, q10, #14             ; >> 14

    ; stage 6
    vadd.s16        q2, q8, q7                ; step2[0] = step1[0] + step1[7];
    vadd.s16        q10, q8, q5               ; step2[2] = step1[2] + step1[5];
    vadd.s16        q11, q8, q4               ; step2[3] = step1[3] + step1[4];
    vadd.s16        q9, q8, q6                ; step2[1] = step1[1] + step1[6];
    vsub.s16        q12, q8, q4               ; step2[4] = step1[3] - step1[4];
    vsub.s16        q13, q8, q5               ; step2[5] = step1[2] - step1[5];
    vsub.s16        q14, q8, q6               ; step2[6] = step1[1] - step1[6];
    vsub.s16        q15, q8, q7               ; step2[7] = step1[0] - step1[7];

    ; store the data
    vst1.64         {d4}, [r1], r2
    vst1.64         {d5}, [r1], r2
    vst1.64         {d18}, [r1], r2
    vst1.64         {d19}, [r1], r2
    vst1.64         {d20}, [r1], r2
    vst1.64         {d21}, [r1], r2
    vst1.64         {d22}, [r1], r2
    vst1.64         {d23}, [r1], r2
    vst1.64         {d24}, [r1], r2
    vst1.64         {d25}, [r1], r2
    vst1.64         {d26}, [r1], r2
    vst1.64         {d27}, [r1], r2
    vst1.64         {d28}, [r1], r2
    vst1.64         {d29}, [r1], r2
    vst1.64         {d30}, [r1], r2
    vst1.64         {d31}, [r1], r2

    bx              lr
    ENDP  ; |vpx_idct16x16_10_add_neon_pass1|

;void vpx_idct16x16_10_add_neon_pass2(int16_t *src,
;                                           int16_t *output,
;                                           int16_t *pass1Output,
;                                           int16_t skip_adding,
;                                           uint8_t *dest,
;                                           int dest_stride)
;
; r0  int16_t *src
; r1  int16_t *output,
; r2  int16_t *pass1Output,
; r3  int16_t skip_adding,
; r4  uint8_t *dest,
; r5  int dest_stride)

; idct16 stage1 - stage7 on all the elements loaded in q8-q15. The output
; will be stored back into q8-q15 registers. This function will touch q0-q7
; registers and use them as buffer during calculation.
|vpx_idct16x16_10_add_neon_pass2| PROC
    push            {r3-r9}

    ; TODO(hkuang): Find a better way to load the elements.
    ; load elements of 1, 3, 5, 7, 9, 11, 13, 15 into q8 - q15
    vld2.s16        {q8,q9}, [r0]!
    vld2.s16        {q9,q10}, [r0]!
    vld2.s16        {q10,q11}, [r0]!
    vld2.s16        {q11,q12}, [r0]!
    vld2.s16        {q12,q13}, [r0]!
    vld2.s16        {q13,q14}, [r0]!
    vld2.s16        {q14,q15}, [r0]!
    vld2.s16        {q0,q1}, [r0]!
    vmov.s16        q15, q0;

    ; generate 2*cospi_30_64 = 3212
    mov             r3, #0xc00
    add             r3, #0x8c

    ; generate 2*cospi_2_64  = 32610
    mov             r12, #0x7f00
    add             r12, #0x62

    ; transpose the input data
    TRANSPOSE8X8

    ; stage 3
    vdup.16         q6, r3                    ; duplicate 2*cospi_30_64

    ; dct_const_round_shift(step1[8] * cospi_30_64)
    vqrdmulh.s16    q0, q8, q6

    vdup.16         q6, r12                   ; duplicate 2*cospi_2_64

    ; dct_const_round_shift(step1[8] * cospi_2_64)
    vqrdmulh.s16    q7, q8, q6

    ; preloading to avoid stall
    ; generate 2*cospi_26_64 = 9512
    mov             r12, #0x2500
    add             r12, #0x28
    rsb             r12, #0
    vdup.16         q15, r12                  ; duplicate -2*cospi_26_64

    ; generate 2*cospi_6_64 = 31358
    mov             r3, #0x7a00
    add             r3, #0x7e
    vdup.16         q14, r3                   ; duplicate 2*cospi_6_64

    ; dct_const_round_shift(- step1[12] * cospi_26_64)
    vqrdmulh.s16    q3, q9, q15

    ; dct_const_round_shift(step1[12] * cospi_6_64)
    vqrdmulh.s16    q4, q9, q14

    ; stage 4
    ; generate cospi_24_64 = 6270
    mov             r3, #0x1800
    add             r3, #0x7e
    vdup.16         d31, r3                   ; duplicate cospi_24_64

    ; generate cospi_8_64 = 15137
    mov             r12, #0x3b00
    add             r12, #0x21
    vdup.16         d30, r12                  ; duplicate cospi_8_64

    ; step1[14] * cospi_24_64
    vmull.s16       q12, d14, d31
    vmull.s16       q5, d15, d31

    ; step1[9] * cospi_24_64
    vmull.s16       q2, d0, d31
    vmull.s16       q11, d1, d31

    ; temp1 = -step1[9] * cospi_8_64 + step1[14] * cospi_24_64
    vmlsl.s16       q12, d0, d30
    vmlsl.s16       q5, d1, d30

    ; temp2 = step1[9] * cospi_24_64 + step1[14] * cospi_8_64
    vmlal.s16       q2, d14, d30
    vmlal.s16       q11, d15, d30

    rsb              r12, #0
    vdup.16          d30, r12                 ; duplicate -cospi_8_64

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d2, q12, #14              ; >> 14
    vqrshrn.s32     d3, q5, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d12, q2, #14              ; >> 14
    vqrshrn.s32     d13, q11, #14             ; >> 14

    ; - step1[13] * cospi_8_64
    vmull.s16       q10, d8, d30
    vmull.s16       q13, d9, d30

    ; -step1[10] * cospi_8_64
    vmull.s16       q8, d6, d30
    vmull.s16       q9, d7, d30

    ; temp1 = -step1[10] * cospi_24_64 - step1[13] * cospi_8_64
    vmlsl.s16       q10, d6, d31
    vmlsl.s16       q13, d7, d31

    ; temp2 = -step1[10] * cospi_8_64 + step1[13] * cospi_24_64
    vmlal.s16       q8, d8, d31
    vmlal.s16       q9, d9, d31

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d4, q10, #14              ; >> 14
    vqrshrn.s32     d5, q13, #14              ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d10, q8, #14              ; >> 14
    vqrshrn.s32     d11, q9, #14              ; >> 14

    ; stage 5
    vadd.s16        q8, q0, q3                ; step1[8] = step2[8]+step2[11];
    vadd.s16        q9, q1, q2                ; step1[9] = step2[9]+step2[10];
    vsub.s16        q10, q1, q2               ; step1[10] = step2[9]-step2[10];
    vsub.s16        q11, q0, q3               ; step1[11] = step2[8]-step2[11];
    vsub.s16        q12, q7, q4               ; step1[12] =-step2[12]+step2[15];
    vsub.s16        q13, q6, q5               ; step1[13] =-step2[13]+step2[14];
    vadd.s16        q14, q6, q5               ; step1[14] =step2[13]+step2[14];
    vadd.s16        q15, q7, q4               ; step1[15] =step2[12]+step2[15];

    ; stage 6.
    ; generate cospi_16_64 = 11585
    mov             r12, #0x2d00
    add             r12, #0x41

    vdup.16         d14, r12                  ; duplicate cospi_16_64

    ; step1[13] * cospi_16_64
    vmull.s16       q3, d26, d14
    vmull.s16       q4, d27, d14

    ; step1[10] * cospi_16_64
    vmull.s16       q0, d20, d14
    vmull.s16       q1, d21, d14

    ; temp1 = (-step1[10] + step1[13]) * cospi_16_64
    vsub.s32        q5, q3, q0
    vsub.s32        q6, q4, q1

    ; temp2 = (step1[10] + step1[13]) * cospi_16_64
    vadd.s32        q0, q3, q0
    vadd.s32        q1, q4, q1

    ; dct_const_round_shift(temp1)
    vqrshrn.s32     d4, q5, #14               ; >> 14
    vqrshrn.s32     d5, q6, #14               ; >> 14

    ; dct_const_round_shift(temp2)
    vqrshrn.s32     d10, q0, #14              ; >> 14
    vqrshrn.s32     d11, q1, #14              ; >> 14

    ; step1[11] * cospi_16_64
    vmull.s16       q0, d22, d14
    vmull.s16       q1, d23, d14

    ; step1[12] * cospi_16_64
    vmull.s16       q13, d24, d14
    vmull.s16       q6, d25, d14

    ; temp1 = (-step1[11] + step1[12]) * cospi_16_64
    vsub.s32        q10, q13, q0
    vsub.s32        q4, q6, q1

    ; temp2 = (step1[11] + step1[12]) * cospi_16_64
    vadd.s32        q13, q13, q0
    vadd.s32        q6, q6, q1

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d6, q10, #14              ; >> 14
    vqrshrn.s32     d7, q4, #14               ; >> 14

    ; dct_const_round_shift((step1[11] + step1[12]) * cospi_16_64);
    vqrshrn.s32     d8, q13, #14              ; >> 14
    vqrshrn.s32     d9, q6, #14               ; >> 14

    mov              r4, #16                  ; pass1Output stride
    ldr              r3, [sp]                 ; load skip_adding

    ; stage 7
    ; load the data in pass1
    mov              r5, #24
    mov              r3, #8

    vld1.s16        {q0}, [r2], r4            ; load data step2[0]
    vld1.s16        {q1}, [r2], r4            ; load data step2[1]
    vadd.s16        q12, q0, q15              ; step2[0] + step2[15]
    vadd.s16        q13, q1, q14              ; step2[1] + step2[14]
    vld1.s16        {q10}, [r2], r4           ; load data step2[2]
    vld1.s16        {q11}, [r2], r4           ; load data step2[3]
    vst1.64         {d24}, [r1], r3           ; store output[0]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[1]
    vst1.64         {d27}, [r1], r5
    vadd.s16        q12, q10, q5              ; step2[2] + step2[13]
    vadd.s16        q13, q11, q4              ; step2[3] + step2[12]
    vsub.s16        q14, q1, q14              ; step2[1] - step2[14]
    vsub.s16        q15, q0, q15              ; step2[0] - step2[15]
    vst1.64         {d24}, [r1], r3           ; store output[2]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[3]
    vst1.64         {d27}, [r1], r5
    vsub.s16        q4, q11, q4               ; step2[3] - step2[12]
    vsub.s16        q5, q10, q5               ; step2[2] - step2[13]
    vld1.s16        {q0}, [r2], r4            ; load data step2[4]
    vld1.s16        {q1}, [r2], r4            ; load data step2[5]
    vadd.s16        q12, q0, q3               ; step2[4] + step2[11]
    vadd.s16        q13, q1, q2               ; step2[5] + step2[10]
    vld1.s16        {q10}, [r2], r4           ; load data step2[6]
    vld1.s16        {q11}, [r2], r4           ; load data step2[7]
    vst1.64         {d24}, [r1], r3           ; store output[4]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[5]
    vst1.64         {d27}, [r1], r5
    vadd.s16        q12, q10, q9              ; step2[6] + step2[9]
    vadd.s16        q13, q11, q8              ; step2[7] + step2[8]
    vsub.s16        q2, q1, q2                ; step2[5] - step2[10]
    vsub.s16        q3, q0, q3                ; step2[4] - step2[11]
    vsub.s16        q8, q11, q8               ; step2[7] - step2[8]
    vsub.s16        q9, q10, q9               ; step2[6] - step2[9]
    vst1.64         {d24}, [r1], r3           ; store output[6]
    vst1.64         {d25}, [r1], r5
    vst1.64         {d26}, [r1], r3           ; store output[7]
    vst1.64         {d27}, [r1], r5

    ; store the data  output 8,9,10,11,12,13,14,15
    vst1.64         {d16}, [r1], r3
    vst1.64         {d17}, [r1], r5
    vst1.64         {d18}, [r1], r3
    vst1.64         {d19}, [r1], r5
    vst1.64         {d4}, [r1], r3
    vst1.64         {d5}, [r1], r5
    vst1.64         {d6}, [r1], r3
    vst1.64         {d7}, [r1], r5
    vst1.64         {d8}, [r1], r3
    vst1.64         {d9}, [r1], r5
    vst1.64         {d10}, [r1], r3
    vst1.64         {d11}, [r1], r5
    vst1.64         {d28}, [r1], r3
    vst1.64         {d29}, [r1], r5
    vst1.64         {d30}, [r1], r3
    vst1.64         {d31}, [r1], r5
end_idct10_16x16_pass2
    pop             {r3-r9}
    bx              lr
    ENDP  ; |vpx_idct16x16_10_add_neon_pass2|
    END
