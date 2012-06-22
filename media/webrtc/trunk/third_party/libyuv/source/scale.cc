/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "libyuv/scale.h"

#include <assert.h>
#include <string.h>

#include "libyuv/cpu_id.h"
#include "libyuv/planar_functions.h"  // For CopyPlane
#include "row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#if defined(_MSC_VER)
#define ALIGN16(var) __declspec(align(16)) var
#else
#define ALIGN16(var) var __attribute__((aligned(16)))
#endif

// Note: A Neon reference manual
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0204j/CJAJIIGG.html
// Note: Some SSE2 reference manuals
// cpuvol1.pdf agner_instruction_tables.pdf 253666.pdf 253667.pdf

// Set the following flag to true to revert to only
// using the reference implementation ScalePlaneBox(), and
// NOT the optimized versions. Useful for debugging and
// when comparing the quality of the resulting YUV planes
// as produced by the optimized and non-optimized versions.

static bool use_reference_impl_ = false;

void SetUseReferenceImpl(bool use) {
  use_reference_impl_ = use;
}

// ScaleRowDown2Int also used by planar functions

/**
 * NEON downscalers with interpolation.
 *
 * Provided by Fritz Koenig
 *
 */

#if defined(__ARM_NEON__) && !defined(YUV_DISABLE_ASM)
#define HAS_SCALEROWDOWN2_NEON
void ScaleRowDown2_NEON(const uint8* src_ptr, int /* src_stride */,
                        uint8* dst, int dst_width) {
  asm volatile (
    "1:                                        \n"
    "vld2.u8    {q0,q1}, [%0]!                 \n"  // load even pixels into q0, odd into q1
    "vst1.u8    {q0}, [%1]!                    \n"  // store even pixels
    "subs       %2, %2, #16                    \n"  // 16 processed per loop
    "bhi        1b                             \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst),              // %1
      "+r"(dst_width)         // %2
    :
    : "q0", "q1"              // Clobber List
  );
}

void ScaleRowDown2Int_NEON(const uint8* src_ptr, int src_stride,
                           uint8* dst, int dst_width) {
  asm volatile (
    "add        %1, %0                         \n"  // change the stride to row 2 pointer
    "1:                                        \n"
    "vld1.u8    {q0,q1}, [%0]!                 \n"  // load row 1 and post increment
    "vld1.u8    {q2,q3}, [%1]!                 \n"  // load row 2 and post increment
    "vpaddl.u8  q0, q0                         \n"  // row 1 add adjacent
    "vpaddl.u8  q1, q1                         \n"
    "vpadal.u8  q0, q2                         \n"  // row 2 add adjacent, add row 1 to row 2
    "vpadal.u8  q1, q3                         \n"
    "vrshrn.u16 d0, q0, #2                     \n"  // downshift, round and pack
    "vrshrn.u16 d1, q1, #2                     \n"
    "vst1.u8    {q0}, [%2]!                    \n"
    "subs       %3, %3, #16                    \n"  // 16 processed per loop
    "bhi        1b                             \n"
    : "+r"(src_ptr),          // %0
      "+r"(src_stride),       // %1
      "+r"(dst),              // %2
      "+r"(dst_width)         // %3
    :
    : "q0", "q1", "q2", "q3"     // Clobber List
   );
}

#define HAS_SCALEROWDOWN4_NEON
static void ScaleRowDown4_NEON(const uint8* src_ptr, int /* src_stride */,
                               uint8* dst_ptr, int dst_width) {
  asm volatile (
    "1:                                        \n"
    "vld2.u8    {d0, d1}, [%0]!                \n"
    "vtrn.u8    d1, d0                         \n"
    "vshrn.u16  d0, q0, #8                     \n"
    "vst1.u32   {d0[1]}, [%1]!                 \n"

    "subs       %2, #4                         \n"
    "bhi        1b                             \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    :
    : "q0", "q1", "memory", "cc"
  );
}

static void ScaleRowDown4Int_NEON(const uint8* src_ptr, int src_stride,
                                  uint8* dst_ptr, int dst_width) {
  asm volatile (
    "add        r4, %0, %3                     \n"
    "add        r5, r4, %3                     \n"
    "add        %3, r5, %3                     \n"
    "1:                                        \n"
    "vld1.u8    {q0}, [%0]!                    \n"   // load up 16x4 block of input data
    "vld1.u8    {q1}, [r4]!                    \n"
    "vld1.u8    {q2}, [r5]!                    \n"
    "vld1.u8    {q3}, [%3]!                    \n"

    "vpaddl.u8  q0, q0                         \n"
    "vpadal.u8  q0, q1                         \n"
    "vpadal.u8  q0, q2                         \n"
    "vpadal.u8  q0, q3                         \n"

    "vpaddl.u16 q0, q0                         \n"

    "vrshrn.u32 d0, q0, #4                     \n"   // divide by 16 w/rounding

    "vmovn.u16  d0, q0                         \n"
    "vst1.u32   {d0[0]}, [%1]!                 \n"

    "subs       %2, #4                         \n"
    "bhi        1b                             \n"

    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    : "r"(src_stride)         // %3
    : "r4", "r5", "q0", "q1", "q2", "q3", "memory", "cc"
  );
}

#define HAS_SCALEROWDOWN34_NEON
// Down scale from 4 to 3 pixels.  Use the neon multilane read/write
//  to load up the every 4th pixel into a 4 different registers.
// Point samples 32 pixels to 24 pixels.
static void ScaleRowDown34_NEON(const uint8* src_ptr, int /* src_stride */,
                                uint8* dst_ptr, int dst_width) {
  asm volatile (
    "1:                                        \n"
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n" // src line 0
    "vmov         d2, d3                       \n" // order needs to be d0, d1, d2
    "vst3.u8      {d0, d1, d2}, [%1]!          \n"
    "subs         %2, #24                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    :
    : "d0", "d1", "d2", "d3", "memory", "cc"
  );
}

static void ScaleRowDown34_0_Int_NEON(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width) {
  asm volatile (
    "vmov.u8      d24, #3                      \n"
    "add          %3, %0                       \n"
    "1:                                        \n"
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n" // src line 0
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n" // src line 1

    // filter src line 0 with src line 1
    // expand chars to shorts to allow for room
    // when adding lines together
    "vmovl.u8     q8, d4                       \n"
    "vmovl.u8     q9, d5                       \n"
    "vmovl.u8     q10, d6                      \n"
    "vmovl.u8     q11, d7                      \n"

    // 3 * line_0 + line_1
    "vmlal.u8     q8, d0, d24                  \n"
    "vmlal.u8     q9, d1, d24                  \n"
    "vmlal.u8     q10, d2, d24                 \n"
    "vmlal.u8     q11, d3, d24                 \n"

    // (3 * line_0 + line_1) >> 2
    "vqrshrn.u16  d0, q8, #2                   \n"
    "vqrshrn.u16  d1, q9, #2                   \n"
    "vqrshrn.u16  d2, q10, #2                  \n"
    "vqrshrn.u16  d3, q11, #2                  \n"

    // a0 = (src[0] * 3 + s[1] * 1) >> 2
    "vmovl.u8     q8, d1                       \n"
    "vmlal.u8     q8, d0, d24                  \n"
    "vqrshrn.u16  d0, q8, #2                   \n"

    // a1 = (src[1] * 1 + s[2] * 1) >> 1
    "vrhadd.u8    d1, d1, d2                   \n"

    // a2 = (src[2] * 1 + s[3] * 3) >> 2
    "vmovl.u8     q8, d2                       \n"
    "vmlal.u8     q8, d3, d24                  \n"
    "vqrshrn.u16  d2, q8, #2                   \n"

    "vst3.u8      {d0, d1, d2}, [%1]!          \n"

    "subs         %2, #24                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    :
    : "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "d24", "memory", "cc"
  );
}

static void ScaleRowDown34_1_Int_NEON(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width) {
  asm volatile (
    "vmov.u8      d24, #3                      \n"
    "add          %3, %0                       \n"
    "1:                                        \n"
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n" // src line 0
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n" // src line 1

    // average src line 0 with src line 1
    "vrhadd.u8    q0, q0, q2                   \n"
    "vrhadd.u8    q1, q1, q3                   \n"

    // a0 = (src[0] * 3 + s[1] * 1) >> 2
    "vmovl.u8     q3, d1                       \n"
    "vmlal.u8     q3, d0, d24                  \n"
    "vqrshrn.u16  d0, q3, #2                   \n"

    // a1 = (src[1] * 1 + s[2] * 1) >> 1
    "vrhadd.u8    d1, d1, d2                   \n"

    // a2 = (src[2] * 1 + s[3] * 3) >> 2
    "vmovl.u8     q3, d2                       \n"
    "vmlal.u8     q3, d3, d24                  \n"
    "vqrshrn.u16  d2, q3, #2                   \n"

    "vst3.u8      {d0, d1, d2}, [%1]!          \n"

    "subs         %2, #24                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    :
    : "r4", "q0", "q1", "q2", "q3", "d24", "memory", "cc"
  );
}

#define HAS_SCALEROWDOWN38_NEON
const uint8 shuf38[16] __attribute__ ((aligned(16))) =
  { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30, 0, 0, 0, 0 };
const uint8 shuf38_2[16] __attribute__ ((aligned(16))) =
  { 0, 8, 16, 2, 10, 17, 4, 12, 18, 6, 14, 19, 0, 0, 0, 0 };
const unsigned short mult38_div6[8] __attribute__ ((aligned(16))) =
  { 65536 / 12, 65536 / 12, 65536 / 12, 65536 / 12,
    65536 / 12, 65536 / 12, 65536 / 12, 65536 / 12 };
const unsigned short mult38_div9[8] __attribute__ ((aligned(16))) =
  { 65536 / 18, 65536 / 18, 65536 / 18, 65536 / 18,
    65536 / 18, 65536 / 18, 65536 / 18, 65536 / 18 };

// 32 -> 12
static void ScaleRowDown38_NEON(const uint8* src_ptr, int,
                                uint8* dst_ptr, int dst_width) {
  asm volatile (
    "vld1.u8      {q3}, [%3]                   \n"
    "1:                                        \n"
    "vld1.u8      {d0, d1, d2, d3}, [%0]!      \n"
    "vtbl.u8      d4, {d0, d1, d2, d3}, d6     \n"
    "vtbl.u8      d5, {d0, d1, d2, d3}, d7     \n"
    "vst1.u8      {d4}, [%1]!                  \n"
    "vst1.u32     {d5[0]}, [%1]!               \n"
    "subs         %2, #12                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    : "r"(shuf38)             // %3
    : "d0", "d1", "d2", "d3", "d4", "d5", "memory", "cc"
  );
}

// 32x3 -> 12x1
static void ScaleRowDown38_3_Int_NEON(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width) {
  asm volatile (
    "vld1.u16     {q13}, [%4]                  \n"
    "vld1.u8      {q14}, [%5]                  \n"
    "vld1.u8      {q15}, [%6]                  \n"
    "add          r4, %0, %3, lsl #1           \n"
    "add          %3, %0                       \n"
    "1:                                        \n"

    // d0 = 00 40 01 41 02 42 03 43
    // d1 = 10 50 11 51 12 52 13 53
    // d2 = 20 60 21 61 22 62 23 63
    // d3 = 30 70 31 71 32 72 33 73
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n"
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n"
    "vld4.u8      {d16, d17, d18, d19}, [r4]!  \n"

    // Shuffle the input data around to get align the data
    //  so adjacent data can be added.  0,1 - 2,3 - 4,5 - 6,7
    // d0 = 00 10 01 11 02 12 03 13
    // d1 = 40 50 41 51 42 52 43 53
    "vtrn.u8      d0, d1                       \n"
    "vtrn.u8      d4, d5                       \n"
    "vtrn.u8      d16, d17                     \n"

    // d2 = 20 30 21 31 22 32 23 33
    // d3 = 60 70 61 71 62 72 63 73
    "vtrn.u8      d2, d3                       \n"
    "vtrn.u8      d6, d7                       \n"
    "vtrn.u8      d18, d19                     \n"

    // d0 = 00+10 01+11 02+12 03+13
    // d2 = 40+50 41+51 42+52 43+53
    "vpaddl.u8    q0, q0                       \n"
    "vpaddl.u8    q2, q2                       \n"
    "vpaddl.u8    q8, q8                       \n"

    // d3 = 60+70 61+71 62+72 63+73
    "vpaddl.u8    d3, d3                       \n"
    "vpaddl.u8    d7, d7                       \n"
    "vpaddl.u8    d19, d19                     \n"

    // combine source lines
    "vadd.u16     q0, q2                       \n"
    "vadd.u16     q0, q8                       \n"
    "vadd.u16     d4, d3, d7                   \n"
    "vadd.u16     d4, d19                      \n"

    // dst_ptr[3] = (s[6 + st * 0] + s[7 + st * 0]
    //             + s[6 + st * 1] + s[7 + st * 1]
    //             + s[6 + st * 2] + s[7 + st * 2]) / 6
    "vqrdmulh.s16 q2, q13                      \n"
    "vmovn.u16    d4, q2                       \n"

    // Shuffle 2,3 reg around so that 2 can be added to the
    //  0,1 reg and 3 can be added to the 4,5 reg.  This
    //  requires expanding from u8 to u16 as the 0,1 and 4,5
    //  registers are already expanded.  Then do transposes
    //  to get aligned.
    // q2 = xx 20 xx 30 xx 21 xx 31 xx 22 xx 32 xx 23 xx 33
    "vmovl.u8     q1, d2                       \n"
    "vmovl.u8     q3, d6                       \n"
    "vmovl.u8     q9, d18                      \n"

    // combine source lines
    "vadd.u16     q1, q3                       \n"
    "vadd.u16     q1, q9                       \n"

    // d4 = xx 20 xx 30 xx 22 xx 32
    // d5 = xx 21 xx 31 xx 23 xx 33
    "vtrn.u32     d2, d3                       \n"

    // d4 = xx 20 xx 21 xx 22 xx 23
    // d5 = xx 30 xx 31 xx 32 xx 33
    "vtrn.u16     d2, d3                       \n"

    // 0+1+2, 3+4+5
    "vadd.u16     q0, q1                       \n"

    // Need to divide, but can't downshift as the the value
    //  isn't a power of 2.  So multiply by 65536 / n
    //  and take the upper 16 bits.
    "vqrdmulh.s16 q0, q15                      \n"

    // Align for table lookup, vtbl requires registers to
    //  be adjacent
    "vmov.u8      d2, d4                       \n"

    "vtbl.u8      d3, {d0, d1, d2}, d28        \n"
    "vtbl.u8      d4, {d0, d1, d2}, d29        \n"

    "vst1.u8      {d3}, [%1]!                  \n"
    "vst1.u32     {d4[0]}, [%1]!               \n"
    "subs         %2, #12                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    : "r"(mult38_div6),       // %4
      "r"(shuf38_2),          // %5
      "r"(mult38_div9)        // %6
    : "r4", "q0", "q1", "q2", "q3", "q8", "q9",
      "q13", "q14", "q15", "memory", "cc"
  );
}

// 32x2 -> 12x1
static void ScaleRowDown38_2_Int_NEON(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width) {
  asm volatile (
    "vld1.u16     {q13}, [%4]                  \n"
    "vld1.u8      {q14}, [%5]                  \n"
    "add          %3, %0                       \n"
    "1:                                        \n"

    // d0 = 00 40 01 41 02 42 03 43
    // d1 = 10 50 11 51 12 52 13 53
    // d2 = 20 60 21 61 22 62 23 63
    // d3 = 30 70 31 71 32 72 33 73
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n"
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n"

    // Shuffle the input data around to get align the data
    //  so adjacent data can be added.  0,1 - 2,3 - 4,5 - 6,7
    // d0 = 00 10 01 11 02 12 03 13
    // d1 = 40 50 41 51 42 52 43 53
    "vtrn.u8      d0, d1                       \n"
    "vtrn.u8      d4, d5                       \n"

    // d2 = 20 30 21 31 22 32 23 33
    // d3 = 60 70 61 71 62 72 63 73
    "vtrn.u8      d2, d3                       \n"
    "vtrn.u8      d6, d7                       \n"

    // d0 = 00+10 01+11 02+12 03+13
    // d2 = 40+50 41+51 42+52 43+53
    "vpaddl.u8    q0, q0                       \n"
    "vpaddl.u8    q2, q2                       \n"

    // d3 = 60+70 61+71 62+72 63+73
    "vpaddl.u8    d3, d3                       \n"
    "vpaddl.u8    d7, d7                       \n"

    // combine source lines
    "vadd.u16     q0, q2                       \n"
    "vadd.u16     d4, d3, d7                   \n"

    // dst_ptr[3] = (s[6] + s[7] + s[6+st] + s[7+st]) / 4
    "vqrshrn.u16  d4, q2, #2                   \n"

    // Shuffle 2,3 reg around so that 2 can be added to the
    //  0,1 reg and 3 can be added to the 4,5 reg.  This
    //  requires expanding from u8 to u16 as the 0,1 and 4,5
    //  registers are already expanded.  Then do transposes
    //  to get aligned.
    // q2 = xx 20 xx 30 xx 21 xx 31 xx 22 xx 32 xx 23 xx 33
    "vmovl.u8     q1, d2                       \n"
    "vmovl.u8     q3, d6                       \n"

    // combine source lines
    "vadd.u16     q1, q3                       \n"

    // d4 = xx 20 xx 30 xx 22 xx 32
    // d5 = xx 21 xx 31 xx 23 xx 33
    "vtrn.u32     d2, d3                       \n"

    // d4 = xx 20 xx 21 xx 22 xx 23
    // d5 = xx 30 xx 31 xx 32 xx 33
    "vtrn.u16     d2, d3                       \n"

    // 0+1+2, 3+4+5
    "vadd.u16     q0, q1                       \n"

    // Need to divide, but can't downshift as the the value
    //  isn't a power of 2.  So multiply by 65536 / n
    //  and take the upper 16 bits.
    "vqrdmulh.s16 q0, q13                      \n"

    // Align for table lookup, vtbl requires registers to
    //  be adjacent
    "vmov.u8      d2, d4                       \n"

    "vtbl.u8      d3, {d0, d1, d2}, d28        \n"
    "vtbl.u8      d4, {d0, d1, d2}, d29        \n"

    "vst1.u8      {d3}, [%1]!                  \n"
    "vst1.u32     {d4[0]}, [%1]!               \n"
    "subs         %2, #12                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    : "r"(mult38_div6),       // %4
      "r"(shuf38_2)           // %5
    : "q0", "q1", "q2", "q3", "q13", "q14", "memory", "cc"
  );
}

