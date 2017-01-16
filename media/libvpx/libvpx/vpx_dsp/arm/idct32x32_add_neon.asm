;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

;TODO(cd): adjust these constant to be able to use vqdmulh for faster
;          dct_const_round_shift(a * b) within butterfly calculations.
cospi_1_64  EQU 16364
cospi_2_64  EQU 16305
cospi_3_64  EQU 16207
cospi_4_64  EQU 16069
cospi_5_64  EQU 15893
cospi_6_64  EQU 15679
cospi_7_64  EQU 15426
cospi_8_64  EQU 15137
cospi_9_64  EQU 14811
cospi_10_64 EQU 14449
cospi_11_64 EQU 14053
cospi_12_64 EQU 13623
cospi_13_64 EQU 13160
cospi_14_64 EQU 12665
cospi_15_64 EQU 12140
cospi_16_64 EQU 11585
cospi_17_64 EQU 11003
cospi_18_64 EQU 10394
cospi_19_64 EQU  9760
cospi_20_64 EQU  9102
cospi_21_64 EQU  8423
cospi_22_64 EQU  7723
cospi_23_64 EQU  7005
cospi_24_64 EQU  6270
cospi_25_64 EQU  5520
cospi_26_64 EQU  4756
cospi_27_64 EQU  3981
cospi_28_64 EQU  3196
cospi_29_64 EQU  2404
cospi_30_64 EQU  1606
cospi_31_64 EQU   804


    EXPORT  |vpx_idct32x32_1024_add_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

    AREA     Block, CODE, READONLY

    ; --------------------------------------------------------------------------
    ; Load from transposed_buffer
    ;   q13 = transposed_buffer[first_offset]
    ;   q14 = transposed_buffer[second_offset]
    ;   for proper address calculation, the last offset used when manipulating
    ;   transposed_buffer must be passed in. use 0 for first use.
    MACRO
    LOAD_FROM_TRANSPOSED $prev_offset, $first_offset, $second_offset
    ; address calculation with proper stride and loading
    add r0, #($first_offset  - $prev_offset )*8*2
    vld1.s16        {q14}, [r0]
    add r0, #($second_offset - $first_offset)*8*2
    vld1.s16        {q13}, [r0]
    ; (used) two registers (q14, q13)
    MEND
    ; --------------------------------------------------------------------------
    ; Load from output (used as temporary storage)
    ;   reg1 = output[first_offset]
    ;   reg2 = output[second_offset]
    ;   for proper address calculation, the last offset used when manipulating
    ;   output, whether reading or storing) must be passed in. use 0 for first
    ;   use.
    MACRO
    LOAD_FROM_OUTPUT $prev_offset, $first_offset, $second_offset, $reg1, $reg2
    ; address calculation with proper stride and loading
    add r1, #($first_offset  - $prev_offset )*32*2
    vld1.s16        {$reg1}, [r1]
    add r1, #($second_offset - $first_offset)*32*2
    vld1.s16        {$reg2}, [r1]
    ; (used) two registers ($reg1, $reg2)
    MEND
    ; --------------------------------------------------------------------------
    ; Store into output (sometimes as as temporary storage)
    ;   output[first_offset] = reg1
    ;   output[second_offset] = reg2
    ;   for proper address calculation, the last offset used when manipulating
    ;   output, whether reading or storing) must be passed in. use 0 for first
    ;   use.
    MACRO
    STORE_IN_OUTPUT $prev_offset, $first_offset, $second_offset, $reg1, $reg2
    ; address calculation with proper stride and storing
    add r1, #($first_offset  - $prev_offset )*32*2
    vst1.16 {$reg1}, [r1]
    add r1, #($second_offset - $first_offset)*32*2
    vst1.16 {$reg2}, [r1]
    MEND
    ; --------------------------------------------------------------------------
    ; Combine-add results with current destination content
    ;   q6-q9 contain the results (out[j * 32 + 0-31])
    MACRO
    STORE_COMBINE_CENTER_RESULTS
    ; load dest[j * dest_stride + 0-31]
    vld1.s16        {d8}, [r10], r2
    vld1.s16        {d11}, [r9], r11
    vld1.s16        {d9}, [r10]
    vld1.s16        {d10}, [r9]
    ; ROUND_POWER_OF_TWO
    vrshr.s16       q7, q7, #6
    vrshr.s16       q8, q8, #6
    vrshr.s16       q9, q9, #6
    vrshr.s16       q6, q6, #6
    ; add to dest[j * dest_stride + 0-31]
    vaddw.u8        q7, q7, d9
    vaddw.u8        q8, q8, d10
    vaddw.u8        q9, q9, d11
    vaddw.u8        q6, q6, d8
    ; clip pixel
    vqmovun.s16     d9,  q7
    vqmovun.s16     d10, q8
    vqmovun.s16     d11, q9
    vqmovun.s16     d8,  q6
    ; store back into dest[j * dest_stride + 0-31]
    vst1.16         {d9}, [r10], r11
    vst1.16         {d10}, [r9], r2
    vst1.16         {d8}, [r10]
    vst1.16         {d11}, [r9]
    ; update pointers (by dest_stride * 2)
    sub r9,  r9,  r2, lsl #1
    add r10, r10, r2, lsl #1
    MEND
    ; --------------------------------------------------------------------------
    ; Combine-add results with current destination content
    ;   q6-q9 contain the results (out[j * 32 + 0-31])
    MACRO
    STORE_COMBINE_CENTER_RESULTS_LAST
    ; load dest[j * dest_stride + 0-31]
    vld1.s16        {d8}, [r10], r2
    vld1.s16        {d11}, [r9], r11
    vld1.s16        {d9}, [r10]
    vld1.s16        {d10}, [r9]
    ; ROUND_POWER_OF_TWO
    vrshr.s16       q7, q7, #6
    vrshr.s16       q8, q8, #6
    vrshr.s16       q9, q9, #6
    vrshr.s16       q6, q6, #6
    ; add to dest[j * dest_stride + 0-31]
    vaddw.u8        q7, q7, d9
    vaddw.u8        q8, q8, d10
    vaddw.u8        q9, q9, d11
    vaddw.u8        q6, q6, d8
    ; clip pixel
    vqmovun.s16     d9,  q7
    vqmovun.s16     d10, q8
    vqmovun.s16     d11, q9
    vqmovun.s16     d8,  q6
    ; store back into dest[j * dest_stride + 0-31]
    vst1.16         {d9}, [r10], r11
    vst1.16         {d10}, [r9], r2
    vst1.16         {d8}, [r10]!
    vst1.16         {d11}, [r9]!
    ; update pointers (by dest_stride * 2)
    sub r9,  r9,  r2, lsl #1
    add r10, r10, r2, lsl #1
    MEND
    ; --------------------------------------------------------------------------
    ; Combine-add results with current destination content
    ;   q4-q7 contain the results (out[j * 32 + 0-31])
    MACRO
    STORE_COMBINE_EXTREME_RESULTS
    ; load dest[j * dest_stride + 0-31]
    vld1.s16        {d4}, [r7], r2
    vld1.s16        {d7}, [r6], r11
    vld1.s16        {d5}, [r7]
    vld1.s16        {d6}, [r6]
    ; ROUND_POWER_OF_TWO
    vrshr.s16       q5, q5, #6
    vrshr.s16       q6, q6, #6
    vrshr.s16       q7, q7, #6
    vrshr.s16       q4, q4, #6
    ; add to dest[j * dest_stride + 0-31]
    vaddw.u8        q5, q5, d5
    vaddw.u8        q6, q6, d6
    vaddw.u8        q7, q7, d7
    vaddw.u8        q4, q4, d4
    ; clip pixel
    vqmovun.s16     d5, q5
    vqmovun.s16     d6, q6
    vqmovun.s16     d7, q7
    vqmovun.s16     d4, q4
    ; store back into dest[j * dest_stride + 0-31]
    vst1.16         {d5}, [r7], r11
    vst1.16         {d6}, [r6], r2
    vst1.16         {d7}, [r6]
    vst1.16         {d4}, [r7]
    ; update pointers (by dest_stride * 2)
    sub r6, r6, r2, lsl #1
    add r7, r7, r2, lsl #1
    MEND
    ; --------------------------------------------------------------------------
    ; Combine-add results with current destination content
    ;   q4-q7 contain the results (out[j * 32 + 0-31])
    MACRO
    STORE_COMBINE_EXTREME_RESULTS_LAST
    ; load dest[j * dest_stride + 0-31]
    vld1.s16        {d4}, [r7], r2
    vld1.s16        {d7}, [r6], r11
    vld1.s16        {d5}, [r7]
    vld1.s16        {d6}, [r6]
    ; ROUND_POWER_OF_TWO
    vrshr.s16       q5, q5, #6
    vrshr.s16       q6, q6, #6
    vrshr.s16       q7, q7, #6
    vrshr.s16       q4, q4, #6
    ; add to dest[j * dest_stride + 0-31]
    vaddw.u8        q5, q5, d5
    vaddw.u8        q6, q6, d6
    vaddw.u8        q7, q7, d7
    vaddw.u8        q4, q4, d4
    ; clip pixel
    vqmovun.s16     d5, q5
    vqmovun.s16     d6, q6
    vqmovun.s16     d7, q7
    vqmovun.s16     d4, q4
    ; store back into dest[j * dest_stride + 0-31]
    vst1.16         {d5}, [r7], r11
    vst1.16         {d6}, [r6], r2
    vst1.16         {d7}, [r6]!
    vst1.16         {d4}, [r7]!
    ; update pointers (by dest_stride * 2)
    sub r6, r6, r2, lsl #1
    add r7, r7, r2, lsl #1
    MEND
    ; --------------------------------------------------------------------------
    ; Touches q8-q12, q15 (q13-q14 are preserved)
    ; valid output registers are anything but q8-q11
    MACRO
    DO_BUTTERFLY $regC, $regD, $regA, $regB, $first_constant, $second_constant, $reg1, $reg2, $reg3, $reg4
    ; TODO(cd): have special case to re-use constants when they are similar for
    ;           consecutive butterflies
    ; TODO(cd): have special case when both constants are the same, do the
    ;           additions/subtractions before the multiplies.
    ; generate the constants
    ;   generate scalar constants
    mov             r8,  #$first_constant  & 0xFF00
    mov             r12, #$second_constant & 0xFF00
    add             r8,  #$first_constant  & 0x00FF
    add             r12, #$second_constant & 0x00FF
    ;   generate vector constants
    vdup.16         d30, r8
    vdup.16         d31, r12
    ; (used) two for inputs (regA-regD), one for constants (q15)
    ; do some multiplications (ordered for maximum latency hiding)
    vmull.s16 q8,  $regC, d30
    vmull.s16 q10, $regA, d31
    vmull.s16 q9,  $regD, d30
    vmull.s16 q11, $regB, d31
    vmull.s16 q12, $regC, d31
    ; (used) five for intermediate (q8-q12), one for constants (q15)
    ; do some addition/subtractions (to get back two register)
    vsub.s32  q8, q8, q10
    vsub.s32  q9, q9, q11
    ; do more multiplications (ordered for maximum latency hiding)
    vmull.s16 q10, $regD, d31
    vmull.s16 q11, $regA, d30
    vmull.s16 q15, $regB, d30
    ; (used) six for intermediate (q8-q12, q15)
    ; do more addition/subtractions
    vadd.s32  q11, q12, q11
    vadd.s32  q10, q10, q15
    ; (used) four for intermediate (q8-q11)
    ; dct_const_round_shift
    vqrshrn.s32 $reg1, q8,  #14
    vqrshrn.s32 $reg2, q9,  #14
    vqrshrn.s32 $reg3, q11, #14
    vqrshrn.s32 $reg4, q10, #14
    ; (used) two for results, well four d registers
    MEND
    ; --------------------------------------------------------------------------
    ; Touches q8-q12, q15 (q13-q14 are preserved)
    ; valid output registers are anything but q8-q11
    MACRO
    DO_BUTTERFLY_STD $first_constant, $second_constant, $reg1, $reg2, $reg3, $reg4
    DO_BUTTERFLY d28, d29, d26, d27, $first_constant, $second_constant, $reg1, $reg2, $reg3, $reg4
    MEND
    ; --------------------------------------------------------------------------

