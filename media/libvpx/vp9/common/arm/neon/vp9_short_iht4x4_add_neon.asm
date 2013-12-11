;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT  |vp9_iht4x4_16_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

    ; Parallel 1D IDCT on all the columns of a 4x4 16bits data matrix which are
    ; loaded in d16-d19. d0 must contain cospi_8_64. d1 must contain
    ; cospi_16_64. d2 must contain cospi_24_64. The output will be stored back
    ; into d16-d19 registers. This macro will touch q10- q15 registers and use
    ; them as buffer during calculation.
    MACRO
    IDCT4x4_1D
    ; stage 1
    vadd.s16    d23, d16, d18   ; (input[0] + input[2])
    vsub.s16    d24, d16, d18   ; (input[0] - input[2])

    vmull.s16   q15, d17, d2    ; input[1] * cospi_24_64
    vmull.s16   q10, d17, d0    ; input[1] * cospi_8_64
    vmull.s16   q13, d23, d1    ; (input[0] + input[2]) * cospi_16_64
    vmull.s16   q14, d24, d1    ; (input[0] - input[2]) * cospi_16_64
    vmlsl.s16   q15, d19, d0    ; input[1] * cospi_24_64 - input[3] * cospi_8_64
    vmlal.s16   q10, d19, d2    ; input[1] * cospi_8_64 + input[3] * cospi_24_64

    ; dct_const_round_shift
    vqrshrn.s32 d26, q13, #14
    vqrshrn.s32 d27, q14, #14
    vqrshrn.s32 d29, q15, #14
    vqrshrn.s32 d28, q10, #14

    ; stage 2
    ; output[0] = step[0] + step[3];
    ; output[1] = step[1] + step[2];
    ; output[3] = step[0] - step[3];
    ; output[2] = step[1] - step[2];
    vadd.s16    q8,  q13, q14
    vsub.s16    q9,  q13, q14
    vswp        d18, d19
    MEND

    ; Parallel 1D IADST on all the columns of a 4x4 16bits data matrix which
    ; loaded in d16-d19. d3 must contain sinpi_1_9. d4 must contain sinpi_2_9.
    ; d5 must contain sinpi_4_9. d6 must contain sinpi_3_9. The output will be
    ; stored back into d16-d19 registers. This macro will touch q11,q12,q13,
    ; q14,q15 registers and use them as buffer during calculation.
    MACRO
    IADST4x4_1D
    vmull.s16   q10, d3, d16    ; s0 = sinpi_1_9 * x0
    vmull.s16   q11, d4, d16    ; s1 = sinpi_2_9 * x0
    vmull.s16   q12, d6, d17    ; s2 = sinpi_3_9 * x1
    vmull.s16   q13, d5, d18    ; s3 = sinpi_4_9 * x2
    vmull.s16   q14, d3, d18    ; s4 = sinpi_1_9 * x2
    vmovl.s16   q15, d16        ; expand x0 from 16 bit to 32 bit
    vaddw.s16   q15, q15, d19   ; x0 + x3
    vmull.s16   q8, d4, d19     ; s5 = sinpi_2_9 * x3
    vsubw.s16   q15, q15, d18   ; s7 = x0 + x3 - x2
    vmull.s16   q9, d5, d19     ; s6 = sinpi_4_9 * x3

    vadd.s32    q10, q10, q13   ; x0 = s0 + s3 + s5
    vadd.s32    q10, q10, q8
    vsub.s32    q11, q11, q14   ; x1 = s1 - s4 - s6
    vdup.32     q8, r0          ; duplicate sinpi_3_9
    vsub.s32    q11, q11, q9
    vmul.s32    q15, q15, q8    ; x2 = sinpi_3_9 * s7

    vadd.s32    q13, q10, q12   ; s0 = x0 + x3
    vadd.s32    q10, q10, q11   ; x0 + x1
    vadd.s32    q14, q11, q12   ; s1 = x1 + x3
    vsub.s32    q10, q10, q12   ; s3 = x0 + x1 - x3

    ; dct_const_round_shift
    vqrshrn.s32 d16, q13, #14
    vqrshrn.s32 d17, q14, #14
    vqrshrn.s32 d18, q15, #14
    vqrshrn.s32 d19, q10, #14
    MEND

    ; Generate cosine constants in d6 - d8 for the IDCT
    MACRO
    GENERATE_COSINE_CONSTANTS
    ; cospi_8_64 = 15137 = 0x3b21
    mov         r0, #0x3b00
    add         r0, #0x21
    ; cospi_16_64 = 11585 = 0x2d41
    mov         r3, #0x2d00
    add         r3, #0x41
    ; cospi_24_64 = 6270 = 0x187e
    mov         r12, #0x1800
    add         r12, #0x7e

    ; generate constant vectors
    vdup.16     d0, r0          ; duplicate cospi_8_64
    vdup.16     d1, r3          ; duplicate cospi_16_64
    vdup.16     d2, r12         ; duplicate cospi_24_64
    MEND

    ; Generate sine constants in d1 - d4 for the IADST.
    MACRO
    GENERATE_SINE_CONSTANTS
    ; sinpi_1_9 = 5283 = 0x14A3
    mov         r0, #0x1400
    add         r0, #0xa3
    ; sinpi_2_9 = 9929 = 0x26C9
    mov         r3, #0x2600
    add         r3, #0xc9
    ; sinpi_4_9 = 15212 = 0x3B6C
    mov         r12, #0x3b00
    add         r12, #0x6c

    ; generate constant vectors
    vdup.16     d3, r0          ; duplicate sinpi_1_9

    ; sinpi_3_9 = 13377 = 0x3441
    mov         r0, #0x3400
    add         r0, #0x41

    vdup.16     d4, r3          ; duplicate sinpi_2_9
    vdup.16     d5, r12         ; duplicate sinpi_4_9
    vdup.16     q3, r0          ; duplicate sinpi_3_9
    MEND

    ; Transpose a 4x4 16bits data matrix. Datas are loaded in d16-d19.
    MACRO
    TRANSPOSE4X4
    vtrn.16     d16, d17
    vtrn.16     d18, d19
    vtrn.32     q8, q9
    MEND

    AREA     Block, CODE, READONLY ; name this block of code