/**
 * SSE2 downscalers with interpolation.
 *
 * Provided by Frank Barchard (fbarchard@google.com)
 *
 */

// Constants for SSE2 code
#elif (defined(_M_IX86) || defined(__i386__) || defined(__x86_64__)) && \
    !defined(YUV_DISABLE_ASM)
#if defined(_MSC_VER)
#define TALIGN16(t, var) __declspec(align(16)) t _ ## var
#elif (defined(__APPLE__) || defined(__MINGW32__) || defined(__CYGWIN__)) && \
    defined(__i386__)
#define TALIGN16(t, var) t var __attribute__((aligned(16)))
#else
#define TALIGN16(t, var) t _ ## var __attribute__((aligned(16)))
#endif

#if defined(__APPLE__) && defined(__i386__)
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".private_extern _" #name "                \n"                             \
    ".align 4,0x90                             \n"                             \
"_" #name ":                                   \n"
#elif (defined(__MINGW32__) || defined(__CYGWIN__)) && defined(__i386__)
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".align 4,0x90                             \n"                             \
"_" #name ":                                   \n"
#else
#define DECLARE_FUNCTION(name)                                                 \
    ".text                                     \n"                             \
    ".align 4,0x90                             \n"                             \
#name ":                                       \n"
#endif

// Offsets for source bytes 0 to 9
extern "C" TALIGN16(const uint8, shuf0[16]) =
  { 0, 1, 3, 4, 5, 7, 8, 9, 128, 128, 128, 128, 128, 128, 128, 128 };

// Offsets for source bytes 11 to 20 with 8 subtracted = 3 to 12.
extern "C" TALIGN16(const uint8, shuf1[16]) =
  { 3, 4, 5, 7, 8, 9, 11, 12, 128, 128, 128, 128, 128, 128, 128, 128 };

// Offsets for source bytes 21 to 31 with 16 subtracted = 5 to 31.
extern "C" TALIGN16(const uint8, shuf2[16]) =
  { 5, 7, 8, 9, 11, 12, 13, 15, 128, 128, 128, 128, 128, 128, 128, 128 };

// Offsets for source bytes 0 to 10
extern "C" TALIGN16(const uint8, shuf01[16]) =
  { 0, 1, 1, 2, 2, 3, 4, 5, 5, 6, 6, 7, 8, 9, 9, 10 };

// Offsets for source bytes 10 to 21 with 8 subtracted = 3 to 13.
extern "C" TALIGN16(const uint8, shuf11[16]) =
  { 2, 3, 4, 5, 5, 6, 6, 7, 8, 9, 9, 10, 10, 11, 12, 13 };

// Offsets for source bytes 21 to 31 with 16 subtracted = 5 to 31.
extern "C" TALIGN16(const uint8, shuf21[16]) =
  { 5, 6, 6, 7, 8, 9, 9, 10, 10, 11, 12, 13, 13, 14, 14, 15 };

// Coefficients for source bytes 0 to 10
extern "C" TALIGN16(const uint8, madd01[16]) =
  { 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2 };

// Coefficients for source bytes 10 to 21
extern "C" TALIGN16(const uint8, madd11[16]) =
  { 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1 };

// Coefficients for source bytes 21 to 31
extern "C" TALIGN16(const uint8, madd21[16]) =
  { 2, 2, 1, 3, 3, 1, 2, 2, 1, 3, 3, 1, 2, 2, 1, 3 };

// Coefficients for source bytes 21 to 31
extern "C" TALIGN16(const int16, round34[8]) =
  { 2, 2, 2, 2, 2, 2, 2, 2 };

extern "C" TALIGN16(const uint8, shuf38a[16]) =
  { 0, 3, 6, 8, 11, 14, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 };

extern "C" TALIGN16(const uint8, shuf38b[16]) =
  { 128, 128, 128, 128, 128, 128, 0, 3, 6, 8, 11, 14, 128, 128, 128, 128 };

// Arrange words 0,3,6 into 0,1,2
extern "C" TALIGN16(const uint8, shufac0[16]) =
  { 0, 1, 6, 7, 12, 13, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 };

// Arrange words 0,3,6 into 3,4,5
extern "C" TALIGN16(const uint8, shufac3[16]) =
  { 128, 128, 128, 128, 128, 128, 0, 1, 6, 7, 12, 13, 128, 128, 128, 128 };

// Scaling values for boxes of 3x3 and 2x3
extern "C" TALIGN16(const uint16, scaleac3[8]) =
  { 65536 / 9, 65536 / 9, 65536 / 6, 65536 / 9, 65536 / 9, 65536 / 6, 0, 0 };

// Arrange first value for pixels 0,1,2,3,4,5
extern "C" TALIGN16(const uint8, shufab0[16]) =
  { 0, 128, 3, 128, 6, 128, 8, 128, 11, 128, 14, 128, 128, 128, 128, 128 };

// Arrange second value for pixels 0,1,2,3,4,5
extern "C" TALIGN16(const uint8, shufab1[16]) =
  { 1, 128, 4, 128, 7, 128, 9, 128, 12, 128, 15, 128, 128, 128, 128, 128 };

// Arrange third value for pixels 0,1,2,3,4,5
extern "C" TALIGN16(const uint8, shufab2[16]) =
  { 2, 128, 5, 128, 128, 128, 10, 128, 13, 128, 128, 128, 128, 128, 128, 128 };

// Scaling values for boxes of 3x2 and 2x2
extern "C" TALIGN16(const uint16, scaleab2[8]) =
  { 65536 / 3, 65536 / 3, 65536 / 2, 65536 / 3, 65536 / 3, 65536 / 2, 0, 0 };
#endif

#if defined(_M_IX86) && !defined(YUV_DISABLE_ASM)

#define HAS_SCALEROWDOWN2_SSE2
// Reads 32 pixels, throws half away and writes 16 pixels.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 16 byte aligned.
__declspec(naked)
static void ScaleRowDown2_SSE2(const uint8* src_ptr, int src_stride,
                               uint8* dst_ptr, int dst_width) {
  __asm {
    mov        eax, [esp + 4]        // src_ptr
                                     // src_stride ignored
    mov        edx, [esp + 12]       // dst_ptr
    mov        ecx, [esp + 16]       // dst_width
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8

  wloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    lea        eax,  [eax + 32]
    pand       xmm0, xmm5
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    ja         wloop

    ret
  }
}
// Blends 32x2 rectangle to 16x1.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 16 byte aligned.
__declspec(naked)
void ScaleRowDown2Int_SSE2(const uint8* src_ptr, int src_stride,
                           uint8* dst_ptr, int dst_width) {
  __asm {
    push       esi
    mov        eax, [esp + 4 + 4]    // src_ptr
    mov        esi, [esp + 4 + 8]    // src_stride
    mov        edx, [esp + 4 + 12]   // dst_ptr
    mov        ecx, [esp + 4 + 16]   // dst_width
    pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
    psrlw      xmm5, 8

  wloop:
    movdqa     xmm0, [eax]
    movdqa     xmm1, [eax + 16]
    movdqa     xmm2, [eax + esi]
    movdqa     xmm3, [eax + esi + 16]
    lea        eax,  [eax + 32]
    pavgb      xmm0, xmm2            // average rows
    pavgb      xmm1, xmm3

    movdqa     xmm2, xmm0            // average columns (32 to 16 pixels)
    psrlw      xmm0, 8
    movdqa     xmm3, xmm1
    psrlw      xmm1, 8
    pand       xmm2, xmm5
    pand       xmm3, xmm5
    pavgw      xmm0, xmm2
    pavgw      xmm1, xmm3
    packuswb   xmm0, xmm1

    movdqa     [edx], xmm0
    lea        edx, [edx + 16]
    sub        ecx, 16
    ja         wloop

    pop        esi
    ret
  }
}

#define HAS_SCALEROWDOWN4_SSE2
// Point samples 32 pixels to 8 pixels.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 8 byte aligned.
__declspec(naked)
static void ScaleRowDown4_SSE2(const uint8* src_ptr, int src_stride,
                               uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
                                     // src_stride ignored
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    pcmpeqb    xmm5, xmm5            // generate mask 0x000000ff
    psrld      xmm5, 24

  wloop:
    movdqa     xmm0, [esi]
    movdqa     xmm1, [esi + 16]
    lea        esi,  [esi + 32]
    pand       xmm0, xmm5
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1
    packuswb   xmm0, xmm0
    movq       qword ptr [edi], xmm0
    lea        edi, [edi + 8]
    sub        ecx, 8
    ja         wloop

    popad
    ret
  }
}

// Blends 32x4 rectangle to 8x1.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 8 byte aligned.
__declspec(naked)
static void ScaleRowDown4Int_SSE2(const uint8* src_ptr, int src_stride,
                                  uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        ebx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    pcmpeqb    xmm7, xmm7            // generate mask 0x00ff00ff
    psrlw      xmm7, 8
    lea        edx, [ebx + ebx * 2]  // src_stride * 3

  wloop:
    movdqa     xmm0, [esi]
    movdqa     xmm1, [esi + 16]
    movdqa     xmm2, [esi + ebx]
    movdqa     xmm3, [esi + ebx + 16]
    pavgb      xmm0, xmm2            // average rows
    pavgb      xmm1, xmm3
    movdqa     xmm2, [esi + ebx * 2]
    movdqa     xmm3, [esi + ebx * 2 + 16]
    movdqa     xmm4, [esi + edx]
    movdqa     xmm5, [esi + edx + 16]
    lea        esi, [esi + 32]
    pavgb      xmm2, xmm4
    pavgb      xmm3, xmm5
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3

    movdqa     xmm2, xmm0            // average columns (32 to 16 pixels)
    psrlw      xmm0, 8
    movdqa     xmm3, xmm1
    psrlw      xmm1, 8
    pand       xmm2, xmm7
    pand       xmm3, xmm7
    pavgw      xmm0, xmm2
    pavgw      xmm1, xmm3
    packuswb   xmm0, xmm1

    movdqa     xmm2, xmm0            // average columns (16 to 8 pixels)
    psrlw      xmm0, 8
    pand       xmm2, xmm7
    pavgw      xmm0, xmm2
    packuswb   xmm0, xmm0

    movq       qword ptr [edi], xmm0
    lea        edi, [edi + 8]
    sub        ecx, 8
    ja         wloop

    popad
    ret
  }
}

#define HAS_SCALEROWDOWN8_SSE2
// Point samples 32 pixels to 4 pixels.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 4 byte aligned.
__declspec(naked)
static void ScaleRowDown8_SSE2(const uint8* src_ptr, int src_stride,
                               uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
                                     // src_stride ignored
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    pcmpeqb    xmm5, xmm5            // generate mask isolating 1 src 8 bytes
    psrlq      xmm5, 56

  wloop:
    movdqa     xmm0, [esi]
    movdqa     xmm1, [esi + 16]
    lea        esi,  [esi + 32]
    pand       xmm0, xmm5
    pand       xmm1, xmm5
    packuswb   xmm0, xmm1  // 32->16
    packuswb   xmm0, xmm0  // 16->8
    packuswb   xmm0, xmm0  // 8->4
    movd       dword ptr [edi], xmm0
    lea        edi, [edi + 4]
    sub        ecx, 4
    ja         wloop

    popad
    ret
  }
}

// Blends 32x8 rectangle to 4x1.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 4 byte aligned.
__declspec(naked)
static void ScaleRowDown8Int_SSE2(const uint8* src_ptr, int src_stride,
                                  uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        ebx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    lea        edx, [ebx + ebx * 2]  // src_stride * 3
    pxor       xmm7, xmm7

  wloop:
    movdqa     xmm0, [esi]           // average 8 rows to 1
    movdqa     xmm1, [esi + 16]
    movdqa     xmm2, [esi + ebx]
    movdqa     xmm3, [esi + ebx + 16]
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3
    movdqa     xmm2, [esi + ebx * 2]
    movdqa     xmm3, [esi + ebx * 2 + 16]
    movdqa     xmm4, [esi + edx]
    movdqa     xmm5, [esi + edx + 16]
    lea        ebp, [esi + ebx * 4]
    lea        esi, [esi + 32]
    pavgb      xmm2, xmm4
    pavgb      xmm3, xmm5
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3

    movdqa     xmm2, [ebp]
    movdqa     xmm3, [ebp + 16]
    movdqa     xmm4, [ebp + ebx]
    movdqa     xmm5, [ebp + ebx + 16]
    pavgb      xmm2, xmm4
    pavgb      xmm3, xmm5
    movdqa     xmm4, [ebp + ebx * 2]
    movdqa     xmm5, [ebp + ebx * 2 + 16]
    movdqa     xmm6, [ebp + edx]
    pavgb      xmm4, xmm6
    movdqa     xmm6, [ebp + edx + 16]
    pavgb      xmm5, xmm6
    pavgb      xmm2, xmm4
    pavgb      xmm3, xmm5
    pavgb      xmm0, xmm2
    pavgb      xmm1, xmm3

    psadbw     xmm0, xmm7            // average 32 pixels to 4
    psadbw     xmm1, xmm7
    pshufd     xmm0, xmm0, 0xd8      // x1x0 -> xx01
    pshufd     xmm1, xmm1, 0x8d      // x3x2 -> 32xx
    por        xmm0, xmm1            //      -> 3201
    psrlw      xmm0, 3
    packuswb   xmm0, xmm0
    packuswb   xmm0, xmm0
    movd       dword ptr [edi], xmm0

    lea        edi, [edi + 4]
    sub        ecx, 4
    ja         wloop

    popad
    ret
  }
}

#define HAS_SCALEROWDOWN34_SSSE3
// Point samples 32 pixels to 24 pixels.
// Produces three 8 byte values.  For each 8 bytes, 16 bytes are read.
// Then shuffled to do the scaling.

// Note that movdqa+palign may be better than movdqu.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 8 byte aligned.
__declspec(naked)
static void ScaleRowDown34_SSSE3(const uint8* src_ptr, int src_stride,
                                 uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
                                     // src_stride ignored
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    movdqa     xmm3, _shuf0
    movdqa     xmm4, _shuf1
    movdqa     xmm5, _shuf2

  wloop:
    movdqa     xmm0, [esi]
    movdqa     xmm1, [esi + 16]
    lea        esi,  [esi + 32]
    movdqa     xmm2, xmm1
    palignr    xmm1, xmm0, 8
    pshufb     xmm0, xmm3
    pshufb     xmm1, xmm4
    pshufb     xmm2, xmm5
    movq       qword ptr [edi], xmm0
    movq       qword ptr [edi + 8], xmm1
    movq       qword ptr [edi + 16], xmm2
    lea        edi, [edi + 24]
    sub        ecx, 24
    ja         wloop

    popad
    ret
  }
}

// Blends 32x2 rectangle to 24x1
// Produces three 8 byte values.  For each 8 bytes, 16 bytes are read.
// Then shuffled to do the scaling.

// Register usage:
// xmm0 src_row 0
// xmm1 src_row 1
// xmm2 shuf 0
// xmm3 shuf 1
// xmm4 shuf 2
// xmm5 madd 0
// xmm6 madd 1
// xmm7 round34

