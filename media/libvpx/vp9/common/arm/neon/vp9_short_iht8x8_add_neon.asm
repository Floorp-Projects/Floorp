;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT  |vp9_iht8x8_64_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

    ; Generate IADST constants in r0 - r12 for the IADST.
    MACRO
    GENERATE_IADST_CONSTANTS
    ; generate  cospi_2_64  = 16305
    mov             r0, #0x3f00
    add             r0, #0xb1

    ; generate cospi_30_64 = 1606
    mov             r1, #0x600
    add             r1, #0x46

    ; generate cospi_10_64 = 14449
    mov             r2, #0x3800
    add             r2, #0x71

    ; generate cospi_22_64 = 7723
    mov             r3, #0x1e00
    add             r3, #0x2b

    ; generate cospi_18_64 = 10394
    mov             r4, #0x2800
    add             r4, #0x9a

    ; generate cospi_14_64 = 12665
    mov             r5, #0x3100
    add             r5, #0x79

    ; generate cospi_26_64 = 4756
    mov             r6, #0x1200
    add             r6, #0x94

    ; generate cospi_6_64  = 15679
    mov             r7, #0x3d00
    add             r7, #0x3f

    ; generate cospi_8_64  = 15137
    mov             r8, #0x3b00
    add             r8, #0x21

    ; generate cospi_24_64 = 6270
    mov             r9, #0x1800
    add             r9, #0x7e

    ; generate 0
    mov             r10, #0

    ; generate  cospi_16_64 = 11585
    mov             r12, #0x2d00
    add             r12, #0x41
    MEND

    ; Generate IDCT constants in r3 - r9 for the IDCT.
    MACRO
    GENERATE_IDCT_CONSTANTS
    ; generate  cospi_28_64 = 3196
    mov             r3, #0x0c00
    add             r3, #0x7c

    ; generate cospi_4_64  = 16069
    mov             r4, #0x3e00
    add             r4, #0xc5

    ; generate cospi_12_64 = 13623
    mov             r5, #0x3500
    add             r5, #0x37

    ; generate cospi_20_64 = 9102
    mov             r6, #0x2300
    add             r6, #0x8e

    ; generate cospi_16_64 = 11585
    mov             r7, #0x2d00
    add             r7, #0x41

    ; generate cospi_24_64 = 6270
    mov             r8, #0x1800
    add             r8, #0x7e

    ; generate cospi_8_64 = 15137
    mov             r9, #0x3b00
    add             r9, #0x21
    MEND

    ; Transpose a 8x8 16bits data matrix. Datas are loaded in q8-q15.
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

    ; Parallel 1D IDCT on all the columns of a 8x8 16bits data matrix which are
    ; loaded in q8-q15. The IDCT constants are loaded in r3 - r9. The output
    ; will be stored back into q8-q15 registers. This macro will touch q0-q7
    ; registers and use them as buffer during calculation.
    MACRO
    IDCT8x8_1D
    ; stage 1
    vdup.16         d0, r3                    ; duplicate cospi_28_64
    vdup.16         d1, r4                    ; duplicate cospi_4_64
    vdup.16         d2, r5                    ; duplicate cospi_12_64
    vdup.16         d3, r6                    ; duplicate cospi_20_64

    ; input[1] * cospi_28_64
    vmull.s16       q2, d18, d0
    vmull.s16       q3, d19, d0

    ; input[5] * cospi_12_64
    vmull.s16       q5, d26, d2
    vmull.s16       q6, d27, d2

    ; input[1]*cospi_28_64-input[7]*cospi_4_64
    vmlsl.s16       q2, d30, d1
    vmlsl.s16       q3, d31, d1

    ; input[5] * cospi_12_64 - input[3] * cospi_20_64
    vmlsl.s16       q5, d22, d3
    vmlsl.s16       q6, d23, d3

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d8, q2, #14               ; >> 14
    vqrshrn.s32     d9, q3, #14               ; >> 14

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d10, q5, #14              ; >> 14
    vqrshrn.s32     d11, q6, #14              ; >> 14

    ; input[1] * cospi_4_64
    vmull.s16       q2, d18, d1
    vmull.s16       q3, d19, d1

    ; input[5] * cospi_20_64
    vmull.s16       q9, d26, d3
    vmull.s16       q13, d27, d3

    ; input[1]*cospi_4_64+input[7]*cospi_28_64
    vmlal.s16       q2, d30, d0
    vmlal.s16       q3, d31, d0

    ; input[5] * cospi_20_64 + input[3] * cospi_12_64
    vmlal.s16       q9, d22, d2
    vmlal.s16       q13, d23, d2

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d14, q2, #14              ; >> 14
    vqrshrn.s32     d15, q3, #14              ; >> 14

    ; stage 2 & stage 3 - even half
    vdup.16         d0, r7                    ; duplicate cospi_16_64

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d12, q9, #14              ; >> 14
    vqrshrn.s32     d13, q13, #14             ; >> 14

    ; input[0] * cospi_16_64
    vmull.s16       q2, d16, d0
    vmull.s16       q3, d17, d0

    ; input[0] * cospi_16_64
    vmull.s16       q13, d16, d0
    vmull.s16       q15, d17, d0

    ; (input[0] + input[2]) * cospi_16_64
    vmlal.s16       q2,  d24, d0
    vmlal.s16       q3, d25, d0

    ; (input[0] - input[2]) * cospi_16_64
    vmlsl.s16       q13, d24, d0
    vmlsl.s16       q15, d25, d0

    vdup.16         d0, r8                    ; duplicate cospi_24_64
    vdup.16         d1, r9                    ; duplicate cospi_8_64

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d18, q2, #14              ; >> 14
    vqrshrn.s32     d19, q3, #14              ; >> 14

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d22, q13, #14             ; >> 14
    vqrshrn.s32     d23, q15, #14             ; >> 14

    ; input[1] * cospi_24_64
    vmull.s16       q2, d20, d0
    vmull.s16       q3, d21, d0

    ; input[1] * cospi_8_64
    vmull.s16       q8, d20, d1
    vmull.s16       q12, d21, d1

    ; input[1] * cospi_24_64 - input[3] * cospi_8_64
    vmlsl.s16       q2, d28, d1
    vmlsl.s16       q3, d29, d1

    ; input[1] * cospi_8_64 + input[3] * cospi_24_64
    vmlal.s16       q8, d28, d0
    vmlal.s16       q12, d29, d0

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d26, q2, #14              ; >> 14
    vqrshrn.s32     d27, q3, #14              ; >> 14

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d30, q8, #14              ; >> 14
    vqrshrn.s32     d31, q12, #14             ; >> 14

    vadd.s16        q0, q9, q15               ; output[0] = step[0] + step[3]
    vadd.s16        q1, q11, q13              ; output[1] = step[1] + step[2]
    vsub.s16        q2, q11, q13              ; output[2] = step[1] - step[2]
    vsub.s16        q3, q9, q15               ; output[3] = step[0] - step[3]

    ; stage 3 -odd half
    vdup.16         d16, r7                   ; duplicate cospi_16_64

    ; stage 2 - odd half
    vsub.s16        q13, q4, q5               ; step2[5] = step1[4] - step1[5]
    vadd.s16        q4, q4, q5                ; step2[4] = step1[4] + step1[5]
    vsub.s16        q14, q7, q6               ; step2[6] = -step1[6] + step1[7]
    vadd.s16        q7, q7, q6                ; step2[7] = step1[6] + step1[7]

    ; step2[6] * cospi_16_64
    vmull.s16       q9, d28, d16
    vmull.s16       q10, d29, d16

    ; step2[6] * cospi_16_64
    vmull.s16       q11, d28, d16
    vmull.s16       q12, d29, d16

    ; (step2[6] - step2[5]) * cospi_16_64
    vmlsl.s16       q9, d26, d16
    vmlsl.s16       q10, d27, d16

    ; (step2[5] + step2[6]) * cospi_16_64
    vmlal.s16       q11, d26, d16
    vmlal.s16       q12, d27, d16

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d10, q9, #14              ; >> 14
    vqrshrn.s32     d11, q10, #14             ; >> 14

    ; dct_const_round_shift(input_dc * cospi_16_64)
    vqrshrn.s32     d12, q11, #14             ; >> 14
    vqrshrn.s32     d13, q12, #14             ; >> 14

    ; stage 4
    vadd.s16        q8, q0, q7                ; output[0] = step1[0] + step1[7];
    vadd.s16        q9, q1, q6                ; output[1] = step1[1] + step1[6];
    vadd.s16        q10, q2, q5               ; output[2] = step1[2] + step1[5];
    vadd.s16        q11, q3, q4               ; output[3] = step1[3] + step1[4];
    vsub.s16        q12, q3, q4               ; output[4] = step1[3] - step1[4];
    vsub.s16        q13, q2, q5               ; output[5] = step1[2] - step1[5];
    vsub.s16        q14, q1, q6               ; output[6] = step1[1] - step1[6];
    vsub.s16        q15, q0, q7               ; output[7] = step1[0] - step1[7];
    MEND

    ; Parallel 1D IADST on all the columns of a 8x8 16bits data matrix which
    ; loaded in q8-q15. IADST constants are loaded in r0 - r12 registers. The
    ; output will be stored back into q8-q15 registers. This macro will touch
    ; q0 - q7 registers and use them as buffer during calculation.
    MACRO
    IADST8X8_1D
    vdup.16         d14, r0                   ; duplicate cospi_2_64
    vdup.16         d15, r1                   ; duplicate cospi_30_64

    ; cospi_2_64  * x0
    vmull.s16       q1, d30, d14
    vmull.s16       q2, d31, d14

    ; cospi_30_64 * x0
    vmull.s16       q3, d30, d15
    vmull.s16       q4, d31, d15

    vdup.16         d30, r4                   ; duplicate cospi_18_64
    vdup.16         d31, r5                   ; duplicate cospi_14_64

    ; s0 = cospi_2_64  * x0 + cospi_30_64 * x1;
    vmlal.s16       q1, d16, d15
    vmlal.s16       q2, d17, d15

    ; s1 = cospi_30_64 * x0 - cospi_2_64  * x1
    vmlsl.s16       q3, d16, d14
    vmlsl.s16       q4, d17, d14

    ; cospi_18_64 * x4
    vmull.s16       q5, d22, d30
    vmull.s16       q6, d23, d30

    ; cospi_14_64 * x4
    vmull.s16       q7, d22, d31
    vmull.s16       q8, d23, d31

    ; s4 = cospi_18_64 * x4 + cospi_14_64 * x5;
    vmlal.s16       q5, d24, d31
    vmlal.s16       q6, d25, d31

    ; s5 = cospi_14_64 * x4 - cospi_18_64 * x5
    vmlsl.s16       q7, d24, d30
    vmlsl.s16       q8, d25, d30

    ; (s0 + s4)
    vadd.s32        q11, q1, q5
    vadd.s32        q12, q2, q6

    vdup.16         d0, r2                   ; duplicate cospi_10_64
    vdup.16         d1, r3                   ; duplicate cospi_22_64

    ; (s0 - s4)
    vsub.s32        q1, q1, q5
    vsub.s32        q2, q2, q6

    ; x0 = dct_const_round_shift(s0 + s4);
    vqrshrn.s32     d22, q11, #14             ; >> 14
    vqrshrn.s32     d23, q12, #14             ; >> 14

    ; (s1 + s5)
    vadd.s32        q12, q3, q7
    vadd.s32        q15, q4, q8

    ; (s1 - s5)
    vsub.s32        q3, q3, q7
    vsub.s32        q4, q4, q8

    ; x4 = dct_const_round_shift(s0 - s4);
    vqrshrn.s32     d2, q1, #14               ; >> 14
    vqrshrn.s32     d3, q2, #14               ; >> 14

    ; x1 = dct_const_round_shift(s1 + s5);
    vqrshrn.s32     d24, q12, #14             ; >> 14
    vqrshrn.s32     d25, q15, #14             ; >> 14

    ; x5 = dct_const_round_shift(s1 - s5);
    vqrshrn.s32     d6, q3, #14               ; >> 14
    vqrshrn.s32     d7, q4, #14               ; >> 14

    ; cospi_10_64 * x2
    vmull.s16       q4, d26, d0
    vmull.s16       q5, d27, d0

    ; cospi_22_64 * x2
    vmull.s16       q2, d26, d1
    vmull.s16       q6, d27, d1

    vdup.16         d30, r6                   ; duplicate cospi_26_64
    vdup.16         d31, r7                   ; duplicate cospi_6_64

    ; s2 = cospi_10_64 * x2 + cospi_22_64 * x3;
    vmlal.s16       q4, d20, d1
    vmlal.s16       q5, d21, d1

    ; s3 = cospi_22_64 * x2 - cospi_10_64 * x3;
    vmlsl.s16       q2, d20, d0
    vmlsl.s16       q6, d21, d0

    ; cospi_26_64 * x6
    vmull.s16       q0, d18, d30
    vmull.s16       q13, d19, d30

    ; s6 = cospi_26_64 * x6 + cospi_6_64  * x7;
    vmlal.s16       q0, d28, d31
    vmlal.s16       q13, d29, d31

    ; cospi_6_64  * x6
    vmull.s16       q10, d18, d31
    vmull.s16       q9, d19, d31

    ; s7 = cospi_6_64  * x6 - cospi_26_64 * x7;
    vmlsl.s16       q10, d28, d30
    vmlsl.s16       q9, d29, d30

    ; (s3 + s7)
    vadd.s32        q14, q2, q10
    vadd.s32        q15, q6, q9

    ; (s3 - s7)
    vsub.s32        q2, q2, q10
    vsub.s32        q6, q6, q9

    ; x3 = dct_const_round_shift(s3 + s7);
    vqrshrn.s32     d28, q14, #14             ; >> 14
    vqrshrn.s32     d29, q15, #14             ; >> 14

    ; x7 = dct_const_round_shift(s3 - s7);
    vqrshrn.s32     d4, q2, #14               ; >> 14
    vqrshrn.s32     d5, q6, #14               ; >> 14

    ; (s2 + s6)
    vadd.s32        q9, q4, q0
    vadd.s32        q10, q5, q13

    ; (s2 - s6)
    vsub.s32        q4, q4, q0
    vsub.s32        q5, q5, q13

    vdup.16         d30, r8                   ; duplicate cospi_8_64
    vdup.16         d31, r9                   ; duplicate cospi_24_64

    ; x2 = dct_const_round_shift(s2 + s6);
    vqrshrn.s32     d18, q9, #14              ; >> 14
    vqrshrn.s32     d19, q10, #14             ; >> 14

    ; x6 = dct_const_round_shift(s2 - s6);
    vqrshrn.s32     d8, q4, #14               ; >> 14
    vqrshrn.s32     d9, q5, #14               ; >> 14

    ; cospi_8_64  * x4
    vmull.s16       q5, d2, d30
    vmull.s16       q6, d3, d30

    ; cospi_24_64 * x4
    vmull.s16       q7, d2, d31
    vmull.s16       q0, d3, d31

    ; s4 =  cospi_8_64  * x4 + cospi_24_64 * x5;
    vmlal.s16       q5, d6, d31
    vmlal.s16       q6, d7, d31

    ; s5 =  cospi_24_64 * x4 - cospi_8_64  * x5;
    vmlsl.s16       q7, d6, d30
    vmlsl.s16       q0, d7, d30

    ; cospi_8_64  * x7
    vmull.s16       q1, d4, d30
    vmull.s16       q3, d5, d30

    ; cospi_24_64 * x7
    vmull.s16       q10, d4, d31
    vmull.s16       q2, d5, d31

    ; s6 = -cospi_24_64 * x6 + cospi_8_64  * x7;
    vmlsl.s16       q1, d8, d31
    vmlsl.s16       q3, d9, d31

    ; s7 =  cospi_8_64  * x6 + cospi_24_64 * x7;
    vmlal.s16       q10, d8, d30
    vmlal.s16       q2, d9, d30

    vadd.s16        q8, q11, q9               ; x0 = s0 + s2;

    vsub.s16        q11, q11, q9              ; x2 = s0 - s2;

    vadd.s16        q4, q12, q14              ; x1 = s1 + s3;

    vsub.s16        q12, q12, q14             ; x3 = s1 - s3;

    ; (s4 + s6)
    vadd.s32        q14, q5, q1
    vadd.s32        q15, q6, q3

    ; (s4 - s6)
    vsub.s32        q5, q5, q1
    vsub.s32        q6, q6, q3

    ; x4 = dct_const_round_shift(s4 + s6);
    vqrshrn.s32     d18, q14, #14             ; >> 14
    vqrshrn.s32     d19, q15, #14             ; >> 14

    ; x6 = dct_const_round_shift(s4 - s6);
    vqrshrn.s32     d10, q5, #14              ; >> 14
    vqrshrn.s32     d11, q6, #14              ; >> 14

    ; (s5 + s7)
    vadd.s32        q1, q7, q10
    vadd.s32        q3, q0, q2

    ; (s5 - s7))
    vsub.s32        q7, q7, q10
    vsub.s32        q0, q0, q2

    ; x5 = dct_const_round_shift(s5 + s7);
    vqrshrn.s32     d28, q1, #14               ; >> 14
    vqrshrn.s32     d29, q3, #14               ; >> 14

    ; x7 = dct_const_round_shift(s5 - s7);
    vqrshrn.s32     d14, q7, #14              ; >> 14
    vqrshrn.s32     d15, q0, #14              ; >> 14

    vdup.16         d30, r12                  ; duplicate cospi_16_64

    ; cospi_16_64 * x2
    vmull.s16       q2, d22, d30
    vmull.s16       q3, d23, d30

    ; cospi_6_64  * x6
    vmull.s16       q13, d22, d30
    vmull.s16       q1, d23, d30

    ; cospi_16_64 * x2 + cospi_16_64  * x3;
    vmlal.s16       q2, d24, d30
    vmlal.s16       q3, d25, d30

    ; cospi_16_64 * x2 - cospi_16_64  * x3;
    vmlsl.s16       q13, d24, d30
    vmlsl.s16       q1, d25, d30

    ; x2 = dct_const_round_shift(s2);
    vqrshrn.s32     d4, q2, #14               ; >> 14
    vqrshrn.s32     d5, q3, #14               ; >> 14

    ;x3 = dct_const_round_shift(s3);
    vqrshrn.s32     d24, q13, #14             ; >> 14
    vqrshrn.s32     d25, q1, #14              ; >> 14

    ; cospi_16_64 * x6
    vmull.s16       q13, d10, d30
    vmull.s16       q1, d11, d30

    ; cospi_6_64  * x6
    vmull.s16       q11, d10, d30
    vmull.s16       q0, d11, d30

    ; cospi_16_64 * x6 + cospi_16_64  * x7;
    vmlal.s16       q13, d14, d30
    vmlal.s16       q1, d15, d30

    ; cospi_16_64 * x6 - cospi_16_64  * x7;
    vmlsl.s16       q11, d14, d30
    vmlsl.s16       q0, d15, d30

    ; x6 = dct_const_round_shift(s6);
    vqrshrn.s32     d20, q13, #14             ; >> 14
    vqrshrn.s32     d21, q1, #14              ; >> 14

    ;x7 = dct_const_round_shift(s7);
    vqrshrn.s32     d12, q11, #14             ; >> 14
    vqrshrn.s32     d13, q0, #14              ; >> 14

    vdup.16         q5, r10                   ; duplicate 0

    vsub.s16        q9, q5, q9                ; output[1] = -x4;
    vsub.s16        q11, q5, q2               ; output[3] = -x2;
    vsub.s16        q13, q5, q6               ; output[5] = -x7;
    vsub.s16        q15, q5, q4               ; output[7] = -x1;
    MEND


    AREA     Block, CODE, READONLY ; name this block of code
