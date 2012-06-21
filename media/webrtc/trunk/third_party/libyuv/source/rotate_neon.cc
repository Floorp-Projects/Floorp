/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/basic_types.h"
#include "row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)

void ReverseRow_NEON(const uint8* src, uint8* dst, int width) {
  asm volatile (
    // compute where to start writing destination
    "add         %1, %2                        \n"

    // work on segments that are multiples of 16
    "lsrs        r3, %2, #4                    \n"

    // the output is written in two block.  8 bytes followed
    // by another 8.  reading is done sequentially, from left to
    // right.  writing is done from right to left in block sizes
    // %1, the destination pointer is incremented after writing
    // the first of the two blocks.  need to subtract that 8 off
    // along with 16 to get the next location.
    "mov         r3, #-24                      \n"

    "beq         2f                            \n"

    // back of destination by the size of the register that is
    // going to be reversed
    "sub         %1, #16                       \n"

    // the loop needs to run on blocks of 16.  what will be left
    // over is either a negative number, the residuals that need
    // to be done, or 0.  if this isn't subtracted off here the
    // loop will run one extra time.
    "sub         %2, #16                       \n"

    "1:                                        \n"
      "vld1.8      {q0}, [%0]!                 \n"  // src += 16

        // reverse the bytes in the 64 bit segments.  unable to reverse
        // the bytes in the entire 128 bits in one go.
      "vrev64.8    q0, q0                      \n"

        // because of the inability to reverse the entire 128 bits
        // reverse the writing out of the two 64 bit segments.
      "vst1.8      {d1}, [%1]!                 \n"
      "vst1.8      {d0}, [%1], r3              \n"  // dst -= 16

      "subs        %2, #16                     \n"
    "bge         1b                            \n"

    // add 16 back to the counter.  if the result is 0 there is no
    // residuals so jump past
    "adds        %2, #16                       \n"
    "beq         5f                            \n"

    "add         %1, #16                       \n"

    "2:                                        \n"

    "mov         r3, #-3                       \n"

    "sub         %1, #2                        \n"
    "subs        %2, #2                        \n"
    // check for 16*n+1 scenarios where segments_of_2 should not
    // be run, but there is something left over.
    "blt         4f                            \n"

// do this in neon registers as per
// http://blogs.arm.com/software-enablement/196-coding-for-neon-part-2-dealing-with-leftovers/
    "3:                                        \n"
    "vld2.8      {d0[0], d1[0]}, [%0]!         \n"  // src += 2

    "vst1.8      {d1[0]}, [%1]!                \n"
    "vst1.8      {d0[0]}, [%1], r3             \n"  // dst -= 2

    "subs        %2, #2                        \n"
    "bge         3b                            \n"

    "adds        %2, #2                        \n"
    "beq         5f                            \n"

    "4:                                        \n"
    "add         %1, #1                        \n"
    "vld1.8      {d0[0]}, [%0]                 \n"
    "vst1.8      {d0[0]}, [%1]                 \n"

    "5:                                        \n"
    : "+r"(src),              // %0
      "+r"(dst),              // %1
      "+r"(width)             // %2
    :
    : "memory", "cc", "r3", "q0"
  );
}

static const uint8 vtbl_4x4_transpose[16] __attribute__((vector_size(16))) =
  { 0,  4,  8, 12,  1,  5,  9, 13,  2,  6, 10, 14,  3,  7, 11, 15 };