// Note that movdqa+palign may be better than movdqu.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 8 byte aligned.
__declspec(naked)
static void ScaleRowDown34_1_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        ebx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    movdqa     xmm2, _shuf01
    movdqa     xmm3, _shuf11
    movdqa     xmm4, _shuf21
    movdqa     xmm5, _madd01
    movdqa     xmm6, _madd11
    movdqa     xmm7, _round34

  wloop:
    movdqa     xmm0, [esi]           // pixels 0..7
    movdqa     xmm1, [esi+ebx]
    pavgb      xmm0, xmm1
    pshufb     xmm0, xmm2
    pmaddubsw  xmm0, xmm5
    paddsw     xmm0, xmm7
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edi], xmm0
    movdqu     xmm0, [esi+8]         // pixels 8..15
    movdqu     xmm1, [esi+ebx+8]
    pavgb      xmm0, xmm1
    pshufb     xmm0, xmm3
    pmaddubsw  xmm0, xmm6
    paddsw     xmm0, xmm7
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edi+8], xmm0
    movdqa     xmm0, [esi+16]        // pixels 16..23
    movdqa     xmm1, [esi+ebx+16]
    lea        esi, [esi+32]
    pavgb      xmm0, xmm1
    pshufb     xmm0, xmm4
    movdqa     xmm1, _madd21
    pmaddubsw  xmm0, xmm1
    paddsw     xmm0, xmm7
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edi+16], xmm0
    lea        edi, [edi+24]
    sub        ecx, 24
    ja         wloop

    popad
    ret
  }
}

// Note that movdqa+palign may be better than movdqu.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 8 byte aligned.
__declspec(naked)
static void ScaleRowDown34_0_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        ebx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    movdqa     xmm2, _shuf01
    movdqa     xmm3, _shuf11
    movdqa     xmm4, _shuf21
    movdqa     xmm5, _madd01
    movdqa     xmm6, _madd11
    movdqa     xmm7, _round34

  wloop:
    movdqa     xmm0, [esi]           // pixels 0..7
    movdqa     xmm1, [esi+ebx]
    pavgb      xmm1, xmm0
    pavgb      xmm0, xmm1
    pshufb     xmm0, xmm2
    pmaddubsw  xmm0, xmm5
    paddsw     xmm0, xmm7
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edi], xmm0
    movdqu     xmm0, [esi+8]         // pixels 8..15
    movdqu     xmm1, [esi+ebx+8]
    pavgb      xmm1, xmm0
    pavgb      xmm0, xmm1
    pshufb     xmm0, xmm3
    pmaddubsw  xmm0, xmm6
    paddsw     xmm0, xmm7
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edi+8], xmm0
    movdqa     xmm0, [esi+16]        // pixels 16..23
    movdqa     xmm1, [esi+ebx+16]
    lea        esi, [esi+32]
    pavgb      xmm1, xmm0
    pavgb      xmm0, xmm1
    pshufb     xmm0, xmm4
    movdqa     xmm1, _madd21
    pmaddubsw  xmm0, xmm1
    paddsw     xmm0, xmm7
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edi+16], xmm0
    lea        edi, [edi+24]
    sub        ecx, 24
    ja         wloop

    popad
    ret
  }
}

#define HAS_SCALEROWDOWN38_SSSE3
// 3/8 point sampler

// Scale 32 pixels to 12
__declspec(naked)
static void ScaleRowDown38_SSSE3(const uint8* src_ptr, int src_stride,
                                 uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        edx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    movdqa     xmm4, _shuf38a
    movdqa     xmm5, _shuf38b

  xloop:
    movdqa     xmm0, [esi]           // 16 pixels -> 0,1,2,3,4,5
    movdqa     xmm1, [esi + 16]      // 16 pixels -> 6,7,8,9,10,11
    lea        esi, [esi + 32]
    pshufb     xmm0, xmm4
    pshufb     xmm1, xmm5
    paddusb    xmm0, xmm1

    movq       qword ptr [edi], xmm0 // write 12 pixels
    movhlps    xmm1, xmm0
    movd       [edi + 8], xmm1
    lea        edi, [edi + 12]
    sub        ecx, 12
    ja         xloop

    popad
    ret
  }
}

// Scale 16x3 pixels to 6x1 with interpolation
__declspec(naked)
static void ScaleRowDown38_3_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        edx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    movdqa     xmm4, _shufac0
    movdqa     xmm5, _shufac3
    movdqa     xmm6, _scaleac3
    pxor       xmm7, xmm7

  xloop:
    movdqa     xmm0, [esi]           // sum up 3 rows into xmm0/1
    movdqa     xmm2, [esi + edx]
    movhlps    xmm1, xmm0
    movhlps    xmm3, xmm2
    punpcklbw  xmm0, xmm7
    punpcklbw  xmm1, xmm7
    punpcklbw  xmm2, xmm7
    punpcklbw  xmm3, xmm7
    paddusw    xmm0, xmm2
    paddusw    xmm1, xmm3
    movdqa     xmm2, [esi + edx * 2]
    lea        esi, [esi + 16]
    movhlps    xmm3, xmm2
    punpcklbw  xmm2, xmm7
    punpcklbw  xmm3, xmm7
    paddusw    xmm0, xmm2
    paddusw    xmm1, xmm3

    movdqa     xmm2, xmm0            // 8 pixels -> 0,1,2 of xmm2
    psrldq     xmm0, 2
    paddusw    xmm2, xmm0
    psrldq     xmm0, 2
    paddusw    xmm2, xmm0
    pshufb     xmm2, xmm4

    movdqa     xmm3, xmm1            // 8 pixels -> 3,4,5 of xmm2
    psrldq     xmm1, 2
    paddusw    xmm3, xmm1
    psrldq     xmm1, 2
    paddusw    xmm3, xmm1
    pshufb     xmm3, xmm5
    paddusw    xmm2, xmm3

    pmulhuw    xmm2, xmm6            // divide by 9,9,6, 9,9,6
    packuswb   xmm2, xmm2

    movd       [edi], xmm2           // write 6 pixels
    pextrw     eax, xmm2, 2
    mov        [edi + 4], ax
    lea        edi, [edi + 6]
    sub        ecx, 6
    ja         xloop

    popad
    ret
  }
}

// Scale 16x2 pixels to 6x1 with interpolation
__declspec(naked)
static void ScaleRowDown38_2_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        edx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    movdqa     xmm4, _shufab0
    movdqa     xmm5, _shufab1
    movdqa     xmm6, _shufab2
    movdqa     xmm7, _scaleab2

  xloop:
    movdqa     xmm2, [esi]           // average 2 rows into xmm2
    pavgb      xmm2, [esi + edx]
    lea        esi, [esi + 16]

    movdqa     xmm0, xmm2            // 16 pixels -> 0,1,2,3,4,5 of xmm0
    pshufb     xmm0, xmm4
    movdqa     xmm1, xmm2
    pshufb     xmm1, xmm5
    paddusw    xmm0, xmm1
    pshufb     xmm2, xmm6
    paddusw    xmm0, xmm2

    pmulhuw    xmm0, xmm7            // divide by 3,3,2, 3,3,2
    packuswb   xmm0, xmm0

    movd       [edi], xmm0           // write 6 pixels
    pextrw     eax, xmm0, 2
    mov        [edi + 4], ax
    lea        edi, [edi + 6]
    sub        ecx, 6
    ja         xloop

    popad
    ret
  }
}

#define HAS_SCALEADDROWS_SSE2

// Reads 8xN bytes and produces 16 shorts at a time.
__declspec(naked)
static void ScaleAddRows_SSE2(const uint8* src_ptr, int src_stride,
                              uint16* dst_ptr, int src_width,
                              int src_height) {
  __asm {
    pushad
    mov        esi, [esp + 32 + 4]   // src_ptr
    mov        edx, [esp + 32 + 8]   // src_stride
    mov        edi, [esp + 32 + 12]  // dst_ptr
    mov        ecx, [esp + 32 + 16]  // dst_width
    mov        ebx, [esp + 32 + 20]  // height
    pxor       xmm5, xmm5
    dec        ebx

  xloop:
    // first row
    movdqa     xmm2, [esi]
    lea        eax, [esi + edx]
    movhlps    xmm3, xmm2
    mov        ebp, ebx
    punpcklbw  xmm2, xmm5
    punpcklbw  xmm3, xmm5

    // sum remaining rows
  yloop:
    movdqa     xmm0, [eax]       // read 16 pixels
    lea        eax, [eax + edx]  // advance to next row
    movhlps    xmm1, xmm0
    punpcklbw  xmm0, xmm5
    punpcklbw  xmm1, xmm5
    paddusw    xmm2, xmm0        // sum 16 words
    paddusw    xmm3, xmm1
    sub        ebp, 1
    ja         yloop

    movdqa     [edi], xmm2
    movdqa     [edi + 16], xmm3
    lea        edi, [edi + 32]
    lea        esi, [esi + 16]

    sub        ecx, 16
    ja         xloop

    popad
    ret
  }
}

// Bilinear row filtering combines 16x2 -> 16x1. SSE2 version.
#define HAS_SCALEFILTERROWS_SSE2
__declspec(naked)
static void ScaleFilterRows_SSE2(uint8* dst_ptr, const uint8* src_ptr,
                                 int src_stride, int dst_width,
                                 int source_y_fraction) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]   // dst_ptr
    mov        esi, [esp + 8 + 8]   // src_ptr
    mov        edx, [esp + 8 + 12]  // src_stride
    mov        ecx, [esp + 8 + 16]  // dst_width
    mov        eax, [esp + 8 + 20]  // source_y_fraction (0..255)
    cmp        eax, 0
    je         xloop1
    cmp        eax, 128
    je         xloop2

    movd       xmm6, eax            // xmm6 = y fraction
    punpcklwd  xmm6, xmm6
    pshufd     xmm6, xmm6, 0
    neg        eax                  // xmm5 = 256 - y fraction
    add        eax, 256
    movd       xmm5, eax
    punpcklwd  xmm5, xmm5
    pshufd     xmm5, xmm5, 0
    pxor       xmm7, xmm7

  xloop:
    movdqa     xmm0, [esi]
    movdqa     xmm2, [esi + edx]
    lea        esi, [esi + 16]
    movdqa     xmm1, xmm0
    movdqa     xmm3, xmm2
    punpcklbw  xmm0, xmm7
    punpcklbw  xmm2, xmm7
    punpckhbw  xmm1, xmm7
    punpckhbw  xmm3, xmm7
    pmullw     xmm0, xmm5           // scale row 0
    pmullw     xmm1, xmm5
    pmullw     xmm2, xmm6           // scale row 1
    pmullw     xmm3, xmm6
    paddusw    xmm0, xmm2           // sum rows
    paddusw    xmm1, xmm3
    psrlw      xmm0, 8
    psrlw      xmm1, 8
    packuswb   xmm0, xmm1
    movdqa     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 16
    ja         xloop

    mov        al, [edi - 1]
    mov        [edi], al
    pop        edi
    pop        esi
    ret

  xloop1:
    movdqa     xmm0, [esi]
    lea        esi, [esi + 16]
    movdqa     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 16
    ja         xloop1

    mov        al, [edi - 1]
    mov        [edi], al
    pop        edi
    pop        esi
    ret

  xloop2:
    movdqa     xmm0, [esi]
    movdqa     xmm2, [esi + edx]
    lea        esi, [esi + 16]
    pavgb      xmm0, xmm2
    movdqa     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 16
    ja         xloop2

    mov        al, [edi - 1]
    mov        [edi], al
    pop        edi
    pop        esi
    ret
  }
}

// Bilinear row filtering combines 16x2 -> 16x1. SSSE3 version.
#define HAS_SCALEFILTERROWS_SSSE3
__declspec(naked)
static void ScaleFilterRows_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                                  int src_stride, int dst_width,
                                  int source_y_fraction) {
  __asm {
    push       esi
    push       edi
    mov        edi, [esp + 8 + 4]   // dst_ptr
    mov        esi, [esp + 8 + 8]   // src_ptr
    mov        edx, [esp + 8 + 12]  // src_stride
    mov        ecx, [esp + 8 + 16]  // dst_width
    mov        eax, [esp + 8 + 20]  // source_y_fraction (0..255)
    cmp        eax, 0
    je         xloop1
    cmp        eax, 128
    je         xloop2

    shr        eax, 1
    mov        ah,al
    neg        al
    add        al, 128
    movd       xmm5, eax
    punpcklwd  xmm5, xmm5
    pshufd     xmm5, xmm5, 0

  xloop:
    movdqa     xmm0, [esi]
    movdqa     xmm2, [esi + edx]
    lea        esi, [esi + 16]
    movdqa     xmm1, xmm0
    punpcklbw  xmm0, xmm2
    punpckhbw  xmm1, xmm2
    pmaddubsw  xmm0, xmm5
    pmaddubsw  xmm1, xmm5
    psrlw      xmm0, 7
    psrlw      xmm1, 7
    packuswb   xmm0, xmm1
    movdqa     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 16
    ja         xloop

    mov        al, [edi - 1]
    mov        [edi], al
    pop        edi
    pop        esi
    ret

  xloop1:
    movdqa     xmm0, [esi]
    lea        esi, [esi + 16]
    movdqa     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 16
    ja         xloop1

    mov        al, [edi - 1]
    mov        [edi], al
    pop        edi
    pop        esi
    ret

  xloop2:
    movdqa     xmm0, [esi]
    movdqa     xmm2, [esi + edx]
    lea        esi, [esi + 16]
    pavgb      xmm0, xmm2
    movdqa     [edi], xmm0
    lea        edi, [edi + 16]
    sub        ecx, 16
    ja         xloop2

    mov        al, [edi - 1]
    mov        [edi], al
    pop        edi
    pop        esi
    ret

  }
}

// Note that movdqa+palign may be better than movdqu.
// Alignment requirement: src_ptr 16 byte aligned, dst_ptr 8 byte aligned.
__declspec(naked)
static void ScaleFilterCols34_SSSE3(uint8* dst_ptr, const uint8* src_ptr,
                                    int dst_width) {
  __asm {
    mov        edx, [esp + 4]    // dst_ptr
    mov        eax, [esp + 8]    // src_ptr
    mov        ecx, [esp + 12]   // dst_width
    movdqa     xmm1, _round34
    movdqa     xmm2, _shuf01
    movdqa     xmm3, _shuf11
    movdqa     xmm4, _shuf21
    movdqa     xmm5, _madd01
    movdqa     xmm6, _madd11
    movdqa     xmm7, _madd21

  wloop:
    movdqa     xmm0, [eax]           // pixels 0..7
    pshufb     xmm0, xmm2
    pmaddubsw  xmm0, xmm5
    paddsw     xmm0, xmm1
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edx], xmm0
    movdqu     xmm0, [eax+8]         // pixels 8..15
    pshufb     xmm0, xmm3
    pmaddubsw  xmm0, xmm6
    paddsw     xmm0, xmm1
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edx+8], xmm0
    movdqa     xmm0, [eax+16]        // pixels 16..23
    lea        eax, [eax+32]
    pshufb     xmm0, xmm4
    pmaddubsw  xmm0, xmm7
    paddsw     xmm0, xmm1
    psrlw      xmm0, 2
    packuswb   xmm0, xmm0
    movq       qword ptr [edx+16], xmm0
    lea        edx, [edx+24]
    sub        ecx, 24
    ja         wloop
    ret
  }
}

#elif (defined(__x86_64__) || defined(__i386__)) && !defined(YUV_DISABLE_ASM)

// GCC versions of row functions are verbatim conversions from Visual C.
// Generated using gcc disassembly on Visual C object file:
// objdump -D yuvscaler.obj >yuvscaler.txt
#define HAS_SCALEROWDOWN2_SSE2
static void ScaleRowDown2_SSE2(const uint8* src_ptr, int src_stride,
                               uint8* dst_ptr, int dst_width) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),    // %0
    "+r"(dst_ptr),    // %1
    "+r"(dst_width)   // %2
  :
  : "memory", "cc"
);
}

static void ScaleRowDown2Int_SSE2(const uint8* src_ptr, int src_stride,
                                  uint8* dst_ptr, int dst_width) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlw      $0x8,%%xmm5                      \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     (%0,%3,1),%%xmm2                 \n"
  "movdqa     0x10(%0,%3,1),%%xmm3             \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "movdqa     %%xmm1,%%xmm3                    \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "pand       %%xmm5,%%xmm2                    \n"
  "pand       %%xmm5,%%xmm3                    \n"
  "pavgw      %%xmm2,%%xmm0                    \n"
  "pavgw      %%xmm3,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,(%1)                      \n"
  "lea        0x10(%1),%1                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),    // %0
    "+r"(dst_ptr),    // %1
    "+r"(dst_width)   // %2
  : "r"(static_cast<intptr_t>(src_stride))   // %3
  : "memory", "cc"
);
}

#define HAS_SCALEROWDOWN4_SSE2
static void ScaleRowDown4_SSE2(const uint8* src_ptr, int src_stride,
                               uint8* dst_ptr, int dst_width) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrld      $0x18,%%xmm5                     \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "lea        0x8(%1),%1                       \n"
  "sub        $0x8,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),    // %0
    "+r"(dst_ptr),    // %1
    "+r"(dst_width)   // %2
  :
  : "memory", "cc"
);
}