;void vpx_idct32x32_1024_add_neon(int16_t *input, uint8_t *dest, int dest_stride);
;
;   r0  int16_t *input,
;   r1  uint8_t *dest,
;   r2  int dest_stride)
; loop counters
;   r4  bands loop counter
;   r5  pass loop counter
;   r8  transpose loop counter
; combine-add pointers
;   r6  dest + 31 * dest_stride, descending (30, 29, 28, ...)
;   r7  dest +  0 * dest_stride, ascending  (1, 2, 3, ...)
;   r9  dest + 15 * dest_stride, descending (14, 13, 12, ...)
;   r10 dest + 16 * dest_stride, ascending  (17, 18, 19, ...)

|vpx_idct32x32_1024_add_neon| PROC
    ; This function does one pass of idct32x32 transform.
    ;
    ; This is done by transposing the input and then doing a 1d transform on
    ; columns. In the first pass, the transposed columns are the original
    ; rows. In the second pass, after the transposition, the colums are the
    ; original columns.
    ; The 1d transform is done by looping over bands of eight columns (the
    ; idct32_bands loop). For each band, the transform input transposition
    ; is done on demand, one band of four 8x8 matrices at a time. The four
    ; matrices are transposed by pairs (the idct32_transpose_pair loop).
    push  {r4-r11}
    vpush {d8-d15}
    ; stack operation
    ; internal buffer used to transpose 8 lines into before transforming them
    ;   int16_t transpose_buffer[32 * 8];
    ;   at sp + [4096, 4607]
    ; results of the first pass (transpose and transform rows)
    ;   int16_t pass1[32 * 32];
    ;   at sp + [0, 2047]
    ; results of the second pass (transpose and transform columns)
    ;   int16_t pass2[32 * 32];
    ;   at sp + [2048, 4095]
    sub sp, sp, #512+2048+2048

    ; r6  = dest + 31 * dest_stride
    ; r7  = dest +  0 * dest_stride
    ; r9  = dest + 15 * dest_stride
    ; r10 = dest + 16 * dest_stride
    rsb r6,  r2, r2, lsl #5
    rsb r9,  r2, r2, lsl #4
    add r10, r1, r2, lsl #4
    mov r7, r1
    add r6, r6, r1
    add r9, r9, r1
    ; r11 = -dest_stride
    neg r11, r2
    ; r3 = input
    mov r3, r0
    ; parameters for first pass
      ; r0 = transpose_buffer[32 * 8]
    add r0, sp, #4096
      ; r1 = pass1[32 * 32]
    mov r1, sp

    mov r5, #0          ; initialize pass loop counter