void TransposeWx8_NEON(const uint8* src, int src_stride,
                       uint8* dst, int dst_stride,
                       int width) {
  asm volatile (
    // loops are on blocks of 8.  loop will stop when
    // counter gets to or below 0.  starting the counter
    // at w-8 allow for this
    "sub         %4, #8                        \n"

    // handle 8x8 blocks.  this should be the majority of the plane
    "1:                                        \n"
      "mov         r9, %0                      \n"

      "vld1.8      {d0}, [r9], %1              \n"
      "vld1.8      {d1}, [r9], %1              \n"
      "vld1.8      {d2}, [r9], %1              \n"
      "vld1.8      {d3}, [r9], %1              \n"
      "vld1.8      {d4}, [r9], %1              \n"
      "vld1.8      {d5}, [r9], %1              \n"
      "vld1.8      {d6}, [r9], %1              \n"
      "vld1.8      {d7}, [r9]                  \n"

      "vtrn.8      d1, d0                      \n"
      "vtrn.8      d3, d2                      \n"
      "vtrn.8      d5, d4                      \n"
      "vtrn.8      d7, d6                      \n"

      "vtrn.16     d1, d3                      \n"
      "vtrn.16     d0, d2                      \n"
      "vtrn.16     d5, d7                      \n"
      "vtrn.16     d4, d6                      \n"

      "vtrn.32     d1, d5                      \n"
      "vtrn.32     d0, d4                      \n"
      "vtrn.32     d3, d7                      \n"
      "vtrn.32     d2, d6                      \n"

      "vrev16.8    q0, q0                      \n"
      "vrev16.8    q1, q1                      \n"
      "vrev16.8    q2, q2                      \n"
      "vrev16.8    q3, q3                      \n"

      "mov         r9, %2                      \n"

      "vst1.8      {d1}, [r9], %3              \n"
      "vst1.8      {d0}, [r9], %3              \n"
      "vst1.8      {d3}, [r9], %3              \n"
      "vst1.8      {d2}, [r9], %3              \n"
      "vst1.8      {d5}, [r9], %3              \n"
      "vst1.8      {d4}, [r9], %3              \n"
      "vst1.8      {d7}, [r9], %3              \n"
      "vst1.8      {d6}, [r9]                  \n"

      "add         %0, #8                      \n"  // src += 8
      "add         %2, %2, %3, lsl #3          \n"  // dst += 8 * dst_stride
      "subs        %4,  #8                     \n"  // w   -= 8
      "bge         1b                          \n"

    // add 8 back to counter.  if the result is 0 there are
    // no residuals.
    "adds        %4, #8                        \n"
    "beq         4f                            \n"

    // some residual, so between 1 and 7 lines left to transpose
    "cmp         %4, #2                        \n"
    "blt         3f                            \n"

    "cmp         %4, #4                        \n"
    "blt         2f                            \n"

    // 4x8 block
    "mov         r9, %0                        \n"
    "vld1.32     {d0[0]}, [r9], %1             \n"
    "vld1.32     {d0[1]}, [r9], %1             \n"
    "vld1.32     {d1[0]}, [r9], %1             \n"
    "vld1.32     {d1[1]}, [r9], %1             \n"
    "vld1.32     {d2[0]}, [r9], %1             \n"
    "vld1.32     {d2[1]}, [r9], %1             \n"
    "vld1.32     {d3[0]}, [r9], %1             \n"
    "vld1.32     {d3[1]}, [r9]                 \n"

    "mov         r9, %2                        \n"

    "vld1.8      {q3}, [%5]                    \n"

    "vtbl.8      d4, {d0, d1}, d6              \n"
    "vtbl.8      d5, {d0, d1}, d7              \n"
    "vtbl.8      d0, {d2, d3}, d6              \n"
    "vtbl.8      d1, {d2, d3}, d7              \n"

    // TODO: rework shuffle above to write
    //       out with 4 instead of 8 writes
    "vst1.32     {d4[0]}, [r9], %3             \n"
    "vst1.32     {d4[1]}, [r9], %3             \n"
    "vst1.32     {d5[0]}, [r9], %3             \n"
    "vst1.32     {d5[1]}, [r9]                 \n"

    "add         r9, %2, #4                    \n"
    "vst1.32     {d0[0]}, [r9], %3             \n"
    "vst1.32     {d0[1]}, [r9], %3             \n"
    "vst1.32     {d1[0]}, [r9], %3             \n"
    "vst1.32     {d1[1]}, [r9]                 \n"

    "add         %0, #4                        \n"  // src += 4
    "add         %2, %2, %3, lsl #2            \n"  // dst += 4 * dst_stride
    "subs        %4,  #4                       \n"  // w   -= 4
    "beq         4f                            \n"

    // some residual, check to see if it includes a 2x8 block,
    // or less
    "cmp         %4, #2                        \n"
    "blt         3f                            \n"

    // 2x8 block
    "2:                                        \n"
    "mov         r9, %0                        \n"
    "vld1.16     {d0[0]}, [r9], %1             \n"
    "vld1.16     {d1[0]}, [r9], %1             \n"
    "vld1.16     {d0[1]}, [r9], %1             \n"
    "vld1.16     {d1[1]}, [r9], %1             \n"
    "vld1.16     {d0[2]}, [r9], %1             \n"
    "vld1.16     {d1[2]}, [r9], %1             \n"
    "vld1.16     {d0[3]}, [r9], %1             \n"
    "vld1.16     {d1[3]}, [r9]                 \n"

    "vtrn.8      d0, d1                        \n"

    "mov         r9, %2                        \n"

    "vst1.64     {d0}, [r9], %3                \n"
    "vst1.64     {d1}, [r9]                    \n"

    "add         %0, #2                        \n"  // src += 2
    "add         %2, %2, %3, lsl #1            \n"  // dst += 2 * dst_stride
    "subs        %4,  #2                       \n"  // w   -= 2
    "beq         4f                            \n"

    // 1x8 block
    "3:                                        \n"
    "vld1.8      {d0[0]}, [%0], %1             \n"
    "vld1.8      {d0[1]}, [%0], %1             \n"
    "vld1.8      {d0[2]}, [%0], %1             \n"
    "vld1.8      {d0[3]}, [%0], %1             \n"
    "vld1.8      {d0[4]}, [%0], %1             \n"
    "vld1.8      {d0[5]}, [%0], %1             \n"
    "vld1.8      {d0[6]}, [%0], %1             \n"
    "vld1.8      {d0[7]}, [%0]                 \n"

    "vst1.64     {d0}, [%2]                    \n"

    "4:                                        \n"

    : "+r"(src),              // %0
      "+r"(src_stride),       // %1
      "+r"(dst),              // %2
      "+r"(dst_stride),       // %3
      "+r"(width)             // %4
    : "r"(vtbl_4x4_transpose) // %5
    : "memory", "cc", "r9", "q0", "q1", "q2", "q3"
  );
}