static void ScaleRowDown4Int_SSE2(const uint8* src_ptr, int src_stride,
                                  uint8* dst_ptr, int dst_width) {
  intptr_t temp = 0;
  asm volatile (
  "pcmpeqb    %%xmm7,%%xmm7                    \n"
  "psrlw      $0x8,%%xmm7                      \n"
  "lea        (%4,%4,2),%3                     \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     (%0,%4,1),%%xmm2                 \n"
  "movdqa     0x10(%0,%4,1),%%xmm3             \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "movdqa     (%0,%4,2),%%xmm2                 \n"
  "movdqa     0x10(%0,%4,2),%%xmm3             \n"
  "movdqa     (%0,%3,1),%%xmm4                 \n"
  "movdqa     0x10(%0,%3,1),%%xmm5             \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm4,%%xmm2                    \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm5,%%xmm3                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "movdqa     %%xmm1,%%xmm3                    \n"
  "psrlw      $0x8,%%xmm1                      \n"
  "pand       %%xmm7,%%xmm2                    \n"
  "pand       %%xmm7,%%xmm3                    \n"
  "pavgw      %%xmm2,%%xmm0                    \n"
  "pavgw      %%xmm3,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "psrlw      $0x8,%%xmm0                      \n"
  "pand       %%xmm7,%%xmm2                    \n"
  "pavgw      %%xmm2,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "lea        0x8(%1),%1                       \n"
  "sub        $0x8,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width),   // %2
    "+r"(temp)         // %3
  : "r"(static_cast<intptr_t>(src_stride))    // %4
  : "memory", "cc"
#if defined(__x86_64__)
    , "xmm6", "xmm7"
#endif
);
}

#define HAS_SCALEROWDOWN8_SSE2
static void ScaleRowDown8_SSE2(const uint8* src_ptr, int src_stride,
                               uint8* dst_ptr, int dst_width) {
  asm volatile (
  "pcmpeqb    %%xmm5,%%xmm5                    \n"
  "psrlq      $0x38,%%xmm5                     \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "pand       %%xmm5,%%xmm0                    \n"
  "pand       %%xmm5,%%xmm1                    \n"
  "packuswb   %%xmm1,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movd       %%xmm0,(%1)                      \n"
  "lea        0x4(%1),%1                       \n"
  "sub        $0x4,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),    // %0
    "+r"(dst_ptr),    // %1
    "+r"(dst_width)   // %2
  :
  : "memory", "cc"
);
}

#if defined(__i386__)
extern "C" void ScaleRowDown8Int_SSE2(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown8Int_SSE2)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%ebx                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "lea    (%ebx,%ebx,2),%edx                 \n"
    "pxor   %xmm7,%xmm7                        \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa 0x10(%esi),%xmm1                   \n"
    "movdqa (%esi,%ebx,1),%xmm2                \n"
    "movdqa 0x10(%esi,%ebx,1),%xmm3            \n"
    "pavgb  %xmm2,%xmm0                        \n"
    "pavgb  %xmm3,%xmm1                        \n"
    "movdqa (%esi,%ebx,2),%xmm2                \n"
    "movdqa 0x10(%esi,%ebx,2),%xmm3            \n"
    "movdqa (%esi,%edx,1),%xmm4                \n"
    "movdqa 0x10(%esi,%edx,1),%xmm5            \n"
    "lea    (%esi,%ebx,4),%ebp                 \n"
    "lea    0x20(%esi),%esi                    \n"
    "pavgb  %xmm4,%xmm2                        \n"
    "pavgb  %xmm5,%xmm3                        \n"
    "pavgb  %xmm2,%xmm0                        \n"
    "pavgb  %xmm3,%xmm1                        \n"
    "movdqa 0x0(%ebp),%xmm2                    \n"
    "movdqa 0x10(%ebp),%xmm3                   \n"
    "movdqa 0x0(%ebp,%ebx,1),%xmm4             \n"
    "movdqa 0x10(%ebp,%ebx,1),%xmm5            \n"
    "pavgb  %xmm4,%xmm2                        \n"
    "pavgb  %xmm5,%xmm3                        \n"
    "movdqa 0x0(%ebp,%ebx,2),%xmm4             \n"
    "movdqa 0x10(%ebp,%ebx,2),%xmm5            \n"
    "movdqa 0x0(%ebp,%edx,1),%xmm6             \n"
    "pavgb  %xmm6,%xmm4                        \n"
    "movdqa 0x10(%ebp,%edx,1),%xmm6            \n"
    "pavgb  %xmm6,%xmm5                        \n"
    "pavgb  %xmm4,%xmm2                        \n"
    "pavgb  %xmm5,%xmm3                        \n"
    "pavgb  %xmm2,%xmm0                        \n"
    "pavgb  %xmm3,%xmm1                        \n"
    "psadbw %xmm7,%xmm0                        \n"
    "psadbw %xmm7,%xmm1                        \n"
    "pshufd $0xd8,%xmm0,%xmm0                  \n"
    "pshufd $0x8d,%xmm1,%xmm1                  \n"
    "por    %xmm1,%xmm0                        \n"
    "psrlw  $0x3,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movd   %xmm0,(%edi)                       \n"
    "lea    0x4(%edi),%edi                     \n"
    "sub    $0x4,%ecx                          \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);

// fpic is used for magiccam plugin
#if !defined(__PIC__)
#define HAS_SCALEROWDOWN34_SSSE3
extern "C" void ScaleRowDown34_SSSE3(const uint8* src_ptr, int src_stride,
                                     uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown34_SSSE3)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "movdqa _shuf0,%xmm3                       \n"
    "movdqa _shuf1,%xmm4                       \n"
    "movdqa _shuf2,%xmm5                       \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa 0x10(%esi),%xmm2                   \n"
    "lea    0x20(%esi),%esi                    \n"
    "movdqa %xmm2,%xmm1                        \n"
    "palignr $0x8,%xmm0,%xmm1                  \n"
    "pshufb %xmm3,%xmm0                        \n"
    "pshufb %xmm4,%xmm1                        \n"
    "pshufb %xmm5,%xmm2                        \n"
    "movq   %xmm0,(%edi)                       \n"
    "movq   %xmm1,0x8(%edi)                    \n"
    "movq   %xmm2,0x10(%edi)                   \n"
    "lea    0x18(%edi),%edi                    \n"
    "sub    $0x18,%ecx                         \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);

extern "C" void ScaleRowDown34_1_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                           uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown34_1_Int_SSSE3)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%ebp                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "movdqa _shuf01,%xmm2                      \n"
    "movdqa _shuf11,%xmm3                      \n"
    "movdqa _shuf21,%xmm4                      \n"
    "movdqa _madd01,%xmm5                      \n"
    "movdqa _madd11,%xmm6                      \n"
    "movdqa _round34,%xmm7                     \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%ebp),%xmm1                  \n"
    "pavgb  %xmm1,%xmm0                        \n"
    "pshufb %xmm2,%xmm0                        \n"
    "pmaddubsw %xmm5,%xmm0                     \n"
    "paddsw %xmm7,%xmm0                        \n"
    "psrlw  $0x2,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movq   %xmm0,(%edi)                       \n"
    "movdqu 0x8(%esi),%xmm0                    \n"
    "movdqu 0x8(%esi,%ebp),%xmm1               \n"
    "pavgb  %xmm1,%xmm0                        \n"
    "pshufb %xmm3,%xmm0                        \n"
    "pmaddubsw %xmm6,%xmm0                     \n"
    "paddsw %xmm7,%xmm0                        \n"
    "psrlw  $0x2,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movq   %xmm0,0x8(%edi)                    \n"
    "movdqa 0x10(%esi),%xmm0                   \n"
    "movdqa 0x10(%esi,%ebp),%xmm1              \n"
    "lea    0x20(%esi),%esi                    \n"
    "pavgb  %xmm1,%xmm0                        \n"
    "pshufb %xmm4,%xmm0                        \n"
    "movdqa  _madd21,%xmm1                     \n"
    "pmaddubsw %xmm1,%xmm0                     \n"
    "paddsw %xmm7,%xmm0                        \n"
    "psrlw  $0x2,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movq   %xmm0,0x10(%edi)                   \n"
    "lea    0x18(%edi),%edi                    \n"
    "sub    $0x18,%ecx                         \n"
    "ja     1b                                 \n"

    "popa                                      \n"
    "ret                                       \n"
);

extern "C" void ScaleRowDown34_0_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                           uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown34_0_Int_SSSE3)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%ebp                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "movdqa _shuf01,%xmm2                      \n"
    "movdqa _shuf11,%xmm3                      \n"
    "movdqa _shuf21,%xmm4                      \n"
    "movdqa _madd01,%xmm5                      \n"
    "movdqa _madd11,%xmm6                      \n"
    "movdqa _round34,%xmm7                     \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%ebp,1),%xmm1                \n"
    "pavgb  %xmm0,%xmm1                        \n"
    "pavgb  %xmm1,%xmm0                        \n"
    "pshufb %xmm2,%xmm0                        \n"
    "pmaddubsw %xmm5,%xmm0                     \n"
    "paddsw %xmm7,%xmm0                        \n"
    "psrlw  $0x2,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movq   %xmm0,(%edi)                       \n"
    "movdqu 0x8(%esi),%xmm0                    \n"
    "movdqu 0x8(%esi,%ebp,1),%xmm1             \n"
    "pavgb  %xmm0,%xmm1                        \n"
    "pavgb  %xmm1,%xmm0                        \n"
    "pshufb %xmm3,%xmm0                        \n"
    "pmaddubsw %xmm6,%xmm0                     \n"
    "paddsw %xmm7,%xmm0                        \n"
    "psrlw  $0x2,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movq   %xmm0,0x8(%edi)                    \n"
    "movdqa 0x10(%esi),%xmm0                   \n"
    "movdqa 0x10(%esi,%ebp,1),%xmm1            \n"
    "lea    0x20(%esi),%esi                    \n"
    "pavgb  %xmm0,%xmm1                        \n"
    "pavgb  %xmm1,%xmm0                        \n"
    "pshufb %xmm4,%xmm0                        \n"
    "movdqa  _madd21,%xmm1                     \n"
    "pmaddubsw %xmm1,%xmm0                     \n"
    "paddsw %xmm7,%xmm0                        \n"
    "psrlw  $0x2,%xmm0                         \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movq   %xmm0,0x10(%edi)                   \n"
    "lea    0x18(%edi),%edi                    \n"
    "sub    $0x18,%ecx                         \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);

#define HAS_SCALEROWDOWN38_SSSE3
extern "C" void ScaleRowDown38_SSSE3(const uint8* src_ptr, int src_stride,
                                     uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown38_SSSE3)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%edx                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "movdqa _shuf38a ,%xmm4                    \n"
    "movdqa _shuf38b ,%xmm5                    \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa 0x10(%esi),%xmm1                   \n"
    "lea    0x20(%esi),%esi                    \n"
    "pshufb %xmm4,%xmm0                        \n"
    "pshufb %xmm5,%xmm1                        \n"
    "paddusb %xmm1,%xmm0                       \n"
    "movq   %xmm0,(%edi)                       \n"
    "movhlps %xmm0,%xmm1                       \n"
    "movd   %xmm1,0x8(%edi)                    \n"
    "lea    0xc(%edi),%edi                     \n"
    "sub    $0xc,%ecx                          \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);

extern "C" void ScaleRowDown38_3_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                           uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown38_3_Int_SSSE3)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%edx                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "movdqa _shufac0,%xmm4                     \n"
    "movdqa _shufac3,%xmm5                     \n"
    "movdqa _scaleac3,%xmm6                    \n"
    "pxor   %xmm7,%xmm7                        \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%edx,1),%xmm2                \n"
    "movhlps %xmm0,%xmm1                       \n"
    "movhlps %xmm2,%xmm3                       \n"
    "punpcklbw %xmm7,%xmm0                     \n"
    "punpcklbw %xmm7,%xmm1                     \n"
    "punpcklbw %xmm7,%xmm2                     \n"
    "punpcklbw %xmm7,%xmm3                     \n"
    "paddusw %xmm2,%xmm0                       \n"
    "paddusw %xmm3,%xmm1                       \n"
    "movdqa (%esi,%edx,2),%xmm2                \n"
    "lea    0x10(%esi),%esi                    \n"
    "movhlps %xmm2,%xmm3                       \n"
    "punpcklbw %xmm7,%xmm2                     \n"
    "punpcklbw %xmm7,%xmm3                     \n"
    "paddusw %xmm2,%xmm0                       \n"
    "paddusw %xmm3,%xmm1                       \n"
    "movdqa %xmm0,%xmm2                        \n"
    "psrldq $0x2,%xmm0                         \n"
    "paddusw %xmm0,%xmm2                       \n"
    "psrldq $0x2,%xmm0                         \n"
    "paddusw %xmm0,%xmm2                       \n"
    "pshufb %xmm4,%xmm2                        \n"
    "movdqa %xmm1,%xmm3                        \n"
    "psrldq $0x2,%xmm1                         \n"
    "paddusw %xmm1,%xmm3                       \n"
    "psrldq $0x2,%xmm1                         \n"
    "paddusw %xmm1,%xmm3                       \n"
    "pshufb %xmm5,%xmm3                        \n"
    "paddusw %xmm3,%xmm2                       \n"
    "pmulhuw %xmm6,%xmm2                       \n"
    "packuswb %xmm2,%xmm2                      \n"
    "movd   %xmm2,(%edi)                       \n"
    "pextrw $0x2,%xmm2,%eax                    \n"
    "mov    %ax,0x4(%edi)                      \n"
    "lea    0x6(%edi),%edi                     \n"
    "sub    $0x6,%ecx                          \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);

