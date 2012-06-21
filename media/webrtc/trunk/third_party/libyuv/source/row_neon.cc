/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "row.h"

#ifdef __cplusplus
namespace libyuv {
extern "C" {
#endif

#define YUVTORGB                                                               \
    "vld1.u8    {d0}, [%0]!                    \n"                             \
    "vld1.u32   {d2[0]}, [%1]!                 \n"                             \
    "vld1.u32   {d2[1]}, [%2]!                 \n"                             \
                                                                               \
    "veor.u8    d2, d26                        \n"/*subtract 128 from u and v*/\
                                                                               \
    "vmull.s8   q8, d2, d24                    \n"/*  u/v B/R component      */\
                                                                               \
    "vmull.s8   q9, d2, d25                    \n"/*  u/v G component        */\
                                                                               \
    "vmov.u8    d1, #0                         \n"/*  split odd/even y apart */\
    "vtrn.u8    d0, d1                         \n"                             \
                                                                               \
    "vsub.s16   q0, q0, q15                    \n"/*  offset y               */\
    "vmul.s16   q0, q0, q14                    \n"                             \
                                                                               \
    "vadd.s16   d18, d19                       \n"                             \
                                                                               \
    "vqadd.s16  d20, d0, d16                   \n"                             \
    "vqadd.s16  d21, d1, d16                   \n"                             \
                                                                               \
    "vqadd.s16  d22, d0, d17                   \n"                             \
    "vqadd.s16  d23, d1, d17                   \n"                             \
                                                                               \
    "vqadd.s16  d16, d0, d18                   \n"                             \
    "vqadd.s16  d17, d1, d18                   \n"                             \
                                                                               \
    "vqrshrun.s16 d0, q10, #6                  \n"                             \
    "vqrshrun.s16 d1, q11, #6                  \n"                             \
    "vqrshrun.s16 d2, q8, #6                   \n"                             \
                                                                               \
    "vmovl.u8   q10, d0                        \n"/*  set up for reinterleave*/\
    "vmovl.u8   q11, d1                        \n"                             \
    "vmovl.u8   q8, d2                         \n"                             \
                                                                               \
    "vtrn.u8    d20, d21                       \n"                             \
    "vtrn.u8    d22, d23                       \n"                             \
    "vtrn.u8    d16, d17                       \n"                             \

#if defined(HAS_FASTCONVERTYUVTOARGBROW_NEON) || \
    defined(HAS_FASTCONVERTYUVTOBGRAROW_NEON) || \
    defined(HAS_FASTCONVERTYUVTOABGRROW_NEON)
static const vec8 kUVToRB[8]  = { 127, 127, 127, 127, 102, 102, 102, 102 };
static const vec8 kUVToG[8]   = { -25, -25, -25, -25, -52, -52, -52, -52 };
#endif

#if defined(HAS_FASTCONVERTYUVTOARGBROW_NEON)
void FastConvertYUVToARGBRow_NEON(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width) {
  asm volatile (
    "vld1.u8    {d24}, [%5]                    \n"
    "vld1.u8    {d25}, [%6]                    \n"
    "vmov.u8    d26, #128                      \n"
    "vmov.u16   q14, #74                       \n"
    "vmov.u16   q15, #16                       \n"
  "1:                                          \n"
YUVTORGB
    "vmov.u8    d21, d16                       \n"
    "vmov.u8    d23, #255                      \n"
    "vst4.u8    {d20, d21, d22, d23}, [%3]!    \n"
    "subs       %4, %4, #8                     \n"
    "bhi        1b                             \n"
    : "+r"(y_buf),          // %0
      "+r"(u_buf),          // %1
      "+r"(v_buf),          // %2
      "+r"(rgb_buf),        // %3
      "+r"(width)           // %4
    : "r"(kUVToRB),
      "r"(kUVToG)
    : "cc", "memory", "q0", "q1", "q2", "q3", "q8", "q9",
                      "q10", "q11", "q12", "q13", "q14", "q15"
  );
}
#endif

#if defined(HAS_FASTCONVERTYUVTOBGRAROW_NEON)
void FastConvertYUVToBGRARow_NEON(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width) {
  asm volatile (
    "vld1.u8    {d24}, [%5]                    \n"
    "vld1.u8    {d25}, [%6]                    \n"
    "vmov.u8    d26, #128                      \n"
    "vmov.u16   q14, #74                       \n"
    "vmov.u16   q15, #16                       \n"
  "1:                                          \n"
YUVTORGB
    "vswp.u8    d20, d22                       \n"
    "vmov.u8    d21, d16                       \n"
    "vmov.u8    d19, #255                      \n"
    "vst4.u8    {d19, d20, d21, d22}, [%3]!    \n"
    "subs       %4, %4, #8                     \n"
    "bhi        1b                             \n"
    : "+r"(y_buf),          // %0
      "+r"(u_buf),          // %1
      "+r"(v_buf),          // %2
      "+r"(rgb_buf),        // %3
      "+r"(width)           // %4
    : "r"(kUVToRB),
      "r"(kUVToG)
    : "cc", "memory", "q0", "q1", "q2", "q3", "q8", "q9",
                      "q10", "q11", "q12", "q13", "q14", "q15"
  );
}
#endif

#if defined(HAS_FASTCONVERTYUVTOABGRROW_NEON)
void FastConvertYUVToABGRRow_NEON(const uint8* y_buf,
                                  const uint8* u_buf,
                                  const uint8* v_buf,
                                  uint8* rgb_buf,
                                  int width) {
  asm volatile (
    "vld1.u8    {d24}, [%5]                    \n"
    "vld1.u8    {d25}, [%6]                    \n"
    "vmov.u8    d26, #128                      \n"
    "vmov.u16   q14, #74                       \n"
    "vmov.u16   q15, #16                       \n"
  "1:                                          \n"
YUVTORGB
    "vswp.u8    d20, d22                       \n"
    "vmov.u8    d21, d16                       \n"
    "vmov.u8    d23, #255                      \n"
    "vst4.u8    {d20, d21, d22, d23}, [%3]!    \n"
    "subs       %4, %4, #8                     \n"
    "bhi        1b                             \n"
    : "+r"(y_buf),          // %0
      "+r"(u_buf),          // %1
      "+r"(v_buf),          // %2
      "+r"(rgb_buf),        // %3
      "+r"(width)           // %4
    : "r"(kUVToRB),
      "r"(kUVToG)
    : "cc", "memory", "q0", "q1", "q2", "q3", "q8", "q9",
                      "q10", "q11", "q12", "q13", "q14", "q15"
  );
}
#endif

#ifdef __cplusplus
}  // extern "C"
}  // namespace libyuv
#endif