;void vp9_iht4x4_16_add_neon(int16_t *input, uint8_t *dest,
;                               int dest_stride, int tx_type)
;
; r0  int16_t input
; r1  uint8_t *dest
; r2  int dest_stride
; r3  int tx_type)
; This function will only handle tx_type of 1,2,3.
|vp9_iht4x4_16_add_neon| PROC

    ; load the inputs into d16-d19
    vld1.s16    {q8,q9}, [r0]!

    ; transpose the input data
    TRANSPOSE4X4

    ; decide the type of transform
    cmp         r3, #2
    beq         idct_iadst
    cmp         r3, #3
    beq         iadst_iadst

iadst_idct
    ; generate constants
    GENERATE_COSINE_CONSTANTS
    GENERATE_SINE_CONSTANTS

    ; first transform rows
    IDCT4x4_1D

    ; transpose the matrix
    TRANSPOSE4X4

    ; then transform columns
    IADST4x4_1D

    b end_vp9_iht4x4_16_add_neon

idct_iadst
    ; generate constants
    GENERATE_COSINE_CONSTANTS
    GENERATE_SINE_CONSTANTS

    ; first transform rows
    IADST4x4_1D

    ; transpose the matrix
    TRANSPOSE4X4

    ; then transform columns
    IDCT4x4_1D

    b end_vp9_iht4x4_16_add_neon

iadst_iadst
    ; generate constants
    GENERATE_SINE_CONSTANTS

    ; first transform rows
    IADST4x4_1D

    ; transpose the matrix
    TRANSPOSE4X4

    ; then transform columns
    IADST4x4_1D

end_vp9_iht4x4_16_add_neon
    ; ROUND_POWER_OF_TWO(temp_out[j], 4)
    vrshr.s16   q8, q8, #4
    vrshr.s16   q9, q9, #4

    vld1.32     {d26[0]}, [r1], r2
    vld1.32     {d26[1]}, [r1], r2
    vld1.32     {d27[0]}, [r1], r2
    vld1.32     {d27[1]}, [r1]

    ; ROUND_POWER_OF_TWO(temp_out[j], 4) + dest[j * dest_stride + i]
    vaddw.u8    q8, q8, d26
    vaddw.u8    q9, q9, d27

    ; clip_pixel
    vqmovun.s16 d26, q8
    vqmovun.s16 d27, q9

    ; do the stores in reverse order with negative post-increment, by changing
    ; the sign of the stride
    rsb         r2, r2, #0
    vst1.32     {d27[1]}, [r1], r2
    vst1.32     {d27[0]}, [r1], r2
    vst1.32     {d26[1]}, [r1], r2
    vst1.32     {d26[0]}, [r1]  ; no post-increment
    bx          lr
    ENDP  ; |vp9_iht4x4_16_add_neon|

    END