extern "C" void ScaleRowDown38_2_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                           uint8* dst_ptr, int dst_width);
  asm(
    DECLARE_FUNCTION(ScaleRowDown38_2_Int_SSSE3)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%edx                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "movdqa _shufab0,%xmm4                     \n"
    "movdqa _shufab1,%xmm5                     \n"
    "movdqa _shufab2,%xmm6                     \n"
    "movdqa _scaleab2,%xmm7                    \n"

"1:"
    "movdqa (%esi),%xmm2                       \n"
    "pavgb  (%esi,%edx,1),%xmm2                \n"
    "lea    0x10(%esi),%esi                    \n"
    "movdqa %xmm2,%xmm0                        \n"
    "pshufb %xmm4,%xmm0                        \n"
    "movdqa %xmm2,%xmm1                        \n"
    "pshufb %xmm5,%xmm1                        \n"
    "paddusw %xmm1,%xmm0                       \n"
    "pshufb %xmm6,%xmm2                        \n"
    "paddusw %xmm2,%xmm0                       \n"
    "pmulhuw %xmm7,%xmm0                       \n"
    "packuswb %xmm0,%xmm0                      \n"
    "movd   %xmm0,(%edi)                       \n"
    "pextrw $0x2,%xmm0,%eax                    \n"
    "mov    %ax,0x4(%edi)                      \n"
    "lea    0x6(%edi),%edi                     \n"
    "sub    $0x6,%ecx                          \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);
#endif // __PIC__

#define HAS_SCALEADDROWS_SSE2
extern "C" void ScaleAddRows_SSE2(const uint8* src_ptr, int src_stride,
                                  uint16* dst_ptr, int src_width,
                                  int src_height);
  asm(
    DECLARE_FUNCTION(ScaleAddRows_SSE2)
    "pusha                                     \n"
    "mov    0x24(%esp),%esi                    \n"
    "mov    0x28(%esp),%edx                    \n"
    "mov    0x2c(%esp),%edi                    \n"
    "mov    0x30(%esp),%ecx                    \n"
    "mov    0x34(%esp),%ebx                    \n"
    "pxor   %xmm5,%xmm5                        \n"

"1:"
    "movdqa (%esi),%xmm2                       \n"
    "lea    (%esi,%edx,1),%eax                 \n"
    "movhlps %xmm2,%xmm3                       \n"
    "lea    -0x1(%ebx),%ebp                    \n"
    "punpcklbw %xmm5,%xmm2                     \n"
    "punpcklbw %xmm5,%xmm3                     \n"

"2:"
    "movdqa (%eax),%xmm0                       \n"
    "lea    (%eax,%edx,1),%eax                 \n"
    "movhlps %xmm0,%xmm1                       \n"
    "punpcklbw %xmm5,%xmm0                     \n"
    "punpcklbw %xmm5,%xmm1                     \n"
    "paddusw %xmm0,%xmm2                       \n"
    "paddusw %xmm1,%xmm3                       \n"
    "sub    $0x1,%ebp                          \n"
    "ja     2b                                 \n"

    "movdqa %xmm2,(%edi)                       \n"
    "movdqa %xmm3,0x10(%edi)                   \n"
    "lea    0x20(%edi),%edi                    \n"
    "lea    0x10(%esi),%esi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     1b                                 \n"
    "popa                                      \n"
    "ret                                       \n"
);

// Bilinear row filtering combines 16x2 -> 16x1. SSE2 version
#define HAS_SCALEFILTERROWS_SSE2
extern "C" void ScaleFilterRows_SSE2(uint8* dst_ptr,
                                     const uint8* src_ptr, int src_stride,
                                     int dst_width, int source_y_fraction);
  asm(
    DECLARE_FUNCTION(ScaleFilterRows_SSE2)
    "push   %esi                               \n"
    "push   %edi                               \n"
    "mov    0xc(%esp),%edi                     \n"
    "mov    0x10(%esp),%esi                    \n"
    "mov    0x14(%esp),%edx                    \n"
    "mov    0x18(%esp),%ecx                    \n"
    "mov    0x1c(%esp),%eax                    \n"
    "cmp    $0x0,%eax                          \n"
    "je     2f                                 \n"
    "cmp    $0x80,%eax                         \n"
    "je     3f                                 \n"
    "movd   %eax,%xmm6                         \n"
    "punpcklwd %xmm6,%xmm6                     \n"
    "pshufd $0x0,%xmm6,%xmm6                   \n"
    "neg    %eax                               \n"
    "add    $0x100,%eax                        \n"
    "movd   %eax,%xmm5                         \n"
    "punpcklwd %xmm5,%xmm5                     \n"
    "pshufd $0x0,%xmm5,%xmm5                   \n"
    "pxor   %xmm7,%xmm7                        \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%edx,1),%xmm2                \n"
    "lea    0x10(%esi),%esi                    \n"
    "movdqa %xmm0,%xmm1                        \n"
    "movdqa %xmm2,%xmm3                        \n"
    "punpcklbw %xmm7,%xmm0                     \n"
    "punpcklbw %xmm7,%xmm2                     \n"
    "punpckhbw %xmm7,%xmm1                     \n"
    "punpckhbw %xmm7,%xmm3                     \n"
    "pmullw %xmm5,%xmm0                        \n"
    "pmullw %xmm5,%xmm1                        \n"
    "pmullw %xmm6,%xmm2                        \n"
    "pmullw %xmm6,%xmm3                        \n"
    "paddusw %xmm2,%xmm0                       \n"
    "paddusw %xmm3,%xmm1                       \n"
    "psrlw  $0x8,%xmm0                         \n"
    "psrlw  $0x8,%xmm1                         \n"
    "packuswb %xmm1,%xmm0                      \n"
    "movdqa %xmm0,(%edi)                       \n"
    "lea    0x10(%edi),%edi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     1b                                 \n"
    "mov    -0x1(%edi),%al                     \n"
    "mov    %al,(%edi)                         \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "ret                                       \n"

"2:"
    "movdqa (%esi),%xmm0                       \n"
    "lea    0x10(%esi),%esi                    \n"
    "movdqa %xmm0,(%edi)                       \n"
    "lea    0x10(%edi),%edi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     2b                                 \n"

    "mov    -0x1(%edi),%al                     \n"
    "mov    %al,(%edi)                         \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "ret                                       \n"

"3:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%edx,1),%xmm2                \n"
    "lea    0x10(%esi),%esi                    \n"
    "pavgb  %xmm2,%xmm0                        \n"
    "movdqa %xmm0,(%edi)                       \n"
    "lea    0x10(%edi),%edi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     3b                                 \n"

    "mov    -0x1(%edi),%al                     \n"
    "mov    %al,(%edi)                         \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "ret                                       \n"
);

// Bilinear row filtering combines 16x2 -> 16x1. SSSE3 version
#define HAS_SCALEFILTERROWS_SSSE3
extern "C" void ScaleFilterRows_SSSE3(uint8* dst_ptr,
                                      const uint8* src_ptr, int src_stride,
                                      int dst_width, int source_y_fraction);
  asm(
    DECLARE_FUNCTION(ScaleFilterRows_SSSE3)
    "push   %esi                               \n"
    "push   %edi                               \n"
    "mov    0xc(%esp),%edi                     \n"
    "mov    0x10(%esp),%esi                    \n"
    "mov    0x14(%esp),%edx                    \n"
    "mov    0x18(%esp),%ecx                    \n"
    "mov    0x1c(%esp),%eax                    \n"
    "cmp    $0x0,%eax                          \n"
    "je     2f                                 \n"
    "cmp    $0x80,%eax                         \n"
    "je     3f                                 \n"
    "shr    %eax                               \n"
    "mov    %al,%ah                            \n"
    "neg    %al                                \n"
    "add    $0x80,%al                          \n"
    "movd   %eax,%xmm5                         \n"
    "punpcklwd %xmm5,%xmm5                     \n"
    "pshufd $0x0,%xmm5,%xmm5                   \n"

"1:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%edx,1),%xmm2                \n"
    "lea    0x10(%esi),%esi                    \n"
    "movdqa %xmm0,%xmm1                        \n"
    "punpcklbw %xmm2,%xmm0                     \n"
    "punpckhbw %xmm2,%xmm1                     \n"
    "pmaddubsw %xmm5,%xmm0                     \n"
    "pmaddubsw %xmm5,%xmm1                     \n"
    "psrlw  $0x7,%xmm0                         \n"
    "psrlw  $0x7,%xmm1                         \n"
    "packuswb %xmm1,%xmm0                      \n"
    "movdqa %xmm0,(%edi)                       \n"
    "lea    0x10(%edi),%edi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     1b                                 \n"
    "mov    -0x1(%edi),%al                     \n"
    "mov    %al,(%edi)                         \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "ret                                       \n"

"2:"
    "movdqa (%esi),%xmm0                       \n"
    "lea    0x10(%esi),%esi                    \n"
    "movdqa %xmm0,(%edi)                       \n"
    "lea    0x10(%edi),%edi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     2b                                 \n"
    "mov    -0x1(%edi),%al                     \n"
    "mov    %al,(%edi)                         \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "ret                                       \n"

"3:"
    "movdqa (%esi),%xmm0                       \n"
    "movdqa (%esi,%edx,1),%xmm2                \n"
    "lea    0x10(%esi),%esi                    \n"
    "pavgb  %xmm2,%xmm0                        \n"
    "movdqa %xmm0,(%edi)                       \n"
    "lea    0x10(%edi),%edi                    \n"
    "sub    $0x10,%ecx                         \n"
    "ja     3b                                 \n"
    "mov    -0x1(%edi),%al                     \n"
    "mov    %al,(%edi)                         \n"
    "pop    %edi                               \n"
    "pop    %esi                               \n"
    "ret                                       \n"
);

#elif defined(__x86_64__)
static void ScaleRowDown8Int_SSE2(const uint8* src_ptr, int src_stride,
                                  uint8* dst_ptr, int dst_width) {
  asm volatile (
  "lea        (%3,%3,2),%%r10                  \n"
  "pxor       %%xmm7,%%xmm7                    \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "movdqa     (%0,%3,1),%%xmm2                 \n"
  "movdqa     0x10(%0,%3,1),%%xmm3             \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "movdqa     (%0,%3,2),%%xmm2                 \n"
  "movdqa     0x10(%0,%3,2),%%xmm3             \n"
  "movdqa     (%0,%%r10,1),%%xmm4              \n"
  "movdqa     0x10(%0,%%r10,1),%%xmm5          \n"
  "lea        (%0,%3,4),%%r11                  \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm4,%%xmm2                    \n"
  "pavgb      %%xmm5,%%xmm3                    \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "movdqa     0x0(%%r11),%%xmm2                \n"
  "movdqa     0x10(%%r11),%%xmm3               \n"
  "movdqa     0x0(%%r11,%3,1),%%xmm4           \n"
  "movdqa     0x10(%%r11,%3,1),%%xmm5          \n"
  "pavgb      %%xmm4,%%xmm2                    \n"
  "pavgb      %%xmm5,%%xmm3                    \n"
  "movdqa     0x0(%%r11,%3,2),%%xmm4           \n"
  "movdqa     0x10(%%r11,%3,2),%%xmm5          \n"
  "movdqa     0x0(%%r11,%%r10,1),%%xmm6        \n"
  "pavgb      %%xmm6,%%xmm4                    \n"
  "movdqa     0x10(%%r11,%%r10,1),%%xmm6       \n"
  "pavgb      %%xmm6,%%xmm5                    \n"
  "pavgb      %%xmm4,%%xmm2                    \n"
  "pavgb      %%xmm5,%%xmm3                    \n"
  "pavgb      %%xmm2,%%xmm0                    \n"
  "pavgb      %%xmm3,%%xmm1                    \n"
  "psadbw     %%xmm7,%%xmm0                    \n"
  "psadbw     %%xmm7,%%xmm1                    \n"
  "pshufd     $0xd8,%%xmm0,%%xmm0              \n"
  "pshufd     $0x8d,%%xmm1,%%xmm1              \n"
  "por        %%xmm1,%%xmm0                    \n"
  "psrlw      $0x3,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movd       %%xmm0,(%1)                      \n"
  "lea        0x4(%1),%1                       \n"
  "sub        $0x4,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(static_cast<intptr_t>(src_stride))   // %3
  : "memory", "cc", "r10", "r11", "xmm6", "xmm7"
);
}

#define HAS_SCALEROWDOWN34_SSSE3
static void ScaleRowDown34_SSSE3(const uint8* src_ptr, int src_stride,
                                 uint8* dst_ptr, int dst_width) {
  asm volatile (
  "movdqa     (%3),%%xmm3                      \n"
  "movdqa     (%4),%%xmm4                      \n"
  "movdqa     (%5),%%xmm5                      \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm2                  \n"
  "lea        0x20(%0),%0                      \n"
  "movdqa     %%xmm2,%%xmm1                    \n"
  "palignr    $0x8,%%xmm0,%%xmm1               \n"
  "pshufb     %%xmm3,%%xmm0                    \n"
  "pshufb     %%xmm4,%%xmm1                    \n"
  "pshufb     %%xmm5,%%xmm2                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movq       %%xmm1,0x8(%1)                   \n"
  "movq       %%xmm2,0x10(%1)                  \n"
  "lea        0x18(%1),%1                      \n"
  "sub        $0x18,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(_shuf0),   // %3
    "r"(_shuf1),   // %4
    "r"(_shuf2)    // %5
  : "memory", "cc"
);
}

static void ScaleRowDown34_1_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  asm volatile (
  "movdqa     (%4),%%xmm2                      \n"  // _shuf01
  "movdqa     (%5),%%xmm3                      \n"  // _shuf11
  "movdqa     (%6),%%xmm4                      \n"  // _shuf21
  "movdqa     (%7),%%xmm5                      \n"  // _madd01
  "movdqa     (%8),%%xmm6                      \n"  // _madd11
  "movdqa     (%9),%%xmm7                      \n"  // _round34
  "movdqa     (%10),%%xmm8                     \n"  // _madd21
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     (%0,%3),%%xmm1                   \n"
  "pavgb      %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm2,%%xmm0                    \n"
  "pmaddubsw  %%xmm5,%%xmm0                    \n"
  "paddsw     %%xmm7,%%xmm0                    \n"
  "psrlw      $0x2,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movdqu     0x8(%0),%%xmm0                   \n"
  "movdqu     0x8(%0,%3),%%xmm1                \n"
  "pavgb      %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm3,%%xmm0                    \n"
  "pmaddubsw  %%xmm6,%%xmm0                    \n"
  "paddsw     %%xmm7,%%xmm0                    \n"
  "psrlw      $0x2,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,0x8(%1)                   \n"
  "movdqa     0x10(%0),%%xmm0                  \n"
  "movdqa     0x10(%0,%3),%%xmm1               \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm4,%%xmm0                    \n"
  "pmaddubsw  %%xmm8,%%xmm0                    \n"
  "paddsw     %%xmm7,%%xmm0                    \n"
  "psrlw      $0x2,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,0x10(%1)                  \n"
  "lea        0x18(%1),%1                      \n"
  "sub        $0x18,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(static_cast<intptr_t>(src_stride)),  // %3
    "r"(_shuf01),   // %4
    "r"(_shuf11),   // %5
    "r"(_shuf21),   // %6
    "r"(_madd01),   // %7
    "r"(_madd11),   // %8
    "r"(_round34),  // %9
    "r"(_madd21)    // %10
  : "memory", "cc", "xmm6", "xmm7", "xmm8"
);
}

static void ScaleRowDown34_0_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  asm volatile (
  "movdqa     (%4),%%xmm2                      \n"  // _shuf01
  "movdqa     (%5),%%xmm3                      \n"  // _shuf11
  "movdqa     (%6),%%xmm4                      \n"  // _shuf21
  "movdqa     (%7),%%xmm5                      \n"  // _madd01
  "movdqa     (%8),%%xmm6                      \n"  // _madd11
  "movdqa     (%9),%%xmm7                      \n"  // _round34
  "movdqa     (%10),%%xmm8                     \n"  // _madd21
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     (%0,%3,1),%%xmm1                 \n"
  "pavgb      %%xmm0,%%xmm1                    \n"
  "pavgb      %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm2,%%xmm0                    \n"
  "pmaddubsw  %%xmm5,%%xmm0                    \n"
  "paddsw     %%xmm7,%%xmm0                    \n"
  "psrlw      $0x2,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movdqu     0x8(%0),%%xmm0                   \n"
  "movdqu     0x8(%0,%3,1),%%xmm1              \n"
  "pavgb      %%xmm0,%%xmm1                    \n"
  "pavgb      %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm3,%%xmm0                    \n"
  "pmaddubsw  %%xmm6,%%xmm0                    \n"
  "paddsw     %%xmm7,%%xmm0                    \n"
  "psrlw      $0x2,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,0x8(%1)                   \n"
  "movdqa     0x10(%0),%%xmm0                  \n"
  "movdqa     0x10(%0,%3,1),%%xmm1             \n"
  "lea        0x20(%0),%0                      \n"
  "pavgb      %%xmm0,%%xmm1                    \n"
  "pavgb      %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm4,%%xmm0                    \n"
  "pmaddubsw  %%xmm8,%%xmm0                    \n"
  "paddsw     %%xmm7,%%xmm0                    \n"
  "psrlw      $0x2,%%xmm0                      \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movq       %%xmm0,0x10(%1)                  \n"
  "lea        0x18(%1),%1                      \n"
  "sub        $0x18,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(static_cast<intptr_t>(src_stride)),  // %3
    "r"(_shuf01),   // %4
    "r"(_shuf11),   // %5
    "r"(_shuf21),   // %6
    "r"(_madd01),   // %7
    "r"(_madd11),   // %8
    "r"(_round34),  // %9
    "r"(_madd21)    // %10
  : "memory", "cc", "xmm6", "xmm7", "xmm8"
);
}

#define HAS_SCALEROWDOWN38_SSSE3
static void ScaleRowDown38_SSSE3(const uint8* src_ptr, int src_stride,
                                 uint8* dst_ptr, int dst_width) {
  asm volatile (
  "movdqa     (%3),%%xmm4                      \n"
  "movdqa     (%4),%%xmm5                      \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     0x10(%0),%%xmm1                  \n"
  "lea        0x20(%0),%0                      \n"
  "pshufb     %%xmm4,%%xmm0                    \n"
  "pshufb     %%xmm5,%%xmm1                    \n"
  "paddusb    %%xmm1,%%xmm0                    \n"
  "movq       %%xmm0,(%1)                      \n"
  "movhlps    %%xmm0,%%xmm1                    \n"
  "movd       %%xmm1,0x8(%1)                   \n"
  "lea        0xc(%1),%1                       \n"
  "sub        $0xc,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(_shuf38a),  // %3
    "r"(_shuf38b)   // %4
  : "memory", "cc"
);
}

static void ScaleRowDown38_3_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  asm volatile (
  "movdqa     (%4),%%xmm4                      \n"
  "movdqa     (%5),%%xmm5                      \n"
  "movdqa     (%6),%%xmm6                      \n"
  "pxor       %%xmm7,%%xmm7                    \n"
"1:"
  "movdqa     (%0),%%xmm0                      \n"
  "movdqa     (%0,%3,1),%%xmm2                 \n"
  "movhlps    %%xmm0,%%xmm1                    \n"
  "movhlps    %%xmm2,%%xmm3                    \n"
  "punpcklbw  %%xmm7,%%xmm0                    \n"
  "punpcklbw  %%xmm7,%%xmm1                    \n"
  "punpcklbw  %%xmm7,%%xmm2                    \n"
  "punpcklbw  %%xmm7,%%xmm3                    \n"
  "paddusw    %%xmm2,%%xmm0                    \n"
  "paddusw    %%xmm3,%%xmm1                    \n"
  "movdqa     (%0,%3,2),%%xmm2                 \n"
  "lea        0x10(%0),%0                      \n"
  "movhlps    %%xmm2,%%xmm3                    \n"
  "punpcklbw  %%xmm7,%%xmm2                    \n"
  "punpcklbw  %%xmm7,%%xmm3                    \n"
  "paddusw    %%xmm2,%%xmm0                    \n"
  "paddusw    %%xmm3,%%xmm1                    \n"
  "movdqa     %%xmm0,%%xmm2                    \n"
  "psrldq     $0x2,%%xmm0                      \n"
  "paddusw    %%xmm0,%%xmm2                    \n"
  "psrldq     $0x2,%%xmm0                      \n"
  "paddusw    %%xmm0,%%xmm2                    \n"
  "pshufb     %%xmm4,%%xmm2                    \n"
  "movdqa     %%xmm1,%%xmm3                    \n"
  "psrldq     $0x2,%%xmm1                      \n"
  "paddusw    %%xmm1,%%xmm3                    \n"
  "psrldq     $0x2,%%xmm1                      \n"
  "paddusw    %%xmm1,%%xmm3                    \n"
  "pshufb     %%xmm5,%%xmm3                    \n"
  "paddusw    %%xmm3,%%xmm2                    \n"
  "pmulhuw    %%xmm6,%%xmm2                    \n"
  "packuswb   %%xmm2,%%xmm2                    \n"
  "movd       %%xmm2,(%1)                      \n"
  "pextrw     $0x2,%%xmm2,%%eax                \n"
  "mov        %%ax,0x4(%1)                     \n"
  "lea        0x6(%1),%1                       \n"
  "sub        $0x6,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(static_cast<intptr_t>(src_stride)),  // %3
    "r"(_shufac0),   // %4
    "r"(_shufac3),   // %5
    "r"(_scaleac3)   // %6
  : "memory", "cc", "rax", "xmm6", "xmm7"
);
}