void ReverseRowUV_NEON(const uint8* src,
                       uint8* dst_a, uint8* dst_b,
                       int width) {
  asm volatile (
    // compute where to start writing destination
    "add         %1, %3                        \n"  // dst_a + width
    "add         %2, %3                        \n"  // dst_b + width

    // work on input segments that are multiples of 16, but
    // width that has been passed is output segments, half
    // the size of input.
    "lsrs        r12, %3, #3                   \n"

    "beq         2f                            \n"

    // the output is written in to two blocks.
    "mov         r12, #-8                      \n"

    // back of destination by the size of the register that is
    // going to be reversed
    "sub         %1, #8                        \n"
    "sub         %2, #8                        \n"

    // the loop needs to run on blocks of 8.  what will be left
    // over is either a negative number, the residuals that need
    // to be done, or 0.  if this isn't subtracted off here the
    // loop will run one extra time.
    "sub         %3, #8                        \n"

    "1:                                        \n"
      "vld2.8      {d0, d1}, [%0]!             \n"  // src += 16

      // reverse the bytes in the 64 bit segments
      "vrev64.8    q0, q0                      \n"

      "vst1.8      {d0}, [%1], r12             \n"  // dst_a -= 8
      "vst1.8      {d1}, [%2], r12             \n"  // dst_b -= 8

      "subs        %3, #8                      \n"
      "bge         1b                          \n"

    // add 8 back to the counter.  if the result is 0 there is no
    // residuals so return
    "adds        %3, #8                        \n"
    "beq         4f                            \n"

    "add         %1, #8                        \n"
    "add         %2, #8                        \n"

    "2:                                        \n"

    "mov         r12, #-1                      \n"

    "sub         %1, #1                        \n"
    "sub         %2, #1                        \n"

    "3:                                        \n"
      "vld2.8      {d0[0], d1[0]}, [%0]!       \n"  // src += 2

      "vst1.8      {d0[0]}, [%1], r12          \n"  // dst_a -= 1
      "vst1.8      {d1[0]}, [%2], r12          \n"  // dst_b -= 1

      "subs        %3, %3, #1                  \n"
      "bgt         3b                          \n"
    "4:                                        \n"
    : "+r"(src),              // %0
      "+r"(dst_a),            // %1
      "+r"(dst_b),            // %2
      "+r"(width)             // %3
    :
    : "memory", "cc", "r12", "q0"
  );
}

static const uint8 vtbl_4x4_transpose_di[16] __attribute__((vector_size(16))) =
  { 0,  8,  1,  9,  2, 10,  3, 11,  4, 12,  5, 13,  6, 14,  7, 15 };