;void vp9_iht8x8_64_add_neon(int16_t *input, uint8_t *dest,
;                               int dest_stride, int tx_type)
;
; r0  int16_t input
; r1  uint8_t *dest
; r2  int dest_stride
; r3  int tx_type)
; This function will only handle tx_type of 1,2,3.
|vp9_iht8x8_64_add_neon| PROC

    ; load the inputs into d16-d19
    vld1.s16        {q8,q9}, [r0]!
    vld1.s16        {q10,q11}, [r0]!
    vld1.s16        {q12,q13}, [r0]!
    vld1.s16        {q14,q15}, [r0]!

    push            {r0-r10}

    ; transpose the input data
    TRANSPOSE8X8

    ; decide the type of transform
    cmp         r3, #2
    beq         idct_iadst
    cmp         r3, #3
    beq         iadst_iadst

iadst_idct
    ; generate IDCT constants
    GENERATE_IDCT_CONSTANTS

    ; first transform rows
    IDCT8x8_1D

    ; transpose the matrix
    TRANSPOSE8X8

    ; generate IADST constants
    GENERATE_IADST_CONSTANTS

    ; then transform columns
    IADST8X8_1D

    b end_vp9_iht8x8_64_add_neon

idct_iadst
    ; generate IADST constants
    GENERATE_IADST_CONSTANTS

    ; first transform rows
    IADST8X8_1D

    ; transpose the matrix
    TRANSPOSE8X8

    ; generate IDCT constants
    GENERATE_IDCT_CONSTANTS

    ; then transform columns
    IDCT8x8_1D

    b end_vp9_iht8x8_64_add_neon