static void ScaleRowDown38_2_Int_SSSE3(const uint8* src_ptr, int src_stride,
                                       uint8* dst_ptr, int dst_width) {
  asm volatile (
  "movdqa     (%4),%%xmm4                      \n"
  "movdqa     (%5),%%xmm5                      \n"
  "movdqa     (%6),%%xmm6                      \n"
  "movdqa     (%7),%%xmm7                      \n"
"1:"
  "movdqa     (%0),%%xmm2                      \n"
  "pavgb      (%0,%3,1),%%xmm2                 \n"
  "lea        0x10(%0),%0                      \n"
  "movdqa     %%xmm2,%%xmm0                    \n"
  "pshufb     %%xmm4,%%xmm0                    \n"
  "movdqa     %%xmm2,%%xmm1                    \n"
  "pshufb     %%xmm5,%%xmm1                    \n"
  "paddusw    %%xmm1,%%xmm0                    \n"
  "pshufb     %%xmm6,%%xmm2                    \n"
  "paddusw    %%xmm2,%%xmm0                    \n"
  "pmulhuw    %%xmm7,%%xmm0                    \n"
  "packuswb   %%xmm0,%%xmm0                    \n"
  "movd       %%xmm0,(%1)                      \n"
  "pextrw     $0x2,%%xmm0,%%eax                \n"
  "mov        %%ax,0x4(%1)                     \n"
  "lea        0x6(%1),%1                       \n"
  "sub        $0x6,%2                          \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(dst_width)    // %2
  : "r"(static_cast<intptr_t>(src_stride)),  // %3
    "r"(_shufab0),   // %4
    "r"(_shufab1),   // %5
    "r"(_shufab2),   // %6
    "r"(_scaleab2)   // %7
  : "memory", "cc", "rax", "xmm6", "xmm7"
);
}

#define HAS_SCALEADDROWS_SSE2
static void ScaleAddRows_SSE2(const uint8* src_ptr, int src_stride,
                              uint16* dst_ptr, int src_width,
                              int src_height) {
  asm volatile (
  "pxor       %%xmm5,%%xmm5                    \n"
"1:"
  "movdqa     (%0),%%xmm2                      \n"
  "lea        (%0,%4,1),%%r10                  \n"
  "movhlps    %%xmm2,%%xmm3                    \n"
  "lea        -0x1(%3),%%r11                   \n"
  "punpcklbw  %%xmm5,%%xmm2                    \n"
  "punpcklbw  %%xmm5,%%xmm3                    \n"

"2:"
  "movdqa     (%%r10),%%xmm0                   \n"
  "lea        (%%r10,%4,1),%%r10               \n"
  "movhlps    %%xmm0,%%xmm1                    \n"
  "punpcklbw  %%xmm5,%%xmm0                    \n"
  "punpcklbw  %%xmm5,%%xmm1                    \n"
  "paddusw    %%xmm0,%%xmm2                    \n"
  "paddusw    %%xmm1,%%xmm3                    \n"
  "sub        $0x1,%%r11                       \n"
  "ja         2b                               \n"

  "movdqa     %%xmm2,(%1)                      \n"
  "movdqa     %%xmm3,0x10(%1)                  \n"
  "lea        0x20(%1),%1                      \n"
  "lea        0x10(%0),%0                      \n"
  "sub        $0x10,%2                         \n"
  "ja         1b                               \n"
  : "+r"(src_ptr),     // %0
    "+r"(dst_ptr),     // %1
    "+r"(src_width),   // %2
    "+r"(src_height)   // %3
  : "r"(static_cast<intptr_t>(src_stride))  // %4
  : "memory", "cc", "r10", "r11"
);
}

// Bilinear row filtering combines 16x2 -> 16x1. SSE2 version
#define HAS_SCALEFILTERROWS_SSE2
static void ScaleFilterRows_SSE2(uint8* dst_ptr,
                                 const uint8* src_ptr, int src_stride,
                                 int dst_width, int source_y_fraction) {
  if (source_y_fraction == 0) {
    asm volatile (
    "1:"
      "movdqa     (%1),%%xmm0                  \n"
      "lea        0x10(%1),%1                  \n"
      "movdqa     %%xmm0,(%0)                  \n"
      "lea        0x10(%0),%0                  \n"
      "sub        $0x10,%2                     \n"
      "ja         1b                           \n"
      "mov        -0x1(%0),%%al                \n"
      "mov        %%al,(%0)                    \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(dst_width)    // %2
      :
      : "memory", "cc", "rax"
    );
    return;
  } else if (source_y_fraction == 128) {
    asm volatile (
    "1:"
      "movdqa     (%1),%%xmm0                  \n"
      "movdqa     (%1,%3,1),%%xmm2             \n"
      "lea        0x10(%1),%1                  \n"
      "pavgb      %%xmm2,%%xmm0                \n"
      "movdqa     %%xmm0,(%0)                  \n"
      "lea        0x10(%0),%0                  \n"
      "sub        $0x10,%2                     \n"
      "ja         1b                           \n"
      "mov        -0x1(%0),%%al                \n"
      "mov        %%al,(%0)                    \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(dst_width)    // %2
      : "r"(static_cast<intptr_t>(src_stride))  // %3
      : "memory", "cc", "rax"
    );
    return;
  } else {
    asm volatile (
      "mov        %3,%%eax                     \n"
      "movd       %%eax,%%xmm6                 \n"
      "punpcklwd  %%xmm6,%%xmm6                \n"
      "pshufd     $0x0,%%xmm6,%%xmm6           \n"
      "neg        %%eax                        \n"
      "add        $0x100,%%eax                 \n"
      "movd       %%eax,%%xmm5                 \n"
      "punpcklwd  %%xmm5,%%xmm5                \n"
      "pshufd     $0x0,%%xmm5,%%xmm5           \n"
      "pxor       %%xmm7,%%xmm7                \n"
    "1:"
      "movdqa     (%1),%%xmm0                  \n"
      "movdqa     (%1,%4,1),%%xmm2             \n"
      "lea        0x10(%1),%1                  \n"
      "movdqa     %%xmm0,%%xmm1                \n"
      "movdqa     %%xmm2,%%xmm3                \n"
      "punpcklbw  %%xmm7,%%xmm0                \n"
      "punpcklbw  %%xmm7,%%xmm2                \n"
      "punpckhbw  %%xmm7,%%xmm1                \n"
      "punpckhbw  %%xmm7,%%xmm3                \n"
      "pmullw     %%xmm5,%%xmm0                \n"
      "pmullw     %%xmm5,%%xmm1                \n"
      "pmullw     %%xmm6,%%xmm2                \n"
      "pmullw     %%xmm6,%%xmm3                \n"
      "paddusw    %%xmm2,%%xmm0                \n"
      "paddusw    %%xmm3,%%xmm1                \n"
      "psrlw      $0x8,%%xmm0                  \n"
      "psrlw      $0x8,%%xmm1                  \n"
      "packuswb   %%xmm1,%%xmm0                \n"
      "movdqa     %%xmm0,(%0)                  \n"
      "lea        0x10(%0),%0                  \n"
      "sub        $0x10,%2                     \n"
      "ja         1b                           \n"
      "mov        -0x1(%0),%%al                \n"
      "mov        %%al,(%0)                    \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(dst_width),   // %2
        "+r"(source_y_fraction)  // %3
      : "r"(static_cast<intptr_t>(src_stride))  // %4
      : "memory", "cc", "rax", "xmm6", "xmm7"
    );
  }
  return;
}

// Bilinear row filtering combines 16x2 -> 16x1. SSSE3 version
#define HAS_SCALEFILTERROWS_SSSE3
static void ScaleFilterRows_SSSE3(uint8* dst_ptr,
                                  const uint8* src_ptr, int src_stride,
                                  int dst_width, int source_y_fraction) {
  if (source_y_fraction == 0) {
    asm volatile (
   "1:"
      "movdqa     (%1),%%xmm0                  \n"
      "lea        0x10(%1),%1                  \n"
      "movdqa     %%xmm0,(%0)                  \n"
      "lea        0x10(%0),%0                  \n"
      "sub        $0x10,%2                     \n"
      "ja         1b                           \n"
      "mov        -0x1(%0),%%al                \n"
      "mov        %%al,(%0)                    \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(dst_width)    // %2
      :
      : "memory", "cc", "rax"
    );
    return;
  } else if (source_y_fraction == 128) {
    asm volatile (
    "1:"
      "movdqa     (%1),%%xmm0                  \n"
      "movdqa     (%1,%3,1),%%xmm2             \n"
      "lea        0x10(%1),%1                  \n"
      "pavgb      %%xmm2,%%xmm0                \n"
      "movdqa     %%xmm0,(%0)                  \n"
      "lea        0x10(%0),%0                  \n"
      "sub        $0x10,%2                     \n"
      "ja         1b                           \n"
      "mov        -0x1(%0),%%al                \n"
      "mov        %%al,(%0)                    \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(dst_width)    // %2
      : "r"(static_cast<intptr_t>(src_stride))  // %3
     : "memory", "cc", "rax"
    );
    return;
  } else {
    asm volatile (
      "mov        %3,%%eax                     \n"
      "shr        %%eax                        \n"
      "mov        %%al,%%ah                    \n"
      "neg        %%al                         \n"
      "add        $0x80,%%al                   \n"
      "movd       %%eax,%%xmm5                 \n"
      "punpcklwd  %%xmm5,%%xmm5                \n"
      "pshufd     $0x0,%%xmm5,%%xmm5           \n"
    "1:"
      "movdqa     (%1),%%xmm0                  \n"
      "movdqa     (%1,%4,1),%%xmm2             \n"
      "lea        0x10(%1),%1                  \n"
      "movdqa     %%xmm0,%%xmm1                \n"
      "punpcklbw  %%xmm2,%%xmm0                \n"
      "punpckhbw  %%xmm2,%%xmm1                \n"
      "pmaddubsw  %%xmm5,%%xmm0                \n"
      "pmaddubsw  %%xmm5,%%xmm1                \n"
      "psrlw      $0x7,%%xmm0                  \n"
      "psrlw      $0x7,%%xmm1                  \n"
      "packuswb   %%xmm1,%%xmm0                \n"
      "movdqa     %%xmm0,(%0)                  \n"
      "lea        0x10(%0),%0                  \n"
      "sub        $0x10,%2                     \n"
      "ja         1b                           \n"
      "mov        -0x1(%0),%%al                \n"
      "mov        %%al,(%0)                    \n"
      : "+r"(dst_ptr),     // %0
        "+r"(src_ptr),     // %1
        "+r"(dst_width),   // %2
        "+r"(source_y_fraction)  // %3
      : "r"(static_cast<intptr_t>(src_stride))  // %4
      : "memory", "cc", "rax"
    );
  }
  return;
}
#endif
#endif

// CPU agnostic row functions
static void ScaleRowDown2_C(const uint8* src_ptr, int,
                            uint8* dst, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    *dst++ = *src_ptr;
    src_ptr += 2;
  }
}

void ScaleRowDown2Int_C(const uint8* src_ptr, int src_stride,
                        uint8* dst, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    *dst++ = (src_ptr[0] + src_ptr[1] +
              src_ptr[src_stride] + src_ptr[src_stride + 1] + 2) >> 2;
    src_ptr += 2;
  }
}

static void ScaleRowDown4_C(const uint8* src_ptr, int,
                            uint8* dst, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    *dst++ = *src_ptr;
    src_ptr += 4;
  }
}

static void ScaleRowDown4Int_C(const uint8* src_ptr, int src_stride,
                               uint8* dst, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    *dst++ = (src_ptr[0] + src_ptr[1] + src_ptr[2] + src_ptr[3] +
              src_ptr[src_stride + 0] + src_ptr[src_stride + 1] +
              src_ptr[src_stride + 2] + src_ptr[src_stride + 3] +
              src_ptr[src_stride * 2 + 0] + src_ptr[src_stride * 2 + 1] +
              src_ptr[src_stride * 2 + 2] + src_ptr[src_stride * 2 + 3] +
              src_ptr[src_stride * 3 + 0] + src_ptr[src_stride * 3 + 1] +
              src_ptr[src_stride * 3 + 2] + src_ptr[src_stride * 3 + 3] +
              8) >> 4;
    src_ptr += 4;
  }
}

// 640 output pixels is enough to allow 5120 input pixels with 1/8 scale down.
// Keeping the total buffer under 4096 bytes avoids a stackcheck, saving 4% cpu.
static const int kMaxOutputWidth = 640;
static const int kMaxRow12 = kMaxOutputWidth * 2;

static void ScaleRowDown8_C(const uint8* src_ptr, int,
                            uint8* dst, int dst_width) {
  for (int x = 0; x < dst_width; ++x) {
    *dst++ = *src_ptr;
    src_ptr += 8;
  }
}

// Note calling code checks width is less than max and if not
// uses ScaleRowDown8_C instead.
static void ScaleRowDown8Int_C(const uint8* src_ptr, int src_stride,
                               uint8* dst, int dst_width) {
  ALIGN16(uint8 src_row[kMaxRow12 * 2]);
  assert(dst_width <= kMaxOutputWidth);
  ScaleRowDown4Int_C(src_ptr, src_stride, src_row, dst_width * 2);
  ScaleRowDown4Int_C(src_ptr + src_stride * 4, src_stride,
                     src_row + kMaxOutputWidth,
                     dst_width * 2);
  ScaleRowDown2Int_C(src_row, kMaxOutputWidth, dst, dst_width);
}

static void ScaleRowDown34_C(const uint8* src_ptr, int,
                             uint8* dst, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  uint8* dend = dst + dst_width;
  do {
    dst[0] = src_ptr[0];
    dst[1] = src_ptr[1];
    dst[2] = src_ptr[3];
    dst += 3;
    src_ptr += 4;
  } while (dst < dend);
}

// Filter rows 0 and 1 together, 3 : 1
static void ScaleRowDown34_0_Int_C(const uint8* src_ptr, int src_stride,
                                   uint8* d, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  uint8* dend = d + dst_width;
  const uint8* s = src_ptr;
  const uint8* t = src_ptr + src_stride;
  do {
    uint8 a0 = (s[0] * 3 + s[1] * 1 + 2) >> 2;
    uint8 a1 = (s[1] * 1 + s[2] * 1 + 1) >> 1;
    uint8 a2 = (s[2] * 1 + s[3] * 3 + 2) >> 2;
    uint8 b0 = (t[0] * 3 + t[1] * 1 + 2) >> 2;
    uint8 b1 = (t[1] * 1 + t[2] * 1 + 1) >> 1;
    uint8 b2 = (t[2] * 1 + t[3] * 3 + 2) >> 2;
    d[0] = (a0 * 3 + b0 + 2) >> 2;
    d[1] = (a1 * 3 + b1 + 2) >> 2;
    d[2] = (a2 * 3 + b2 + 2) >> 2;
    d += 3;
    s += 4;
    t += 4;
  } while (d < dend);
}

// Filter rows 1 and 2 together, 1 : 1
static void ScaleRowDown34_1_Int_C(const uint8* src_ptr, int src_stride,
                                   uint8* d, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  uint8* dend = d + dst_width;
  const uint8* s = src_ptr;
  const uint8* t = src_ptr + src_stride;
  do {
    uint8 a0 = (s[0] * 3 + s[1] * 1 + 2) >> 2;
    uint8 a1 = (s[1] * 1 + s[2] * 1 + 1) >> 1;
    uint8 a2 = (s[2] * 1 + s[3] * 3 + 2) >> 2;
    uint8 b0 = (t[0] * 3 + t[1] * 1 + 2) >> 2;
    uint8 b1 = (t[1] * 1 + t[2] * 1 + 1) >> 1;
    uint8 b2 = (t[2] * 1 + t[3] * 3 + 2) >> 2;
    d[0] = (a0 + b0 + 1) >> 1;
    d[1] = (a1 + b1 + 1) >> 1;
    d[2] = (a2 + b2 + 1) >> 1;
    d += 3;
    s += 4;
    t += 4;
  } while (d < dend);
}