idct32_pass_loop
    mov r4, #4          ; initialize bands loop counter
idct32_bands_loop
    mov r8, #2          ; initialize transpose loop counter
idct32_transpose_pair_loop
    ; Load two horizontally consecutive 8x8 16bit data matrices. The first one
    ; into q0-q7 and the second one into q8-q15. There is a stride of 64,
    ; adjusted to 32 because of the two post-increments.
    vld1.s16        {q8},  [r3]!
    vld1.s16        {q0},  [r3]!
    add r3, #32
    vld1.s16        {q9},  [r3]!
    vld1.s16        {q1},  [r3]!
    add r3, #32
    vld1.s16        {q10}, [r3]!
    vld1.s16        {q2},  [r3]!
    add r3, #32
    vld1.s16        {q11}, [r3]!
    vld1.s16        {q3},  [r3]!
    add r3, #32
    vld1.s16        {q12}, [r3]!
    vld1.s16        {q4},  [r3]!
    add r3, #32
    vld1.s16        {q13}, [r3]!
    vld1.s16        {q5},  [r3]!
    add r3, #32
    vld1.s16        {q14}, [r3]!
    vld1.s16        {q6},  [r3]!
    add r3, #32
    vld1.s16        {q15}, [r3]!
    vld1.s16        {q7},  [r3]!

    ; Transpose the two 8x8 16bit data matrices.
    vswp            d17, d24
    vswp            d23, d30
    vswp            d21, d28
    vswp            d19, d26
    vswp            d1,  d8
    vswp            d7,  d14
    vswp            d5,  d12
    vswp            d3,  d10
    vtrn.32         q8,  q10
    vtrn.32         q9,  q11
    vtrn.32         q12, q14
    vtrn.32         q13, q15
    vtrn.32         q0,  q2
    vtrn.32         q1,  q3
    vtrn.32         q4,  q6
    vtrn.32         q5,  q7
    vtrn.16         q8,  q9
    vtrn.16         q10, q11
    vtrn.16         q12, q13
    vtrn.16         q14, q15
    vtrn.16         q0,  q1
    vtrn.16         q2,  q3
    vtrn.16         q4,  q5
    vtrn.16         q6,  q7

    ; Store both matrices after each other. There is a stride of 32, which
    ; adjusts to nothing because of the post-increments.
    vst1.16        {q8},  [r0]!
    vst1.16        {q9},  [r0]!
    vst1.16        {q10}, [r0]!
    vst1.16        {q11}, [r0]!
    vst1.16        {q12}, [r0]!
    vst1.16        {q13}, [r0]!
    vst1.16        {q14}, [r0]!
    vst1.16        {q15}, [r0]!
    vst1.16        {q0},  [r0]!
    vst1.16        {q1},  [r0]!
    vst1.16        {q2},  [r0]!
    vst1.16        {q3},  [r0]!
    vst1.16        {q4},  [r0]!
    vst1.16        {q5},  [r0]!
    vst1.16        {q6},  [r0]!
    vst1.16        {q7},  [r0]!

    ; increment pointers by adjusted stride (not necessary for r0/out)
    ;   go back by 7*32 for the seven lines moved fully by read and add
    ;   go back by 32 for the eigth line only read
    ;   advance by 16*2 to go the next pair
    sub r3,  r3,  #7*32*2 + 32 - 16*2
    ; transpose pair loop processing
    subs r8, r8, #1
    bne idct32_transpose_pair_loop

    ; restore r0/input to its original value
    sub r0, r0, #32*8*2

    ; Instead of doing the transforms stage by stage, it is done by loading
    ; some input values and doing as many stages as possible to minimize the
    ; storing/loading of intermediate results. To fit within registers, the
    ; final coefficients are cut into four blocks:
    ; BLOCK A: 16-19,28-31
    ; BLOCK B: 20-23,24-27
    ; BLOCK C: 8-10,11-15
    ; BLOCK D: 0-3,4-7
    ; Blocks A and C are straight calculation through the various stages. In
    ; block B, further calculations are performed using the results from
    ; block A. In block D, further calculations are performed using the results
    ; from block C and then the final calculations are done using results from
    ; block A and B which have been combined at the end of block B.

    ; --------------------------------------------------------------------------
    ; BLOCK A: 16-19,28-31
    ; --------------------------------------------------------------------------
    ; generate 16,17,30,31
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[1 * 32] * cospi_31_64 - input[31 * 32] *  cospi_1_64;
    ;temp2 = input[1 * 32] *  cospi_1_64 + input[31 * 32] * cospi_31_64;
    ;step1b[16][i] = dct_const_round_shift(temp1);
    ;step1b[31][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 0, 1, 31
    DO_BUTTERFLY_STD cospi_31_64, cospi_1_64, d0, d1, d4, d5
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[17 * 32] * cospi_15_64 - input[15 * 32] * cospi_17_64;
    ;temp2 = input[17 * 32] * cospi_17_64 + input[15 * 32] * cospi_15_64;
    ;step1b[17][i] = dct_const_round_shift(temp1);
    ;step1b[30][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 31, 17, 15
    DO_BUTTERFLY_STD cospi_15_64, cospi_17_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;step2[16] =  step1b[16][i] + step1b[17][i];
    ;step2[17] =  step1b[16][i] - step1b[17][i];
    ;step2[30] = -step1b[30][i] + step1b[31][i];
    ;step2[31] =  step1b[30][i] + step1b[31][i];
    vadd.s16  q4, q0, q1
    vsub.s16  q13, q0, q1
    vadd.s16  q6, q2, q3
    vsub.s16  q14, q2, q3
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;temp1 = step1b[30][i] * cospi_28_64 - step1b[17][i] * cospi_4_64;
    ;temp2 = step1b[30][i] * cospi_4_64  - step1b[17][i] * cospi_28_64;
    ;step3[17] = dct_const_round_shift(temp1);
    ;step3[30] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_28_64, cospi_4_64, d10, d11, d14, d15
    ; --------------------------------------------------------------------------
    ; generate 18,19,28,29
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[9 * 32] * cospi_23_64 - input[23 * 32] * cospi_9_64;
    ;temp2 = input[9 * 32] *  cospi_9_64 + input[23 * 32] * cospi_23_64;
    ;step1b[18][i] = dct_const_round_shift(temp1);
    ;step1b[29][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 15, 9, 23
    DO_BUTTERFLY_STD cospi_23_64, cospi_9_64, d0, d1, d4, d5
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[25 * 32] *  cospi_7_64 - input[7 * 32] * cospi_25_64;
    ;temp2 = input[25 * 32] * cospi_25_64 + input[7 * 32] * cospi_7_64;
    ;step1b[19][i] = dct_const_round_shift(temp1);
    ;step1b[28][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 23, 25, 7
    DO_BUTTERFLY_STD cospi_7_64, cospi_25_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;step2[18] = -step1b[18][i] + step1b[19][i];
    ;step2[19] =  step1b[18][i] + step1b[19][i];
    ;step2[28] =  step1b[28][i] + step1b[29][i];
    ;step2[29] =  step1b[28][i] - step1b[29][i];
    vsub.s16  q13, q3, q2
    vadd.s16  q3,  q3, q2
    vsub.s16  q14, q1, q0
    vadd.s16  q2,  q1, q0
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;temp1 = step1b[18][i] * (-cospi_4_64)  - step1b[29][i] * (-cospi_28_64);
    ;temp2 = step1b[18][i] * (-cospi_28_64) + step1b[29][i] * (-cospi_4_64);
    ;step3[29] = dct_const_round_shift(temp1);
    ;step3[18] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD (-cospi_4_64), (-cospi_28_64), d2, d3, d0, d1
    ; --------------------------------------------------------------------------
    ; combine 16-19,28-31
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;step1[16] = step1b[16][i] + step1b[19][i];
    ;step1[17] = step1b[17][i] + step1b[18][i];
    ;step1[18] = step1b[17][i] - step1b[18][i];
    ;step1[29] = step1b[30][i] - step1b[29][i];
    ;step1[30] = step1b[30][i] + step1b[29][i];
    ;step1[31] = step1b[31][i] + step1b[28][i];
    vadd.s16  q8,  q4, q2
    vadd.s16  q9,  q5, q0
    vadd.s16  q10, q7, q1
    vadd.s16  q15, q6, q3
    vsub.s16  q13, q5, q0
    vsub.s16  q14, q7, q1
    STORE_IN_OUTPUT 0,  16, 31, q8,  q15
    STORE_IN_OUTPUT 31, 17, 30, q9,  q10
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;temp1 = step1b[29][i] * cospi_24_64 - step1b[18][i] * cospi_8_64;
    ;temp2 = step1b[29][i] * cospi_8_64  + step1b[18][i] * cospi_24_64;
    ;step2[18] = dct_const_round_shift(temp1);
    ;step2[29] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_24_64, cospi_8_64, d0, d1, d2, d3
    STORE_IN_OUTPUT 30, 29, 18, q1, q0
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;step1[19] = step1b[16][i] - step1b[19][i];
    ;step1[28] = step1b[31][i] - step1b[28][i];
    vsub.s16  q13, q4, q2
    vsub.s16  q14, q6, q3
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;temp1 = step1b[28][i] * cospi_24_64 - step1b[19][i] * cospi_8_64;
    ;temp2 = step1b[28][i] * cospi_8_64  + step1b[19][i] * cospi_24_64;
    ;step2[19] = dct_const_round_shift(temp1);
    ;step2[28] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_24_64, cospi_8_64, d8, d9, d12, d13
    STORE_IN_OUTPUT 18, 19, 28, q4, q6
    ; --------------------------------------------------------------------------


    ; --------------------------------------------------------------------------
    ; BLOCK B: 20-23,24-27
    ; --------------------------------------------------------------------------
    ; generate 20,21,26,27
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[5 * 32] * cospi_27_64 - input[27 * 32] * cospi_5_64;
    ;temp2 = input[5 * 32] *  cospi_5_64 + input[27 * 32] * cospi_27_64;
    ;step1b[20][i] = dct_const_round_shift(temp1);
    ;step1b[27][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 7, 5, 27
    DO_BUTTERFLY_STD cospi_27_64, cospi_5_64, d0, d1, d4, d5
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[21 * 32] * cospi_11_64 - input[11 * 32] * cospi_21_64;
    ;temp2 = input[21 * 32] * cospi_21_64 + input[11 * 32] * cospi_11_64;
    ;step1b[21][i] = dct_const_round_shift(temp1);
    ;step1b[26][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 27, 21, 11
    DO_BUTTERFLY_STD cospi_11_64, cospi_21_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;step2[20] =  step1b[20][i] + step1b[21][i];
    ;step2[21] =  step1b[20][i] - step1b[21][i];
    ;step2[26] = -step1b[26][i] + step1b[27][i];
    ;step2[27] =  step1b[26][i] + step1b[27][i];
    vsub.s16  q13, q0, q1
    vadd.s16  q0, q0, q1
    vsub.s16  q14, q2, q3
    vadd.s16  q2, q2, q3
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;temp1 = step1b[26][i] * cospi_12_64 - step1b[21][i] * cospi_20_64;
    ;temp2 = step1b[26][i] * cospi_20_64 + step1b[21][i] * cospi_12_64;
    ;step3[21] = dct_const_round_shift(temp1);
    ;step3[26] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_12_64, cospi_20_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; generate 22,23,24,25
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[13 * 32] * cospi_19_64 - input[19 * 32] * cospi_13_64;
    ;temp2 = input[13 * 32] * cospi_13_64 + input[19 * 32] * cospi_19_64;
    ;step1b[22][i] = dct_const_round_shift(temp1);
    ;step1b[25][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 11, 13, 19
    DO_BUTTERFLY_STD cospi_19_64, cospi_13_64, d10, d11, d14, d15
    ; --------------------------------------------------------------------------
    ; part of stage 1
    ;temp1 = input[29 * 32] *  cospi_3_64 - input[3 * 32] * cospi_29_64;
    ;temp2 = input[29 * 32] * cospi_29_64 + input[3 * 32] * cospi_3_64;
    ;step1b[23][i] = dct_const_round_shift(temp1);
    ;step1b[24][i] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 19, 29, 3
    DO_BUTTERFLY_STD cospi_3_64, cospi_29_64, d8, d9, d12, d13
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;step2[22] = -step1b[22][i] + step1b[23][i];
    ;step2[23] =  step1b[22][i] + step1b[23][i];
    ;step2[24] =  step1b[24][i] + step1b[25][i];
    ;step2[25] =  step1b[24][i] - step1b[25][i];
    vsub.s16  q14, q4, q5
    vadd.s16  q5, q4, q5
    vsub.s16  q13, q6, q7
    vadd.s16  q6, q6, q7
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;temp1 = step1b[22][i] * (-cospi_20_64) - step1b[25][i] * (-cospi_12_64);
    ;temp2 = step1b[22][i] * (-cospi_12_64) + step1b[25][i] * (-cospi_20_64);
    ;step3[25] = dct_const_round_shift(temp1);
    ;step3[22] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD (-cospi_20_64), (-cospi_12_64), d8, d9, d14, d15
    ; --------------------------------------------------------------------------
    ; combine 20-23,24-27
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;step1[22] = step1b[22][i] + step1b[21][i];
    ;step1[23] = step1b[23][i] + step1b[20][i];
    vadd.s16  q10, q7, q1
    vadd.s16  q11, q5, q0
    ;step1[24] = step1b[24][i] + step1b[27][i];
    ;step1[25] = step1b[25][i] + step1b[26][i];
    vadd.s16  q12, q6, q2
    vadd.s16  q15, q4, q3
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;step3[16] = step1b[16][i] + step1b[23][i];
    ;step3[17] = step1b[17][i] + step1b[22][i];
    ;step3[22] = step1b[17][i] - step1b[22][i];
    ;step3[23] = step1b[16][i] - step1b[23][i];
    LOAD_FROM_OUTPUT 28, 16, 17, q14, q13
    vadd.s16  q8,  q14, q11
    vadd.s16  q9,  q13, q10
    vsub.s16  q13, q13, q10
    vsub.s16  q11, q14, q11
    STORE_IN_OUTPUT 17, 17, 16, q9, q8
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;step3[24] = step1b[31][i] - step1b[24][i];
    ;step3[25] = step1b[30][i] - step1b[25][i];
    ;step3[30] = step1b[30][i] + step1b[25][i];
    ;step3[31] = step1b[31][i] + step1b[24][i];
    LOAD_FROM_OUTPUT 16, 30, 31, q14, q9
    vsub.s16  q8,  q9,  q12
    vadd.s16  q10, q14, q15
    vsub.s16  q14, q14, q15
    vadd.s16  q12, q9,  q12
    STORE_IN_OUTPUT 31, 30, 31, q10, q12
    ; --------------------------------------------------------------------------
    ; TODO(cd) do some register allocation change to remove these push/pop
    vpush {q8}  ; [24]
    vpush {q11} ; [23]
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;temp1 = (step1b[25][i] - step1b[22][i]) * cospi_16_64;
    ;temp2 = (step1b[25][i] + step1b[22][i]) * cospi_16_64;
    ;step1[22] = dct_const_round_shift(temp1);
    ;step1[25] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d26, d27, d28, d29
    STORE_IN_OUTPUT 31, 25, 22, q14, q13
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;temp1 = (step1b[24][i] - step1b[23][i]) * cospi_16_64;
    ;temp2 = (step1b[24][i] + step1b[23][i]) * cospi_16_64;
    ;step1[23] = dct_const_round_shift(temp1);
    ;step1[24] = dct_const_round_shift(temp2);
    ; TODO(cd) do some register allocation change to remove these push/pop
    vpop  {q13} ; [23]
    vpop  {q14} ; [24]
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d26, d27, d28, d29
    STORE_IN_OUTPUT 22, 24, 23, q14, q13
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;step1[20] = step1b[23][i] - step1b[20][i];
    ;step1[27] = step1b[24][i] - step1b[27][i];
    vsub.s16  q14, q5, q0
    vsub.s16  q13, q6, q2
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;temp1 = step1b[20][i] * (-cospi_8_64)  - step1b[27][i] * (-cospi_24_64);
    ;temp2 = step1b[20][i] * (-cospi_24_64) + step1b[27][i] * (-cospi_8_64);
    ;step2[27] = dct_const_round_shift(temp1);
    ;step2[20] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD (-cospi_8_64), (-cospi_24_64), d10, d11, d12, d13
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;step1[21] = step1b[22][i] - step1b[21][i];
    ;step1[26] = step1b[25][i] - step1b[26][i];
    vsub.s16  q14,  q7, q1
    vsub.s16  q13,  q4, q3
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;temp1 = step1b[21][i] * (-cospi_8_64)  - step1b[26][i] * (-cospi_24_64);
    ;temp2 = step1b[21][i] * (-cospi_24_64) + step1b[26][i] * (-cospi_8_64);
    ;step2[26] = dct_const_round_shift(temp1);
    ;step2[21] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD (-cospi_8_64), (-cospi_24_64), d0, d1, d2, d3
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;step3[18] = step1b[18][i] + step1b[21][i];
    ;step3[19] = step1b[19][i] + step1b[20][i];
    ;step3[20] = step1b[19][i] - step1b[20][i];
    ;step3[21] = step1b[18][i] - step1b[21][i];
    LOAD_FROM_OUTPUT 23, 18, 19, q14, q13
    vadd.s16  q8,  q14, q1
    vadd.s16  q9,  q13, q6
    vsub.s16  q13, q13, q6
    vsub.s16  q1,  q14, q1
    STORE_IN_OUTPUT 19, 18, 19, q8, q9
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;step3[27] = step1b[28][i] - step1b[27][i];
    ;step3[28] = step1b[28][i] + step1b[27][i];
    ;step3[29] = step1b[29][i] + step1b[26][i];
    ;step3[26] = step1b[29][i] - step1b[26][i];
    LOAD_FROM_OUTPUT 19, 28, 29, q8, q9
    vsub.s16  q14, q8, q5
    vadd.s16  q10, q8, q5
    vadd.s16  q11, q9, q0
    vsub.s16  q0, q9, q0
    STORE_IN_OUTPUT 29, 28, 29, q10, q11
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;temp1 = (step1b[27][i] - step1b[20][i]) * cospi_16_64;
    ;temp2 = (step1b[27][i] + step1b[20][i]) * cospi_16_64;
    ;step1[20] = dct_const_round_shift(temp1);
    ;step1[27] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d26, d27, d28, d29
    STORE_IN_OUTPUT 29, 20, 27, q13, q14
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;temp1 = (step1b[26][i] - step1b[21][i]) * cospi_16_64;
    ;temp2 = (step1b[26][i] + step1b[21][i]) * cospi_16_64;
    ;step1[21] = dct_const_round_shift(temp1);
    ;step1[26] = dct_const_round_shift(temp2);
    DO_BUTTERFLY d0, d1, d2, d3, cospi_16_64, cospi_16_64, d2, d3, d0, d1
    STORE_IN_OUTPUT 27, 21, 26, q1, q0
    ; --------------------------------------------------------------------------


    ; --------------------------------------------------------------------------
    ; BLOCK C: 8-10,11-15
    ; --------------------------------------------------------------------------
    ; generate 8,9,14,15
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;temp1 = input[2 * 32] * cospi_30_64 - input[30 * 32] * cospi_2_64;
    ;temp2 = input[2 * 32] * cospi_2_64 + input[30 * 32] * cospi_30_64;
    ;step2[8] = dct_const_round_shift(temp1);
    ;step2[15] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 3, 2, 30
    DO_BUTTERFLY_STD cospi_30_64, cospi_2_64, d0, d1, d4, d5
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;temp1 = input[18 * 32] * cospi_14_64 - input[14 * 32] * cospi_18_64;
    ;temp2 = input[18 * 32] * cospi_18_64 + input[14 * 32] * cospi_14_64;
    ;step2[9] = dct_const_round_shift(temp1);
    ;step2[14] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 30, 18, 14
    DO_BUTTERFLY_STD cospi_14_64, cospi_18_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;step3[8] = step1b[8][i] + step1b[9][i];
    ;step3[9] = step1b[8][i] - step1b[9][i];
    ;step3[14] = step1b[15][i] - step1b[14][i];
    ;step3[15] = step1b[15][i] + step1b[14][i];
    vsub.s16  q13, q0, q1
    vadd.s16  q0, q0, q1
    vsub.s16  q14, q2, q3
    vadd.s16  q2, q2, q3
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;temp1 = step1b[14][i] * cospi_24_64 - step1b[9][i] * cospi_8_64;
    ;temp2 = step1b[14][i] * cospi_8_64  + step1b[9][i] * cospi_24_64;
    ;step1[9]  = dct_const_round_shift(temp1);
    ;step1[14] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_24_64, cospi_8_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; generate 10,11,12,13
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;temp1 = input[10 * 32] * cospi_22_64 - input[22 * 32] * cospi_10_64;
    ;temp2 = input[10 * 32] * cospi_10_64 + input[22 * 32] * cospi_22_64;
    ;step2[10] = dct_const_round_shift(temp1);
    ;step2[13] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 14, 10, 22
    DO_BUTTERFLY_STD cospi_22_64, cospi_10_64, d10, d11, d14, d15
    ; --------------------------------------------------------------------------
    ; part of stage 2
    ;temp1 = input[26 * 32] * cospi_6_64 - input[6 * 32] * cospi_26_64;
    ;temp2 = input[26 * 32] * cospi_26_64 + input[6 * 32] * cospi_6_64;
    ;step2[11] = dct_const_round_shift(temp1);
    ;step2[12] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 22, 26, 6
    DO_BUTTERFLY_STD cospi_6_64, cospi_26_64, d8, d9, d12, d13
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;step3[10] = step1b[11][i] - step1b[10][i];
    ;step3[11] = step1b[11][i] + step1b[10][i];
    ;step3[12] = step1b[12][i] + step1b[13][i];
    ;step3[13] = step1b[12][i] - step1b[13][i];
    vsub.s16  q14, q4, q5
    vadd.s16  q5, q4, q5
    vsub.s16  q13, q6, q7
    vadd.s16  q6, q6, q7
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;temp1 = step1b[10][i] * (-cospi_8_64)  - step1b[13][i] * (-cospi_24_64);
    ;temp2 = step1b[10][i] * (-cospi_24_64) + step1b[13][i] * (-cospi_8_64);
    ;step1[13] = dct_const_round_shift(temp1);
    ;step1[10] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD (-cospi_8_64), (-cospi_24_64), d8, d9, d14, d15
    ; --------------------------------------------------------------------------
    ; combine 8-10,11-15
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;step2[8]  = step1b[8][i] + step1b[11][i];
    ;step2[9]  = step1b[9][i] + step1b[10][i];
    ;step2[10] = step1b[9][i] - step1b[10][i];
    vadd.s16  q8,  q0, q5
    vadd.s16  q9,  q1, q7
    vsub.s16  q13, q1, q7
    ;step2[13] = step1b[14][i] - step1b[13][i];
    ;step2[14] = step1b[14][i] + step1b[13][i];
    ;step2[15] = step1b[15][i] + step1b[12][i];
    vsub.s16  q14, q3, q4
    vadd.s16  q10, q3, q4
    vadd.s16  q15, q2, q6
    STORE_IN_OUTPUT 26, 8, 15, q8, q15
    STORE_IN_OUTPUT 15, 9, 14, q9, q10
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;temp1 = (step1b[13][i] - step1b[10][i]) * cospi_16_64;
    ;temp2 = (step1b[13][i] + step1b[10][i]) * cospi_16_64;
    ;step3[10] = dct_const_round_shift(temp1);
    ;step3[13] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d2, d3, d6, d7
    STORE_IN_OUTPUT 14, 13, 10, q3, q1
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;step2[11] = step1b[8][i] - step1b[11][i];
    ;step2[12] = step1b[15][i] - step1b[12][i];
    vsub.s16  q13, q0, q5
    vsub.s16  q14,  q2, q6
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;temp1 = (step1b[12][i] - step1b[11][i]) * cospi_16_64;
    ;temp2 = (step1b[12][i] + step1b[11][i]) * cospi_16_64;
    ;step3[11] = dct_const_round_shift(temp1);
    ;step3[12] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d2, d3, d6, d7
    STORE_IN_OUTPUT 10, 11, 12, q1, q3
    ; --------------------------------------------------------------------------


    ; --------------------------------------------------------------------------
    ; BLOCK D: 0-3,4-7
    ; --------------------------------------------------------------------------
    ; generate 4,5,6,7
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;temp1 = input[4 * 32] * cospi_28_64 - input[28 * 32] * cospi_4_64;
    ;temp2 = input[4 * 32] * cospi_4_64 + input[28 * 32] * cospi_28_64;
    ;step3[4] = dct_const_round_shift(temp1);
    ;step3[7] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 6, 4, 28
    DO_BUTTERFLY_STD cospi_28_64, cospi_4_64, d0, d1, d4, d5
    ; --------------------------------------------------------------------------
    ; part of stage 3
    ;temp1 = input[20 * 32] * cospi_12_64 - input[12 * 32] * cospi_20_64;
    ;temp2 = input[20 * 32] * cospi_20_64 + input[12 * 32] * cospi_12_64;
    ;step3[5] = dct_const_round_shift(temp1);
    ;step3[6] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 28, 20, 12
    DO_BUTTERFLY_STD cospi_12_64, cospi_20_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;step1[4] = step1b[4][i] + step1b[5][i];
    ;step1[5] = step1b[4][i] - step1b[5][i];
    ;step1[6] = step1b[7][i] - step1b[6][i];
    ;step1[7] = step1b[7][i] + step1b[6][i];
    vsub.s16  q13, q0, q1
    vadd.s16  q0, q0, q1
    vsub.s16  q14, q2, q3
    vadd.s16  q2, q2, q3
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;temp1 = (step1b[6][i] - step1b[5][i]) * cospi_16_64;
    ;temp2 = (step1b[5][i] + step1b[6][i]) * cospi_16_64;
    ;step2[5] = dct_const_round_shift(temp1);
    ;step2[6] = dct_const_round_shift(temp2);
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d2, d3, d6, d7
    ; --------------------------------------------------------------------------
    ; generate 0,1,2,3
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;temp1 = (input[0 * 32] - input[16 * 32]) * cospi_16_64;
    ;temp2 = (input[0 * 32] + input[16 * 32]) * cospi_16_64;
    ;step1[1] = dct_const_round_shift(temp1);
    ;step1[0] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 12, 0, 16
    DO_BUTTERFLY_STD cospi_16_64, cospi_16_64, d10, d11, d14, d15
    ; --------------------------------------------------------------------------
    ; part of stage 4
    ;temp1 = input[8 * 32] * cospi_24_64 - input[24 * 32] * cospi_8_64;
    ;temp2 = input[8 * 32] * cospi_8_64 + input[24 * 32] * cospi_24_64;
    ;step1[2] = dct_const_round_shift(temp1);
    ;step1[3] = dct_const_round_shift(temp2);
    LOAD_FROM_TRANSPOSED 16, 8, 24
    DO_BUTTERFLY_STD cospi_24_64, cospi_8_64, d28, d29, d12, d13
    ; --------------------------------------------------------------------------
    ; part of stage 5
    ;step2[0] = step1b[0][i] + step1b[3][i];
    ;step2[1] = step1b[1][i] + step1b[2][i];
    ;step2[2] = step1b[1][i] - step1b[2][i];
    ;step2[3] = step1b[0][i] - step1b[3][i];
    vadd.s16  q4, q7, q6
    vsub.s16  q7, q7, q6
    vsub.s16  q6, q5, q14
    vadd.s16  q5, q5, q14
    ; --------------------------------------------------------------------------
    ; combine 0-3,4-7
    ; --------------------------------------------------------------------------
    ; part of stage 6
    ;step3[0] = step1b[0][i] + step1b[7][i];
    ;step3[1] = step1b[1][i] + step1b[6][i];
    ;step3[2] = step1b[2][i] + step1b[5][i];
    ;step3[3] = step1b[3][i] + step1b[4][i];
    vadd.s16  q8,  q4, q2
    vadd.s16  q9,  q5, q3
    vadd.s16  q10, q6, q1
    vadd.s16  q11, q7, q0
    ;step3[4] = step1b[3][i] - step1b[4][i];
    ;step3[5] = step1b[2][i] - step1b[5][i];
    ;step3[6] = step1b[1][i] - step1b[6][i];
    ;step3[7] = step1b[0][i] - step1b[7][i];
    vsub.s16  q12, q7, q0
    vsub.s16  q13, q6, q1
    vsub.s16  q14, q5, q3
    vsub.s16  q15, q4, q2
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[0] = step1b[0][i] + step1b[15][i];
    ;step1[1] = step1b[1][i] + step1b[14][i];
    ;step1[14] = step1b[1][i] - step1b[14][i];
    ;step1[15] = step1b[0][i] - step1b[15][i];
    LOAD_FROM_OUTPUT 12, 14, 15, q0, q1
    vadd.s16  q2, q8, q1
    vadd.s16  q3, q9, q0
    vsub.s16  q4, q9, q0
    vsub.s16  q5, q8, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[14 * 32] = step1b[14][i] + step1b[17][i];
    ;output[15 * 32] = step1b[15][i] + step1b[16][i];
    ;output[16 * 32] = step1b[15][i] - step1b[16][i];
    ;output[17 * 32] = step1b[14][i] - step1b[17][i];
    LOAD_FROM_OUTPUT 15, 16, 17, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1

    cmp r5, #0
    bgt idct32_bands_end_2nd_pass

idct32_bands_end_1st_pass
    STORE_IN_OUTPUT 17, 16, 17, q6, q7
    STORE_IN_OUTPUT 17, 14, 15, q8, q9
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 0 * 32] = step1b[0][i] + step1b[31][i];
    ;output[ 1 * 32] = step1b[1][i] + step1b[30][i];
    ;output[30 * 32] = step1b[1][i] - step1b[30][i];
    ;output[31 * 32] = step1b[0][i] - step1b[31][i];
    LOAD_FROM_OUTPUT 15, 30, 31, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_IN_OUTPUT 31, 30, 31, q6, q7
    STORE_IN_OUTPUT 31,  0,  1, q4, q5
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[2] = step1b[2][i] + step1b[13][i];
    ;step1[3] = step1b[3][i] + step1b[12][i];
    ;step1[12] = step1b[3][i] - step1b[12][i];
    ;step1[13] = step1b[2][i] - step1b[13][i];
    LOAD_FROM_OUTPUT 1, 12, 13, q0, q1
    vadd.s16  q2, q10, q1
    vadd.s16  q3, q11, q0
    vsub.s16  q4, q11, q0
    vsub.s16  q5, q10, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[12 * 32] = step1b[12][i] + step1b[19][i];
    ;output[13 * 32] = step1b[13][i] + step1b[18][i];
    ;output[18 * 32] = step1b[13][i] - step1b[18][i];
    ;output[19 * 32] = step1b[12][i] - step1b[19][i];
    LOAD_FROM_OUTPUT 13, 18, 19, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1
    STORE_IN_OUTPUT 19, 18, 19, q6, q7
    STORE_IN_OUTPUT 19, 12, 13, q8, q9
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 2 * 32] = step1b[2][i] + step1b[29][i];
    ;output[ 3 * 32] = step1b[3][i] + step1b[28][i];
    ;output[28 * 32] = step1b[3][i] - step1b[28][i];
    ;output[29 * 32] = step1b[2][i] - step1b[29][i];
    LOAD_FROM_OUTPUT 13, 28, 29, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_IN_OUTPUT 29, 28, 29, q6, q7
    STORE_IN_OUTPUT 29,  2,  3, q4, q5
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[4] = step1b[4][i] + step1b[11][i];
    ;step1[5] = step1b[5][i] + step1b[10][i];
    ;step1[10] = step1b[5][i] - step1b[10][i];
    ;step1[11] = step1b[4][i] - step1b[11][i];
    LOAD_FROM_OUTPUT 3, 10, 11, q0, q1
    vadd.s16  q2, q12, q1
    vadd.s16  q3, q13, q0
    vsub.s16  q4, q13, q0
    vsub.s16  q5, q12, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[10 * 32] = step1b[10][i] + step1b[21][i];
    ;output[11 * 32] = step1b[11][i] + step1b[20][i];
    ;output[20 * 32] = step1b[11][i] - step1b[20][i];
    ;output[21 * 32] = step1b[10][i] - step1b[21][i];
    LOAD_FROM_OUTPUT 11, 20, 21, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1
    STORE_IN_OUTPUT 21, 20, 21, q6, q7
    STORE_IN_OUTPUT 21, 10, 11, q8, q9
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 4 * 32] = step1b[4][i] + step1b[27][i];
    ;output[ 5 * 32] = step1b[5][i] + step1b[26][i];
    ;output[26 * 32] = step1b[5][i] - step1b[26][i];
    ;output[27 * 32] = step1b[4][i] - step1b[27][i];
    LOAD_FROM_OUTPUT 11, 26, 27, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_IN_OUTPUT 27, 26, 27, q6, q7
    STORE_IN_OUTPUT 27,  4,  5, q4, q5
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[6] = step1b[6][i] + step1b[9][i];
    ;step1[7] = step1b[7][i] + step1b[8][i];
    ;step1[8] = step1b[7][i] - step1b[8][i];
    ;step1[9] = step1b[6][i] - step1b[9][i];
    LOAD_FROM_OUTPUT 5, 8, 9, q0, q1
    vadd.s16  q2, q14, q1
    vadd.s16  q3, q15, q0
    vsub.s16  q4, q15, q0
    vsub.s16  q5, q14, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 8 * 32] = step1b[8][i] + step1b[23][i];
    ;output[ 9 * 32] = step1b[9][i] + step1b[22][i];
    ;output[22 * 32] = step1b[9][i] - step1b[22][i];
    ;output[23 * 32] = step1b[8][i] - step1b[23][i];
    LOAD_FROM_OUTPUT 9, 22, 23, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1
    STORE_IN_OUTPUT 23, 22, 23, q6, q7
    STORE_IN_OUTPUT 23, 8, 9, q8, q9
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 6 * 32] = step1b[6][i] + step1b[25][i];
    ;output[ 7 * 32] = step1b[7][i] + step1b[24][i];
    ;output[24 * 32] = step1b[7][i] - step1b[24][i];
    ;output[25 * 32] = step1b[6][i] - step1b[25][i];
    LOAD_FROM_OUTPUT 9, 24, 25, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_IN_OUTPUT 25, 24, 25, q6, q7
    STORE_IN_OUTPUT 25,  6,  7, q4, q5

    ; restore r0 by removing the last offset from the last
    ;     operation (LOAD_FROM_TRANSPOSED 16, 8, 24) => 24*8*2
    sub r0, r0, #24*8*2
    ; restore r1 by removing the last offset from the last
    ;     operation (STORE_IN_OUTPUT 24,  6,  7) => 7*32*2
    ; advance by 8 columns => 8*2
    sub r1, r1, #7*32*2 - 8*2
    ;   advance by 8 lines (8*32*2)
    ;   go back by the two pairs from the loop (32*2)
    add r3, r3, #8*32*2 - 32*2

    ; bands loop processing
    subs r4, r4, #1
    bne idct32_bands_loop

    ; parameters for second pass
    ; the input of pass2 is the result of pass1. we have to remove the offset
    ;   of 32 columns induced by the above idct32_bands_loop
    sub r3, r1, #32*2
      ; r1 = pass2[32 * 32]
    add r1, sp, #2048

    ; pass loop processing
    add r5, r5, #1
    b idct32_pass_loop

idct32_bands_end_2nd_pass
    STORE_COMBINE_CENTER_RESULTS
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 0 * 32] = step1b[0][i] + step1b[31][i];
    ;output[ 1 * 32] = step1b[1][i] + step1b[30][i];
    ;output[30 * 32] = step1b[1][i] - step1b[30][i];
    ;output[31 * 32] = step1b[0][i] - step1b[31][i];
    LOAD_FROM_OUTPUT 17, 30, 31, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_COMBINE_EXTREME_RESULTS
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[2] = step1b[2][i] + step1b[13][i];
    ;step1[3] = step1b[3][i] + step1b[12][i];
    ;step1[12] = step1b[3][i] - step1b[12][i];
    ;step1[13] = step1b[2][i] - step1b[13][i];
    LOAD_FROM_OUTPUT 31, 12, 13, q0, q1
    vadd.s16  q2, q10, q1
    vadd.s16  q3, q11, q0
    vsub.s16  q4, q11, q0
    vsub.s16  q5, q10, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[12 * 32] = step1b[12][i] + step1b[19][i];
    ;output[13 * 32] = step1b[13][i] + step1b[18][i];
    ;output[18 * 32] = step1b[13][i] - step1b[18][i];
    ;output[19 * 32] = step1b[12][i] - step1b[19][i];
    LOAD_FROM_OUTPUT 13, 18, 19, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1
    STORE_COMBINE_CENTER_RESULTS
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 2 * 32] = step1b[2][i] + step1b[29][i];
    ;output[ 3 * 32] = step1b[3][i] + step1b[28][i];
    ;output[28 * 32] = step1b[3][i] - step1b[28][i];
    ;output[29 * 32] = step1b[2][i] - step1b[29][i];
    LOAD_FROM_OUTPUT 19, 28, 29, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_COMBINE_EXTREME_RESULTS
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[4] = step1b[4][i] + step1b[11][i];
    ;step1[5] = step1b[5][i] + step1b[10][i];
    ;step1[10] = step1b[5][i] - step1b[10][i];
    ;step1[11] = step1b[4][i] - step1b[11][i];
    LOAD_FROM_OUTPUT 29, 10, 11, q0, q1
    vadd.s16  q2, q12, q1
    vadd.s16  q3, q13, q0
    vsub.s16  q4, q13, q0
    vsub.s16  q5, q12, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[10 * 32] = step1b[10][i] + step1b[21][i];
    ;output[11 * 32] = step1b[11][i] + step1b[20][i];
    ;output[20 * 32] = step1b[11][i] - step1b[20][i];
    ;output[21 * 32] = step1b[10][i] - step1b[21][i];
    LOAD_FROM_OUTPUT 11, 20, 21, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1
    STORE_COMBINE_CENTER_RESULTS
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 4 * 32] = step1b[4][i] + step1b[27][i];
    ;output[ 5 * 32] = step1b[5][i] + step1b[26][i];
    ;output[26 * 32] = step1b[5][i] - step1b[26][i];
    ;output[27 * 32] = step1b[4][i] - step1b[27][i];
    LOAD_FROM_OUTPUT 21, 26, 27, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_COMBINE_EXTREME_RESULTS
    ; --------------------------------------------------------------------------
    ; part of stage 7
    ;step1[6] = step1b[6][i] + step1b[9][i];
    ;step1[7] = step1b[7][i] + step1b[8][i];
    ;step1[8] = step1b[7][i] - step1b[8][i];
    ;step1[9] = step1b[6][i] - step1b[9][i];
    LOAD_FROM_OUTPUT 27, 8, 9, q0, q1
    vadd.s16  q2, q14, q1
    vadd.s16  q3, q15, q0
    vsub.s16  q4, q15, q0
    vsub.s16  q5, q14, q1
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 8 * 32] = step1b[8][i] + step1b[23][i];
    ;output[ 9 * 32] = step1b[9][i] + step1b[22][i];
    ;output[22 * 32] = step1b[9][i] - step1b[22][i];
    ;output[23 * 32] = step1b[8][i] - step1b[23][i];
    LOAD_FROM_OUTPUT 9, 22, 23, q0, q1
    vadd.s16  q8, q4, q1
    vadd.s16  q9, q5, q0
    vsub.s16  q6, q5, q0
    vsub.s16  q7, q4, q1
    STORE_COMBINE_CENTER_RESULTS_LAST
    ; --------------------------------------------------------------------------
    ; part of final stage
    ;output[ 6 * 32] = step1b[6][i] + step1b[25][i];
    ;output[ 7 * 32] = step1b[7][i] + step1b[24][i];
    ;output[24 * 32] = step1b[7][i] - step1b[24][i];
    ;output[25 * 32] = step1b[6][i] - step1b[25][i];
    LOAD_FROM_OUTPUT 23, 24, 25, q0, q1
    vadd.s16  q4, q2, q1
    vadd.s16  q5, q3, q0
    vsub.s16  q6, q3, q0
    vsub.s16  q7, q2, q1
    STORE_COMBINE_EXTREME_RESULTS_LAST
    ; --------------------------------------------------------------------------
    ; restore pointers to their initial indices for next band pass by
    ;     removing/adding dest_stride * 8. The actual increment by eight
    ;     is taken care of within the _LAST macros.
    add r6,  r6,  r2, lsl #3
    add r9,  r9,  r2, lsl #3
    sub r7,  r7,  r2, lsl #3
    sub r10, r10, r2, lsl #3

    ; restore r0 by removing the last offset from the last
    ;     operation (LOAD_FROM_TRANSPOSED 16, 8, 24) => 24*8*2
    sub r0, r0, #24*8*2
    ; restore r1 by removing the last offset from the last
    ;     operation (LOAD_FROM_OUTPUT 23, 24, 25) => 25*32*2
    ; advance by 8 columns => 8*2
    sub r1, r1, #25*32*2 - 8*2
    ;   advance by 8 lines (8*32*2)
    ;   go back by the two pairs from the loop (32*2)
    add r3, r3, #8*32*2 - 32*2

    ; bands loop processing
    subs r4, r4, #1
    bne idct32_bands_loop

    ; stack operation
    add sp, sp, #512+2048+2048
    vpop {d8-d15}
    pop  {r4-r11}
    bx              lr
    ENDP  ; |vpx_idct32x32_1024_add_neon|
    END