iadst_iadst
    ; generate IADST constants
    GENERATE_IADST_CONSTANTS

    ; first transform rows
    IADST8X8_1D

    ; transpose the matrix
    TRANSPOSE8X8

    ; then transform columns
    IADST8X8_1D

end_vp9_iht8x8_64_add_neon
    pop            {r0-r10}

    ; ROUND_POWER_OF_TWO(temp_out[j], 5)
    vrshr.s16       q8, q8, #5
    vrshr.s16       q9, q9, #5
    vrshr.s16       q10, q10, #5
    vrshr.s16       q11, q11, #5
    vrshr.s16       q12, q12, #5
    vrshr.s16       q13, q13, #5
    vrshr.s16       q14, q14, #5
    vrshr.s16       q15, q15, #5

    ; save dest pointer
    mov             r0, r1

    ; load destination data
    vld1.64         {d0}, [r1], r2
    vld1.64         {d1}, [r1], r2
    vld1.64         {d2}, [r1], r2
    vld1.64         {d3}, [r1], r2
    vld1.64         {d4}, [r1], r2
    vld1.64         {d5}, [r1], r2
    vld1.64         {d6}, [r1], r2
    vld1.64         {d7}, [r1]

    ; ROUND_POWER_OF_TWO(temp_out[j], 5) + dest[j * dest_stride + i]
    vaddw.u8        q8, q8, d0
    vaddw.u8        q9, q9, d1
    vaddw.u8        q10, q10, d2
    vaddw.u8        q11, q11, d3
    vaddw.u8        q12, q12, d4
    vaddw.u8        q13, q13, d5
    vaddw.u8        q14, q14, d6
    vaddw.u8        q15, q15, d7

    ; clip_pixel
    vqmovun.s16     d0, q8
    vqmovun.s16     d1, q9
    vqmovun.s16     d2, q10
    vqmovun.s16     d3, q11
    vqmovun.s16     d4, q12
    vqmovun.s16     d5, q13
    vqmovun.s16     d6, q14
    vqmovun.s16     d7, q15

    ; store the data
    vst1.64         {d0}, [r0], r2
    vst1.64         {d1}, [r0], r2
    vst1.64         {d2}, [r0], r2
    vst1.64         {d3}, [r0], r2
    vst1.64         {d4}, [r0], r2
    vst1.64         {d5}, [r0], r2
    vst1.64         {d6}, [r0], r2
    vst1.64         {d7}, [r0], r2
    bx          lr
    ENDP  ; |vp9_iht8x8_64_add_neon|

    END