#if defined(HAS_SCALEFILTERROWS_SSE2)
// Filter row to 3/4
static void ScaleFilterCols34_C(uint8* dst_ptr, const uint8* src_ptr,
                                int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  uint8* dend = dst_ptr + dst_width;
  const uint8* s = src_ptr;
  do {
    dst_ptr[0] = (s[0] * 3 + s[1] * 1 + 2) >> 2;
    dst_ptr[1] = (s[1] * 1 + s[2] * 1 + 1) >> 1;
    dst_ptr[2] = (s[2] * 1 + s[3] * 3 + 2) >> 2;
    dst_ptr += 3;
    s += 4;
  } while (dst_ptr < dend);
}
#endif

static void ScaleFilterCols_C(uint8* dst_ptr, const uint8* src_ptr,
                              int dst_width, int dx) {
  int x = 0;
  for (int j = 0; j < dst_width; ++j) {
    int xi = x >> 16;
    int xf1 = x & 0xffff;
    int xf0 = 65536 - xf1;

    *dst_ptr++ = (src_ptr[xi] * xf0 + src_ptr[xi + 1] * xf1) >> 16;
    x += dx;
  }
}

static const int kMaxInputWidth = 2560;
#if defined(HAS_SCALEFILTERROWS_SSE2)
#define HAS_SCALEROWDOWN34_SSE2
// Filter rows 0 and 1 together, 3 : 1
static void ScaleRowDown34_0_Int_SSE2(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  ALIGN16(uint8 row[kMaxInputWidth]);
  ScaleFilterRows_SSE2(row, src_ptr, src_stride, dst_width * 4 / 3, 256 / 4);
  ScaleFilterCols34_C(dst_ptr, row, dst_width);
}

// Filter rows 1 and 2 together, 1 : 1
static void ScaleRowDown34_1_Int_SSE2(const uint8* src_ptr, int src_stride,
                                      uint8* dst_ptr, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  ALIGN16(uint8 row[kMaxInputWidth]);
  ScaleFilterRows_SSE2(row, src_ptr, src_stride, dst_width * 4 / 3, 256 / 2);
  ScaleFilterCols34_C(dst_ptr, row, dst_width);
}
#endif

static void ScaleRowDown38_C(const uint8* src_ptr, int,
                             uint8* dst, int dst_width) {
  assert(dst_width % 3 == 0);
  for (int x = 0; x < dst_width; x += 3) {
    dst[0] = src_ptr[0];
    dst[1] = src_ptr[3];
    dst[2] = src_ptr[6];
    dst += 3;
    src_ptr += 8;
  }
}

// 8x3 -> 3x1
static void ScaleRowDown38_3_Int_C(const uint8* src_ptr, int src_stride,
                                   uint8* dst_ptr, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  for (int i = 0; i < dst_width; i+=3) {
    dst_ptr[0] = (src_ptr[0] + src_ptr[1] + src_ptr[2] +
        src_ptr[src_stride + 0] + src_ptr[src_stride + 1] +
        src_ptr[src_stride + 2] + src_ptr[src_stride * 2 + 0] +
        src_ptr[src_stride * 2 + 1] + src_ptr[src_stride * 2 + 2]) *
        (65536 / 9) >> 16;
    dst_ptr[1] = (src_ptr[3] + src_ptr[4] + src_ptr[5] +
        src_ptr[src_stride + 3] + src_ptr[src_stride + 4] +
        src_ptr[src_stride + 5] + src_ptr[src_stride * 2 + 3] +
        src_ptr[src_stride * 2 + 4] + src_ptr[src_stride * 2 + 5]) *
        (65536 / 9) >> 16;
    dst_ptr[2] = (src_ptr[6] + src_ptr[7] +
        src_ptr[src_stride + 6] + src_ptr[src_stride + 7] +
        src_ptr[src_stride * 2 + 6] + src_ptr[src_stride * 2 + 7]) *
        (65536 / 6) >> 16;
    src_ptr += 8;
    dst_ptr += 3;
  }
}

// 8x2 -> 3x1
static void ScaleRowDown38_2_Int_C(const uint8* src_ptr, int src_stride,
                                   uint8* dst_ptr, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  for (int i = 0; i < dst_width; i+=3) {
    dst_ptr[0] = (src_ptr[0] + src_ptr[1] + src_ptr[2] +
        src_ptr[src_stride + 0] + src_ptr[src_stride + 1] +
        src_ptr[src_stride + 2]) * (65536 / 6) >> 16;
    dst_ptr[1] = (src_ptr[3] + src_ptr[4] + src_ptr[5] +
        src_ptr[src_stride + 3] + src_ptr[src_stride + 4] +
        src_ptr[src_stride + 5]) * (65536 / 6) >> 16;
    dst_ptr[2] = (src_ptr[6] + src_ptr[7] +
        src_ptr[src_stride + 6] + src_ptr[src_stride + 7]) *
        (65536 / 4) >> 16;
    src_ptr += 8;
    dst_ptr += 3;
  }
}

// C version 8x2 -> 8x1
static void ScaleFilterRows_C(uint8* dst_ptr,
                              const uint8* src_ptr, int src_stride,
                              int dst_width, int source_y_fraction) {
  assert(dst_width > 0);
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const uint8* src_ptr1 = src_ptr + src_stride;
  uint8* end = dst_ptr + dst_width;
  do {
    dst_ptr[0] = (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction) >> 8;
    dst_ptr[1] = (src_ptr[1] * y0_fraction + src_ptr1[1] * y1_fraction) >> 8;
    dst_ptr[2] = (src_ptr[2] * y0_fraction + src_ptr1[2] * y1_fraction) >> 8;
    dst_ptr[3] = (src_ptr[3] * y0_fraction + src_ptr1[3] * y1_fraction) >> 8;
    dst_ptr[4] = (src_ptr[4] * y0_fraction + src_ptr1[4] * y1_fraction) >> 8;
    dst_ptr[5] = (src_ptr[5] * y0_fraction + src_ptr1[5] * y1_fraction) >> 8;
    dst_ptr[6] = (src_ptr[6] * y0_fraction + src_ptr1[6] * y1_fraction) >> 8;
    dst_ptr[7] = (src_ptr[7] * y0_fraction + src_ptr1[7] * y1_fraction) >> 8;
    src_ptr += 8;
    src_ptr1 += 8;
    dst_ptr += 8;
  } while (dst_ptr < end);
  dst_ptr[0] = dst_ptr[-1];
}

void ScaleAddRows_C(const uint8* src_ptr, int src_stride,
                    uint16* dst_ptr, int src_width, int src_height) {
  assert(src_width > 0);
  assert(src_height > 0);
  for (int x = 0; x < src_width; ++x) {
    const uint8* s = src_ptr + x;
    int sum = 0;
    for (int y = 0; y < src_height; ++y) {
      sum += s[0];
      s += src_stride;
    }
    dst_ptr[x] = sum;
  }
}

/**
 * Scale plane, 1/2
 *
 * This is an optimized version for scaling down a plane to 1/2 of
 * its original size.
 *
 */
static void ScalePlaneDown2(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const uint8* src_ptr, uint8* dst_ptr,
                            FilterMode filtering) {
  assert(IS_ALIGNED(src_width, 2));
  assert(IS_ALIGNED(src_height, 2));
  void (*ScaleRowDown2)(const uint8* src_ptr, int src_stride,
                        uint8* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(dst_width, 16)) {
    ScaleRowDown2 = filtering ? ScaleRowDown2Int_NEON : ScaleRowDown2_NEON;
  } else
#endif
#if defined(HAS_SCALEROWDOWN2_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(dst_width, 16) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_ptr, 16) && IS_ALIGNED(dst_stride, 16)) {
    ScaleRowDown2 = filtering ? ScaleRowDown2Int_SSE2 : ScaleRowDown2_SSE2;
  } else
#endif
  {
    ScaleRowDown2 = filtering ? ScaleRowDown2Int_C : ScaleRowDown2_C;
  }
  for (int y = 0; y < dst_height; ++y) {
    ScaleRowDown2(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 1);
    dst_ptr += dst_stride;
  }
}

/**
 * Scale plane, 1/4
 *
 * This is an optimized version for scaling down a plane to 1/4 of
 * its original size.
 */
static void ScalePlaneDown4(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const uint8* src_ptr, uint8* dst_ptr,
                            FilterMode filtering) {
  assert(IS_ALIGNED(src_width, 4));
  assert(IS_ALIGNED(src_height, 4));
  void (*ScaleRowDown4)(const uint8* src_ptr, int src_stride,
                        uint8* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN4_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      IS_ALIGNED(dst_width, 4)) {
    ScaleRowDown4 = filtering ? ScaleRowDown4Int_NEON : ScaleRowDown4_NEON;
  } else
#endif
#if defined(HAS_SCALEROWDOWN4_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(dst_width, 8) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_ptr, 8) && IS_ALIGNED(dst_stride, 8)) {
    ScaleRowDown4 = filtering ? ScaleRowDown4Int_SSE2 : ScaleRowDown4_SSE2;
  } else
#endif
  {
    ScaleRowDown4 = filtering ? ScaleRowDown4Int_C : ScaleRowDown4_C;
  }
  for (int y = 0; y < dst_height; ++y) {
    ScaleRowDown4(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 2);
    dst_ptr += dst_stride;
  }
}

/**
 * Scale plane, 1/8
 *
 * This is an optimized version for scaling down a plane to 1/8
 * of its original size.
 *
 */
static void ScalePlaneDown8(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const uint8* src_ptr, uint8* dst_ptr,
                            FilterMode filtering) {
  assert(IS_ALIGNED(src_width, 8));
  assert(IS_ALIGNED(src_height, 8));
  void (*ScaleRowDown8)(const uint8* src_ptr, int src_stride,
                        uint8* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN8_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      IS_ALIGNED(dst_width, 4) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_ptr, 4) && IS_ALIGNED(dst_stride, 4)) {
    ScaleRowDown8 = filtering ? ScaleRowDown8Int_SSE2 : ScaleRowDown8_SSE2;
  } else
#endif
  {
    ScaleRowDown8 = filtering && (dst_width <= kMaxOutputWidth) ?
        ScaleRowDown8Int_C : ScaleRowDown8_C;
  }
  for (int y = 0; y < dst_height; ++y) {
    ScaleRowDown8(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 3);
    dst_ptr += dst_stride;
  }
}

/**
 * Scale plane down, 3/4
 *
 * Provided by Frank Barchard (fbarchard@google.com)
 *
 */
static void ScalePlaneDown34(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const uint8* src_ptr, uint8* dst_ptr,
                             FilterMode filtering) {
  assert(dst_width % 3 == 0);
  void (*ScaleRowDown34_0)(const uint8* src_ptr, int src_stride,
                           uint8* dst_ptr, int dst_width);
  void (*ScaleRowDown34_1)(const uint8* src_ptr, int src_stride,
                           uint8* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN34_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      (dst_width % 24 == 0)) {
    if (!filtering) {
      ScaleRowDown34_0 = ScaleRowDown34_NEON;
      ScaleRowDown34_1 = ScaleRowDown34_NEON;
    } else {
      ScaleRowDown34_0 = ScaleRowDown34_0_Int_NEON;
      ScaleRowDown34_1 = ScaleRowDown34_1_Int_NEON;
    }
  } else
#endif
#if defined(HAS_SCALEROWDOWN34_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      (dst_width % 24 == 0) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_ptr, 8) && IS_ALIGNED(dst_stride, 8)) {
    if (!filtering) {
      ScaleRowDown34_0 = ScaleRowDown34_SSSE3;
      ScaleRowDown34_1 = ScaleRowDown34_SSSE3;
    } else {
      ScaleRowDown34_0 = ScaleRowDown34_0_Int_SSSE3;
      ScaleRowDown34_1 = ScaleRowDown34_1_Int_SSSE3;
    }
  } else
#endif
#if defined(HAS_SCALEROWDOWN34_SSE2)
  if (TestCpuFlag(kCpuHasSSE2) &&
      (dst_width % 24 == 0) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_stride, 8) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(dst_ptr, 8) &&
      filtering) {
    ScaleRowDown34_0 = ScaleRowDown34_0_Int_SSE2;
    ScaleRowDown34_1 = ScaleRowDown34_1_Int_SSE2;
  } else
#endif
  {
    if (!filtering) {
      ScaleRowDown34_0 = ScaleRowDown34_C;
      ScaleRowDown34_1 = ScaleRowDown34_C;
    } else {
      ScaleRowDown34_0 = ScaleRowDown34_0_Int_C;
      ScaleRowDown34_1 = ScaleRowDown34_1_Int_C;
    }
  }
  int src_row = 0;
  for (int y = 0; y < dst_height; ++y) {
    switch (src_row) {
      case 0:
        ScaleRowDown34_0(src_ptr, src_stride, dst_ptr, dst_width);
        break;

      case 1:
        ScaleRowDown34_1(src_ptr, src_stride, dst_ptr, dst_width);
        break;

      case 2:
        ScaleRowDown34_0(src_ptr + src_stride, -src_stride,
                         dst_ptr, dst_width);
        break;
    }
    ++src_row;
    src_ptr += src_stride;
    dst_ptr += dst_stride;
    if (src_row >= 3) {
      src_ptr += src_stride;
      src_row = 0;
    }
  }
}

/**
 * Scale plane, 3/8
 *
 * This is an optimized version for scaling down a plane to 3/8
 * of its original size.
 *
 * Reduces 16x3 to 6x1
 */
static void ScalePlaneDown38(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const uint8* src_ptr, uint8* dst_ptr,
                             FilterMode filtering) {
  assert(dst_width % 3 == 0);
  void (*ScaleRowDown38_3)(const uint8* src_ptr, int src_stride,
                           uint8* dst_ptr, int dst_width);
  void (*ScaleRowDown38_2)(const uint8* src_ptr, int src_stride,
                           uint8* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN38_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      (dst_width % 12 == 0)) {
    if (!filtering) {
      ScaleRowDown38_3 = ScaleRowDown38_NEON;
      ScaleRowDown38_2 = ScaleRowDown38_NEON;
    } else {
      ScaleRowDown38_3 = ScaleRowDown38_3_Int_NEON;
      ScaleRowDown38_2 = ScaleRowDown38_2_Int_NEON;
    }
  } else
#endif
#if defined(HAS_SCALEROWDOWN38_SSSE3)
  if (TestCpuFlag(kCpuHasSSSE3) &&
      (dst_width % 24 == 0) && IS_ALIGNED(src_stride, 16) &&
      IS_ALIGNED(dst_stride, 8) &&
      IS_ALIGNED(src_ptr, 16) && IS_ALIGNED(dst_ptr, 8)) {
    if (!filtering) {
      ScaleRowDown38_3 = ScaleRowDown38_SSSE3;
      ScaleRowDown38_2 = ScaleRowDown38_SSSE3;
    } else {
      ScaleRowDown38_3 = ScaleRowDown38_3_Int_SSSE3;
      ScaleRowDown38_2 = ScaleRowDown38_2_Int_SSSE3;
    }
  } else
#endif
  {
    if (!filtering) {
      ScaleRowDown38_3 = ScaleRowDown38_C;
      ScaleRowDown38_2 = ScaleRowDown38_C;
    } else {
      ScaleRowDown38_3 = ScaleRowDown38_3_Int_C;
      ScaleRowDown38_2 = ScaleRowDown38_2_Int_C;
    }
  }
  int src_row = 0;
  for (int y = 0; y < dst_height; ++y) {
    switch (src_row) {
      case 0:
      case 1:
        ScaleRowDown38_3(src_ptr, src_stride, dst_ptr, dst_width);
        src_ptr += src_stride * 3;
        ++src_row;
        break;

      case 2:
        ScaleRowDown38_2(src_ptr, src_stride, dst_ptr, dst_width);
        src_ptr += src_stride * 2;
        src_row = 0;
        break;
    }
    dst_ptr += dst_stride;
  }
}

static __inline uint32 SumBox(int iboxwidth, int iboxheight,
                              int src_stride, const uint8* src_ptr) {
  assert(iboxwidth > 0);
  assert(iboxheight > 0);
  uint32 sum = 0u;
  for (int y = 0; y < iboxheight; ++y) {
    for (int x = 0; x < iboxwidth; ++x) {
      sum += src_ptr[x];
    }
    src_ptr += src_stride;
  }
  return sum;
}

static void ScalePlaneBoxRow(int dst_width, int boxheight,
                             int dx, int src_stride,
                             const uint8* src_ptr, uint8* dst_ptr) {
  int x = 0;
  for (int i = 0; i < dst_width; ++i) {
    int ix = x >> 16;
    x += dx;
    int boxwidth = (x >> 16) - ix;
    *dst_ptr++ = SumBox(boxwidth, boxheight, src_stride, src_ptr + ix) /
        (boxwidth * boxheight);
  }
}