void TransposeUVWx8_NEON(const uint8* src, int src_stride,
                         uint8* dst_a, int dst_stride_a,
                         uint8* dst_b, int dst_stride_b,
                         int width) {
  asm volatile (
    // loops are on blocks of 8.  loop will stop when
    // counter gets to or below 0.  starting the counter
    // at w-8 allow for this
    "sub         %6, #8                        \n"

    // handle 8x8 blocks.  this should be the majority of the plane
    "1:                                        \n"
      "mov         r9, %0                      \n"

      "vld2.8      {d0,  d1},  [r9], %1        \n"
      "vld2.8      {d2,  d3},  [r9], %1        \n"
      "vld2.8      {d4,  d5},  [r9], %1        \n"
      "vld2.8      {d6,  d7},  [r9], %1        \n"
      "vld2.8      {d16, d17}, [r9], %1        \n"
      "vld2.8      {d18, d19}, [r9], %1        \n"
      "vld2.8      {d20, d21}, [r9], %1        \n"
      "vld2.8      {d22, d23}, [r9]            \n"

      "vtrn.8      q1, q0                      \n"
      "vtrn.8      q3, q2                      \n"
      "vtrn.8      q9, q8                      \n"
      "vtrn.8      q11, q10                    \n"

      "vtrn.16     q1, q3                      \n"
      "vtrn.16     q0, q2                      \n"
      "vtrn.16     q9, q11                     \n"
      "vtrn.16     q8, q10                     \n"

      "vtrn.32     q1, q9                      \n"
      "vtrn.32     q0, q8                      \n"
      "vtrn.32     q3, q11                     \n"
      "vtrn.32     q2, q10                     \n"

      "vrev16.8    q0, q0                      \n"
      "vrev16.8    q1, q1                      \n"
      "vrev16.8    q2, q2                      \n"
      "vrev16.8    q3, q3                      \n"
      "vrev16.8    q8, q8                      \n"
      "vrev16.8    q9, q9                      \n"
      "vrev16.8    q10, q10                    \n"
      "vrev16.8    q11, q11                    \n"

      "mov         r9, %2                      \n"

      "vst1.8      {d2},  [r9], %3             \n"
      "vst1.8      {d0},  [r9], %3             \n"
      "vst1.8      {d6},  [r9], %3             \n"
      "vst1.8      {d4},  [r9], %3             \n"
      "vst1.8      {d18}, [r9], %3             \n"
      "vst1.8      {d16}, [r9], %3             \n"
      "vst1.8      {d22}, [r9], %3             \n"
      "vst1.8      {d20}, [r9]                 \n"

      "mov         r9, %4                      \n"

      "vst1.8      {d3},  [r9], %5             \n"
      "vst1.8      {d1},  [r9], %5             \n"
      "vst1.8      {d7},  [r9], %5             \n"
      "vst1.8      {d5},  [r9], %5             \n"
      "vst1.8      {d19}, [r9], %5             \n"
      "vst1.8      {d17}, [r9], %5             \n"
      "vst1.8      {d23}, [r9], %5             \n"
      "vst1.8      {d21}, [r9]                 \n"

      "add         %0, #8*2                    \n"  // src   += 8*2
      "add         %2, %2, %3, lsl #3          \n"  // dst_a += 8 * dst_stride_a
      "add         %4, %4, %5, lsl #3          \n"  // dst_b += 8 * dst_stride_b
      "subs        %6,  #8                     \n"  // w     -= 8
      "bge         1b                          \n"

    // add 8 back to counter.  if the result is 0 there are
    // no residuals.
    "adds        %6, #8                        \n"
    "beq         4f                            \n"

    // some residual, so between 1 and 7 lines left to transpose
    "cmp         %6, #2                        \n"
    "blt         3f                            \n"

    "cmp         %6, #4                        \n"
    "blt         2f                            \n"

    //TODO(frkoenig) : clean this up
    // 4x8 block
    "mov         r9, %0                        \n"
    "vld1.64     {d0}, [r9], %1                \n"
    "vld1.64     {d1}, [r9], %1                \n"
    "vld1.64     {d2}, [r9], %1                \n"
    "vld1.64     {d3}, [r9], %1                \n"
    "vld1.64     {d4}, [r9], %1                \n"
    "vld1.64     {d5}, [r9], %1                \n"
    "vld1.64     {d6}, [r9], %1                \n"
    "vld1.64     {d7}, [r9]                    \n"

    "vld1.8      {q15}, [%7]                   \n"

    "vtrn.8      q0, q1                        \n"
    "vtrn.8      q2, q3                        \n"

    "vtbl.8      d16, {d0, d1}, d30            \n"
    "vtbl.8      d17, {d0, d1}, d31            \n"
    "vtbl.8      d18, {d2, d3}, d30            \n"
    "vtbl.8      d19, {d2, d3}, d31            \n"
    "vtbl.8      d20, {d4, d5}, d30            \n"
    "vtbl.8      d21, {d4, d5}, d31            \n"
    "vtbl.8      d22, {d6, d7}, d30            \n"
    "vtbl.8      d23, {d6, d7}, d31            \n"

    "mov         r9, %2                        \n"

    "vst1.32     {d16[0]},  [r9], %3           \n"
    "vst1.32     {d16[1]},  [r9], %3           \n"
    "vst1.32     {d17[0]},  [r9], %3           \n"
    "vst1.32     {d17[1]},  [r9], %3           \n"

    "add         r9, %2, #4                    \n"
    "vst1.32     {d20[0]}, [r9], %3            \n"
    "vst1.32     {d20[1]}, [r9], %3            \n"
    "vst1.32     {d21[0]}, [r9], %3            \n"
    "vst1.32     {d21[1]}, [r9]                \n"

    "mov         r9, %4                        \n"

    "vst1.32     {d18[0]}, [r9], %5            \n"
    "vst1.32     {d18[1]}, [r9], %5            \n"
    "vst1.32     {d19[0]}, [r9], %5            \n"
    "vst1.32     {d19[1]}, [r9], %5            \n"

    "add         r9, %4, #4                    \n"
    "vst1.32     {d22[0]},  [r9], %5           \n"
    "vst1.32     {d22[1]},  [r9], %5           \n"
    "vst1.32     {d23[0]},  [r9], %5           \n"
    "vst1.32     {d23[1]},  [r9]               \n"

    "add         %0, #4*2                      \n"  // src   += 4 * 2
    "add         %2, %2, %3, lsl #2            \n"  // dst_a += 4 * dst_stride_a
    "add         %4, %4, %5, lsl #2            \n"  // dst_b += 4 * dst_stride_b
    "subs        %6,  #4                       \n"  // w     -= 4
    "beq         4f                            \n"

    // some residual, check to see if it includes a 2x8 block,
    // or less
    "cmp         %6, #2                        \n"
    "blt         3f                            \n"

    // 2x8 block
    "2:                                        \n"
    "mov         r9, %0                        \n"
    "vld2.16     {d0[0], d2[0]}, [r9], %1      \n"
    "vld2.16     {d1[0], d3[0]}, [r9], %1      \n"
    "vld2.16     {d0[1], d2[1]}, [r9], %1      \n"
    "vld2.16     {d1[1], d3[1]}, [r9], %1      \n"
    "vld2.16     {d0[2], d2[2]}, [r9], %1      \n"
    "vld2.16     {d1[2], d3[2]}, [r9], %1      \n"
    "vld2.16     {d0[3], d2[3]}, [r9], %1      \n"
    "vld2.16     {d1[3], d3[3]}, [r9]          \n"

    "vtrn.8      d0, d1                        \n"
    "vtrn.8      d2, d3                        \n"

    "mov         r9, %2                        \n"

    "vst1.64     {d0}, [r9], %3                \n"
    "vst1.64     {d2}, [r9]                    \n"

    "mov         r9, %4                        \n"

    "vst1.64     {d1}, [r9], %5                \n"
    "vst1.64     {d3}, [r9]                    \n"

    "add         %0, #2*2                      \n"  // src   += 2 * 2
    "add         %2, %2, %3, lsl #1            \n"  // dst_a += 2 * dst_stride_a
    "add         %4, %4, %5, lsl #1            \n"  // dst_b += 2 * dst_stride_b
    "subs        %6,  #2                       \n"  // w     -= 2
    "beq         4f                            \n"

    // 1x8 block
    "3:                                        \n"
    "vld2.8      {d0[0], d1[0]}, [%0], %1      \n"
    "vld2.8      {d0[1], d1[1]}, [%0], %1      \n"
    "vld2.8      {d0[2], d1[2]}, [%0], %1      \n"
    "vld2.8      {d0[3], d1[3]}, [%0], %1      \n"
    "vld2.8      {d0[4], d1[4]}, [%0], %1      \n"
    "vld2.8      {d0[5], d1[5]}, [%0], %1      \n"
    "vld2.8      {d0[6], d1[6]}, [%0], %1      \n"
    "vld2.8      {d0[7], d1[7]}, [%0]          \n"

    "vst1.64     {d0}, [%2]                    \n"
    "vst1.64     {d1}, [%4]                    \n"

    "4:                                        \n"

    : "+r"(src),              // %0
      "+r"(src_stride),       // %1
      "+r"(dst_a),            // %2
      "+r"(dst_stride_a),     // %3
      "+r"(dst_b),            // %4
      "+r"(dst_stride_b),     // %5
      "+r"(width)             // %6
    : "r"(vtbl_4x4_transpose_di)// %7
    : "memory", "cc", "r9",
      "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11"
  );
}
#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