static __inline uint32 SumPixels(int iboxwidth, const uint16* src_ptr) {
  assert(iboxwidth > 0);
  uint32 sum = 0u;
  for (int x = 0; x < iboxwidth; ++x) {
    sum += src_ptr[x];
  }
  return sum;
}

static void ScaleAddCols2_C(int dst_width, int boxheight, int dx,
                            const uint16* src_ptr, uint8* dst_ptr) {
  int scaletbl[2];
  int minboxwidth = (dx >> 16);
  scaletbl[0] = 65536 / (minboxwidth * boxheight);
  scaletbl[1] = 65536 / ((minboxwidth + 1) * boxheight);
  int *scaleptr = scaletbl - minboxwidth;
  int x = 0;
  for (int i = 0; i < dst_width; ++i) {
    int ix = x >> 16;
    x += dx;
    int boxwidth = (x >> 16) - ix;
    *dst_ptr++ = SumPixels(boxwidth, src_ptr + ix) * scaleptr[boxwidth] >> 16;
  }
}

static void ScaleAddCols1_C(int dst_width, int boxheight, int dx,
                            const uint16* src_ptr, uint8* dst_ptr) {
  int boxwidth = (dx >> 16);
  int scaleval = 65536 / (boxwidth * boxheight);
  int x = 0;
  for (int i = 0; i < dst_width; ++i) {
    *dst_ptr++ = SumPixels(boxwidth, src_ptr + x) * scaleval >> 16;
    x += boxwidth;
  }
}

/**
 * Scale plane down to any dimensions, with interpolation.
 * (boxfilter).
 *
 * Same method as SimpleScale, which is fixed point, outputting
 * one pixel of destination using fixed point (16.16) to step
 * through source, sampling a box of pixel with simple
 * averaging.
 */
static void ScalePlaneBox(int src_width, int src_height,
                          int dst_width, int dst_height,
                          int src_stride, int dst_stride,
                          const uint8* src_ptr, uint8* dst_ptr) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  int dy = (src_height << 16) / dst_height;
  int dx = (src_width << 16) / dst_width;
  if (!IS_ALIGNED(src_width, 16) || (src_width > kMaxInputWidth) ||
      dst_height * 2 > src_height) {
    uint8* dst = dst_ptr;
    int dy = (src_height << 16) / dst_height;
    int dx = (src_width << 16) / dst_width;
    int y = 0;
    for (int j = 0; j < dst_height; ++j) {
      int iy = y >> 16;
      const uint8* const src = src_ptr + iy * src_stride;
      y += dy;
      if (y > (src_height << 16)) {
        y = (src_height << 16);
      }
      int boxheight = (y >> 16) - iy;
      ScalePlaneBoxRow(dst_width, boxheight,
                       dx, src_stride,
                       src, dst);

      dst += dst_stride;
    }
  } else {
    ALIGN16(uint16 row[kMaxInputWidth]);
    void (*ScaleAddRows)(const uint8* src_ptr, int src_stride,
                         uint16* dst_ptr, int src_width, int src_height);
    void (*ScaleAddCols)(int dst_width, int boxheight, int dx,
                         const uint16* src_ptr, uint8* dst_ptr);
#if defined(HAS_SCALEADDROWS_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) &&
        IS_ALIGNED(src_stride, 16) && IS_ALIGNED(src_ptr, 16) &&
        IS_ALIGNED(src_width, 16)) {
      ScaleAddRows = ScaleAddRows_SSE2;
    } else
#endif
    {
      ScaleAddRows = ScaleAddRows_C;
    }
    if (dx & 0xffff) {
      ScaleAddCols = ScaleAddCols2_C;
    } else {
      ScaleAddCols = ScaleAddCols1_C;
    }

    int y = 0;
    for (int j = 0; j < dst_height; ++j) {
      int iy = y >> 16;
      const uint8* const src = src_ptr + iy * src_stride;
      y += dy;
      if (y > (src_height << 16)) {
        y = (src_height << 16);
      }
      int boxheight = (y >> 16) - iy;
      ScaleAddRows(src, src_stride, row, src_width, boxheight);
      ScaleAddCols(dst_width, boxheight, dx, row, dst_ptr);
      dst_ptr += dst_stride;
    }
  }
}

/**
 * Scale plane to/from any dimensions, with interpolation.
 */
static void ScalePlaneBilinearSimple(int src_width, int src_height,
                                     int dst_width, int dst_height,
                                     int src_stride, int dst_stride,
                                     const uint8* src_ptr, uint8* dst_ptr) {
  uint8* dst = dst_ptr;
  int dx = (src_width << 16) / dst_width;
  int dy = (src_height << 16) / dst_height;
  int maxx = ((src_width - 1) << 16) - 1;
  int maxy = ((src_height - 1) << 16) - 1;
  int y = (dst_height < src_height) ? 32768 :
      (src_height << 16) / dst_height - 32768;
  for (int i = 0; i < dst_height; ++i) {
    int cy = (y < 0) ? 0 : y;
    int yi = cy >> 16;
    int yf = cy & 0xffff;
    const uint8* const src = src_ptr + yi * src_stride;
    int x = (dst_width < src_width) ? 32768 :
        (src_width << 16) / dst_width - 32768;
    for (int j = 0; j < dst_width; ++j) {
      int cx = (x < 0) ? 0 : x;
      int xi = cx >> 16;
      int xf = cx & 0xffff;
      int r0 = (src[xi] * (65536 - xf) + src[xi + 1] * xf) >> 16;
      int r1 = (src[xi + src_stride] * (65536 - xf) +
          src[xi + src_stride + 1] * xf) >> 16;
      *dst++ = (r0 * (65536 - yf) + r1 * yf) >> 16;
      x += dx;
      if (x > maxx)
        x = maxx;
    }
    dst += dst_stride - dst_width;
    y += dy;
    if (y > maxy)
      y = maxy;
  }
}

/**
 * Scale plane to/from any dimensions, with bilinear
 * interpolation.
 */
void ScalePlaneBilinear(int src_width, int src_height,
                        int dst_width, int dst_height,
                        int src_stride, int dst_stride,
                        const uint8* src_ptr, uint8* dst_ptr) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  int dy = (src_height << 16) / dst_height;
  int dx = (src_width << 16) / dst_width;
  if (!IS_ALIGNED(src_width, 8) || (src_width > kMaxInputWidth)) {
    ScalePlaneBilinearSimple(src_width, src_height, dst_width, dst_height,
                             src_stride, dst_stride, src_ptr, dst_ptr);

  } else {
    ALIGN16(uint8 row[kMaxInputWidth + 1]);
    void (*ScaleFilterRows)(uint8* dst_ptr, const uint8* src_ptr,
                            int src_stride,
                            int dst_width, int source_y_fraction);
    void (*ScaleFilterCols)(uint8* dst_ptr, const uint8* src_ptr,
                            int dst_width, int dx);
#if defined(HAS_SCALEFILTERROWS_SSSE3)
    if (TestCpuFlag(kCpuHasSSSE3) &&
        IS_ALIGNED(src_stride, 16) && IS_ALIGNED(src_ptr, 16) &&
        IS_ALIGNED(src_width, 16)) {
      ScaleFilterRows = ScaleFilterRows_SSSE3;
    } else
#endif
#if defined(HAS_SCALEFILTERROWS_SSE2)
    if (TestCpuFlag(kCpuHasSSE2) &&
        IS_ALIGNED(src_stride, 16) && IS_ALIGNED(src_ptr, 16) &&
        IS_ALIGNED(src_width, 16)) {
      ScaleFilterRows = ScaleFilterRows_SSE2;
    } else
#endif
    {
      ScaleFilterRows = ScaleFilterRows_C;
    }
    ScaleFilterCols = ScaleFilterCols_C;

    int y = 0;
    int maxy = ((src_height - 1) << 16) - 1; // max is filter of last 2 rows.
    for (int j = 0; j < dst_height; ++j) {
      int iy = y >> 16;
      int fy = (y >> 8) & 255;
      const uint8* const src = src_ptr + iy * src_stride;
      ScaleFilterRows(row, src, src_stride, src_width, fy);
      ScaleFilterCols(dst_ptr, row, dst_width, dx);
      dst_ptr += dst_stride;
      y += dy;
      if (y > maxy) {
        y = maxy;
      }
    }
  }
}

/**
 * Scale plane to/from any dimensions, without interpolation.
 * Fixed point math is used for performance: The upper 16 bits
 * of x and dx is the integer part of the source position and
 * the lower 16 bits are the fixed decimal part.
 */
static void ScalePlaneSimple(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const uint8* src_ptr, uint8* dst_ptr) {
  uint8* dst = dst_ptr;
  int dx = (src_width << 16) / dst_width;
  for (int y = 0; y < dst_height; ++y) {
    const uint8* const src = src_ptr + (y * src_height / dst_height) *
        src_stride;
    // TODO(fbarchard): Round X coordinate by setting x=0x8000.
    int x = 0;
    for (int i = 0; i < dst_width; ++i) {
      *dst++ = src[x >> 16];
      x += dx;
    }
    dst += dst_stride - dst_width;
  }
}

/**
 * Scale plane to/from any dimensions.
 */
static void ScalePlaneAnySize(int src_width, int src_height,
                              int dst_width, int dst_height,
                              int src_stride, int dst_stride,
                              const uint8* src_ptr, uint8* dst_ptr,
                              FilterMode filtering) {
  if (!filtering) {
    ScalePlaneSimple(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src_ptr, dst_ptr);
  } else {
    // fall back to non-optimized version
    ScalePlaneBilinear(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src_ptr, dst_ptr);
  }
}

/**
 * Scale plane down, any size
 *
 * This is an optimized version for scaling down a plane to any size.
 * The current implementation is ~10 times faster compared to the
 * reference implementation for e.g. XGA->LowResPAL
 *
 */
static void ScalePlaneDown(int src_width, int src_height,
                           int dst_width, int dst_height,
                           int src_stride, int dst_stride,
                           const uint8* src_ptr, uint8* dst_ptr,
                           FilterMode filtering) {
  if (!filtering) {
    ScalePlaneSimple(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src_ptr, dst_ptr);
  } else if (filtering == kFilterBilinear || src_height * 2 > dst_height) {
    // between 1/2x and 1x use bilinear
    ScalePlaneBilinear(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src_ptr, dst_ptr);
  } else {
    ScalePlaneBox(src_width, src_height, dst_width, dst_height,
                  src_stride, dst_stride, src_ptr, dst_ptr);
  }
}

static void ScalePlane(const uint8* src, int src_stride,
                       int src_width, int src_height,
                       uint8* dst, int dst_stride,
                       int dst_width, int dst_height,
                       FilterMode filtering, bool use_ref) {
  // Use specialized scales to improve performance for common resolutions.
  // For example, all the 1/2 scalings will use ScalePlaneDown2()
  if (dst_width == src_width && dst_height == src_height) {
    // Straight copy.
    CopyPlane(src, src_stride, dst, dst_stride, dst_width, dst_height);
  } else if (dst_width <= src_width && dst_height <= src_height) {
    // Scale down.
    if (use_ref) {
      // For testing, allow the optimized versions to be disabled.
      ScalePlaneDown(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src, dst, filtering);
    } else if (4 * dst_width == 3 * src_width &&
               4 * dst_height == 3 * src_height) {
      // optimized, 3/4
      ScalePlaneDown34(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src, dst, filtering);
    } else if (2 * dst_width == src_width && 2 * dst_height == src_height) {
      // optimized, 1/2
      ScalePlaneDown2(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
    // 3/8 rounded up for odd sized chroma height.
    } else if (8 * dst_width == 3 * src_width &&
               dst_height == ((src_height * 3 + 7) / 8)) {
      // optimized, 3/8
      ScalePlaneDown38(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src, dst, filtering);
    } else if (4 * dst_width == src_width && 4 * dst_height == src_height) {
      // optimized, 1/4
      ScalePlaneDown4(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
    } else if (8 * dst_width == src_width && 8 * dst_height == src_height) {
      // optimized, 1/8
      ScalePlaneDown8(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
    } else {
      // Arbitrary downsample
      ScalePlaneDown(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src, dst, filtering);
    }
  } else {
    // Arbitrary scale up and/or down.
    ScalePlaneAnySize(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
  }
}

/**
 * Scale a plane.
 *
 * This function in turn calls a scaling function
 * suitable for handling the desired resolutions.
 *
 */

int I420Scale(const uint8* src_y, int src_stride_y,
              const uint8* src_u, int src_stride_u,
              const uint8* src_v, int src_stride_v,
              int src_width, int src_height,
              uint8* dst_y, int dst_stride_y,
              uint8* dst_u, int dst_stride_u,
              uint8* dst_v, int dst_stride_v,
              int dst_width, int dst_height,
              FilterMode filtering) {
  if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
      !dst_y || !dst_u || !dst_v || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    int halfheight = (src_height + 1) >> 1;
    src_y = src_y + (src_height - 1) * src_stride_y;
    src_u = src_u + (halfheight - 1) * src_stride_u;
    src_v = src_v + (halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  int src_halfwidth = (src_width + 1) >> 1;
  int src_halfheight = (src_height + 1) >> 1;
  int dst_halfwidth = (dst_width + 1) >> 1;
  int dst_halfheight = (dst_height + 1) >> 1;

  ScalePlane(src_y, src_stride_y, src_width, src_height,
             dst_y, dst_stride_y, dst_width, dst_height,
             filtering, use_reference_impl_);
  ScalePlane(src_u, src_stride_u, src_halfwidth, src_halfheight,
             dst_u, dst_stride_u, dst_halfwidth, dst_halfheight,
             filtering, use_reference_impl_);
  ScalePlane(src_v, src_stride_v, src_halfwidth, src_halfheight,
             dst_v, dst_stride_v, dst_halfwidth, dst_halfheight,
             filtering, use_reference_impl_);
  return 0;
}

// Deprecated api
int Scale(const uint8* src_y, const uint8* src_u, const uint8* src_v,
          int src_stride_y, int src_stride_u, int src_stride_v,
          int src_width, int src_height,
          uint8* dst_y, uint8* dst_u, uint8* dst_v,
          int dst_stride_y, int dst_stride_u, int dst_stride_v,
          int dst_width, int dst_height,
          bool interpolate) {
  if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
      !dst_y || !dst_u || !dst_v || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    int halfheight = (src_height + 1) >> 1;
    src_y = src_y + (src_height - 1) * src_stride_y;
    src_u = src_u + (halfheight - 1) * src_stride_u;
    src_v = src_v + (halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  int src_halfwidth = (src_width + 1) >> 1;
  int src_halfheight = (src_height + 1) >> 1;
  int dst_halfwidth = (dst_width + 1) >> 1;
  int dst_halfheight = (dst_height + 1) >> 1;
  FilterMode filtering = interpolate ? kFilterBox : kFilterNone;

  ScalePlane(src_y, src_stride_y, src_width, src_height,
             dst_y, dst_stride_y, dst_width, dst_height,
             filtering, use_reference_impl_);
  ScalePlane(src_u, src_stride_u, src_halfwidth, src_halfheight,
             dst_u, dst_stride_u, dst_halfwidth, dst_halfheight,
             filtering, use_reference_impl_);
  ScalePlane(src_v, src_stride_v, src_halfwidth, src_halfheight,
             dst_v, dst_stride_v, dst_halfwidth, dst_halfheight,
             filtering, use_reference_impl_);
  return 0;
}

// Deprecated api
int ScaleOffset(const uint8* src, int src_width, int src_height,
                uint8* dst, int dst_width, int dst_height, int dst_yoffset,
                bool interpolate) {
  if (!src || src_width <= 0 || src_height <= 0 ||
      !dst || dst_width <= 0 || dst_height <= 0 || dst_yoffset < 0 ||
      dst_yoffset >= dst_height) {
    return -1;
  }
  dst_yoffset = dst_yoffset & ~1;  // chroma requires offset to multiple of 2.
  int src_halfwidth = (src_width + 1) >> 1;
  int src_halfheight = (src_height + 1) >> 1;
  int dst_halfwidth = (dst_width + 1) >> 1;
  int dst_halfheight = (dst_height + 1) >> 1;
  int aheight = dst_height - dst_yoffset * 2;  // actual output height
  const uint8* const src_y = src;
  const uint8* const src_u = src + src_width * src_height;
  const uint8* const src_v = src + src_width * src_height +
                             src_halfwidth * src_halfheight;
  uint8* dst_y = dst + dst_yoffset * dst_width;
  uint8* dst_u = dst + dst_width * dst_height +
                 (dst_yoffset >> 1) * dst_halfwidth;
  uint8* dst_v = dst + dst_width * dst_height + dst_halfwidth * dst_halfheight +
                 (dst_yoffset >> 1) * dst_halfwidth;
  return Scale(src_y, src_u, src_v, src_width, src_halfwidth, src_halfwidth,
               src_width, src_height, dst_y, dst_u, dst_v, dst_width,
               dst_halfwidth, dst_halfwidth, dst_width, aheight, interpolate);
}

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
