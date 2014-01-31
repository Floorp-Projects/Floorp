/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/cpu_id.h"
#include "libyuv/format_conversion.h"
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "../unit_test/unit_test.h"

#if defined(_MSC_VER)
#define SIMD_ALIGNED(var) __declspec(align(16)) var
#else  // __GNUC__
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
#endif

namespace libyuv {

#define TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, N, NEG) \
TEST_F(libyuvTest, FMT_PLANAR##To##FMT_B##N##_OptVsC) {                        \
  const int kWidth = 1280;                                                     \
  const int kHeight = 720;                                                     \
  const int kStride = (kWidth * 8 * BPP_B + 7) / 8;                            \
  align_buffer_16(src_y, kWidth * kHeight);                                    \
  align_buffer_16(src_u, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y);            \
  align_buffer_16(src_v, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y);            \
  align_buffer_16(dst_argb_c, kStride * kHeight);                              \
  align_buffer_16(dst_argb_opt, kStride * kHeight);                            \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kWidth; ++j)                                           \
      src_y[(i * kWidth) + j] = (random() & 0xff);                             \
  for (int i = 0; i < kHeight / SUBSAMP_Y; ++i)                                \
    for (int j = 0; j < kWidth / SUBSAMP_X; ++j) {                             \
      src_u[(i * kWidth / SUBSAMP_X) + j] = (random() & 0xff);                 \
      src_v[(i * kWidth / SUBSAMP_X) + j] = (random() & 0xff);                 \
    }                                                                          \
  MaskCpuFlags(kCpuInitialized);                                               \
  FMT_PLANAR##To##FMT_B(src_y, kWidth,                                         \
                        src_u, kWidth / SUBSAMP_X,                             \
                        src_v, kWidth / SUBSAMP_X,                             \
                        dst_argb_c, kStride,                                   \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_PLANAR##To##FMT_B(src_y, kWidth,                                       \
                          src_u, kWidth / SUBSAMP_X,                           \
                          src_v, kWidth / SUBSAMP_X,                           \
                          dst_argb_opt, kStride,                               \
                          kWidth, NEG kHeight);                                \
  }                                                                            \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kHeight; ++i) {                                          \
    for (int j = 0; j < kWidth * BPP_B; ++j) {                                 \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_argb_c[i * kWidth * BPP_B + j]) -           \
              static_cast<int>(dst_argb_opt[i * kWidth * BPP_B + j]));         \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 2);                                                      \
  free_aligned_buffer_16(src_y)                                                \
  free_aligned_buffer_16(src_u)                                                \
  free_aligned_buffer_16(src_v)                                                \
  free_aligned_buffer_16(dst_argb_c)                                           \
  free_aligned_buffer_16(dst_argb_opt)                                         \
}

#define TESTPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B)          \
    TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, , +)        \
    TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, Invert, -)

TESTPLANARTOB(I420, 2, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, BGRA, 4)
TESTPLANARTOB(I420, 2, 2, ABGR, 4)
TESTPLANARTOB(I420, 2, 2, RGBA, 4)
TESTPLANARTOB(I420, 2, 2, RAW, 3)
TESTPLANARTOB(I420, 2, 2, RGB24, 3)
TESTPLANARTOB(I420, 2, 2, RGB565, 2)
TESTPLANARTOB(I420, 2, 2, ARGB1555, 2)
TESTPLANARTOB(I420, 2, 2, ARGB4444, 2)
TESTPLANARTOB(I422, 2, 1, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, BGRA, 4)
TESTPLANARTOB(I422, 2, 1, ABGR, 4)
TESTPLANARTOB(I422, 2, 1, RGBA, 4)
TESTPLANARTOB(I411, 4, 1, ARGB, 4)
TESTPLANARTOB(I444, 1, 1, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, YUY2, 2)
TESTPLANARTOB(I420, 2, 2, UYVY, 2)
// TODO(fbarchard): Re-enable test and fix valgrind.
// TESTPLANARTOB(I420, 2, 2, V210, 16 / 6)
TESTPLANARTOB(I420, 2, 2, I400, 1)
TESTPLANARTOB(I420, 2, 2, BayerBGGR, 1)
TESTPLANARTOB(I420, 2, 2, BayerRGGB, 1)
TESTPLANARTOB(I420, 2, 2, BayerGBRG, 1)
TESTPLANARTOB(I420, 2, 2, BayerGRBG, 1)

#define TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,       \
                         N, NEG)                                               \
TEST_F(libyuvTest, FMT_PLANAR##To##FMT_B##N##_OptVsC) {                        \
  const int kWidth = 1280;                                                     \
  const int kHeight = 720;                                                     \
  align_buffer_16(src_y, kWidth * kHeight);                                    \
  align_buffer_16(src_uv, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y * 2);       \
  align_buffer_16(dst_argb_c, (kWidth * BPP_B) * kHeight);                     \
  align_buffer_16(dst_argb_opt, (kWidth * BPP_B) * kHeight);                   \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kWidth; ++j)                                           \
      src_y[(i * kWidth) + j] = (random() & 0xff);                             \
  for (int i = 0; i < kHeight / SUBSAMP_Y; ++i)                                \
    for (int j = 0; j < kWidth / SUBSAMP_X * 2; ++j) {                         \
      src_uv[(i * kWidth / SUBSAMP_X) * 2 + j] = (random() & 0xff);            \
    }                                                                          \
  MaskCpuFlags(kCpuInitialized);                                               \
  FMT_PLANAR##To##FMT_B(src_y, kWidth,                                         \
                        src_uv, kWidth / SUBSAMP_X * 2,                        \
                        dst_argb_c, kWidth * BPP_B,                            \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_PLANAR##To##FMT_B(src_y, kWidth,                                       \
                          src_uv, kWidth / SUBSAMP_X * 2,                      \
                          dst_argb_opt, kWidth * BPP_B,                        \
                          kWidth, NEG kHeight);                                \
  }                                                                            \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kHeight; ++i) {                                          \
    for (int j = 0; j < kWidth * BPP_B; ++j) {                                 \
      int abs_diff =                                                           \
        abs(static_cast<int>(dst_argb_c[i * kWidth * BPP_B + j]) -             \
            static_cast<int>(dst_argb_opt[i * kWidth * BPP_B + j]));           \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 3);                                                      \
  free_aligned_buffer_16(src_y)                                                \
  free_aligned_buffer_16(src_uv)                                               \
  free_aligned_buffer_16(dst_argb_c)                                           \
  free_aligned_buffer_16(dst_argb_opt)                                         \
}

#define TESTBIPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B)        \
    TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, , +)      \
    TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, Invert, -)

TESTBIPLANARTOB(NV12, 2, 2, ARGB, 4)
TESTBIPLANARTOB(NV21, 2, 2, ARGB, 4)
TESTBIPLANARTOB(NV12, 2, 2, RGB565, 2)
TESTBIPLANARTOB(NV21, 2, 2, RGB565, 2)

#define TESTATOPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, N, NEG) \
TEST_F(libyuvTest, FMT_A##To##FMT_PLANAR##N##_OptVsC) {                        \
  const int kWidth = 1280;                                                     \
  const int kHeight = 720;                                                     \
  const int kStride = (kWidth * 8 * BPP_A + 7) / 8;                            \
  align_buffer_16(src_argb, kStride * kHeight);                                \
  align_buffer_16(dst_y_c, kWidth * kHeight);                                  \
  align_buffer_16(dst_u_c, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y);          \
  align_buffer_16(dst_v_c, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y);          \
  align_buffer_16(dst_y_opt, kWidth * kHeight);                                \
  align_buffer_16(dst_u_opt, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y);        \
  align_buffer_16(dst_v_opt, kWidth / SUBSAMP_X * kHeight / SUBSAMP_Y);        \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kStride; ++j)                                          \
      src_argb[(i * kStride) + j] = (random() & 0xff);                         \
  MaskCpuFlags(kCpuInitialized);                                               \
  FMT_A##To##FMT_PLANAR(src_argb, kStride,                                     \
                        dst_y_c, kWidth,                                       \
                        dst_u_c, kWidth / SUBSAMP_X,                           \
                        dst_v_c, kWidth / SUBSAMP_X,                           \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_A##To##FMT_PLANAR(src_argb, kStride,                                   \
                          dst_y_opt, kWidth,                                   \
                          dst_u_opt, kWidth / SUBSAMP_X,                       \
                          dst_v_opt, kWidth / SUBSAMP_X,                       \
                          kWidth, NEG kHeight);                                \
  }                                                                            \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kHeight; ++i) {                                          \
    for (int j = 0; j < kWidth; ++j) {                                         \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_y_c[i * kWidth + j]) -                      \
              static_cast<int>(dst_y_opt[i * kWidth + j]));                    \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 2);                                                      \
  for (int i = 0; i < kHeight / SUBSAMP_Y; ++i) {                              \
    for (int j = 0; j < kWidth / SUBSAMP_X; ++j) {                             \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_u_c[i * kWidth / SUBSAMP_X + j]) -          \
              static_cast<int>(dst_u_opt[i * kWidth / SUBSAMP_X + j]));        \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 2);                                                      \
  for (int i = 0; i < kHeight / SUBSAMP_Y; ++i) {                              \
    for (int j = 0; j < kWidth / SUBSAMP_X; ++j) {                             \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_v_c[i * kWidth / SUBSAMP_X + j]) -          \
              static_cast<int>(dst_v_opt[i * kWidth / SUBSAMP_X + j]));        \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 2);                                                      \
  free_aligned_buffer_16(dst_y_c)                                              \
  free_aligned_buffer_16(dst_u_c)                                              \
  free_aligned_buffer_16(dst_v_c)                                              \
  free_aligned_buffer_16(dst_y_opt)                                            \
  free_aligned_buffer_16(dst_u_opt)                                            \
  free_aligned_buffer_16(dst_v_opt)                                            \
  free_aligned_buffer_16(src_argb)                                             \
}

#define TESTATOPLANAR(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)          \
    TESTATOPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, , +)        \
    TESTATOPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, Invert, -)

TESTATOPLANAR(ARGB, 4, I420, 2, 2)
TESTATOPLANAR(BGRA, 4, I420, 2, 2)
TESTATOPLANAR(ABGR, 4, I420, 2, 2)
TESTATOPLANAR(RGBA, 4, I420, 2, 2)
TESTATOPLANAR(RAW, 3, I420, 2, 2)
TESTATOPLANAR(RGB24, 3, I420, 2, 2)
TESTATOPLANAR(RGB565, 2, I420, 2, 2)
TESTATOPLANAR(ARGB1555, 2, I420, 2, 2)
TESTATOPLANAR(ARGB4444, 2, I420, 2, 2)
// TESTATOPLANAR(ARGB, 4, I411, 4, 1)
TESTATOPLANAR(ARGB, 4, I422, 2, 1)
// TESTATOPLANAR(ARGB, 4, I444, 1, 1)
// TODO(fbarchard): Implement and test 411 and 444
TESTATOPLANAR(YUY2, 2, I420, 2, 2)
TESTATOPLANAR(UYVY, 2, I420, 2, 2)
TESTATOPLANAR(V210, 16 / 6, I420, 2, 2)
TESTATOPLANAR(I400, 1, I420, 2, 2)
TESTATOPLANAR(BayerBGGR, 1, I420, 2, 2)
TESTATOPLANAR(BayerRGGB, 1, I420, 2, 2)
TESTATOPLANAR(BayerGBRG, 1, I420, 2, 2)
TESTATOPLANAR(BayerGRBG, 1, I420, 2, 2)

#define TESTATOBI(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B, N, NEG)                \
TEST_F(libyuvTest, FMT_A##To##FMT_B##N##_OptVsC) {                             \
  const int kWidth = 1280;                                                     \
  const int kHeight = 720;                                                     \
  align_buffer_16(src_argb, (kWidth * BPP_A) * kHeight);                       \
  align_buffer_16(dst_argb_c, (kWidth * BPP_B) * kHeight);                     \
  align_buffer_16(dst_argb_opt, (kWidth * BPP_B) * kHeight);                   \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight * kWidth * BPP_A; ++i) {                         \
    src_argb[i] = (random() & 0xff);                                           \
  }                                                                            \
  MaskCpuFlags(kCpuInitialized);                                               \
  FMT_A##To##FMT_B(src_argb, kWidth * STRIDE_A,                                \
                   dst_argb_c, kWidth * BPP_B,                                 \
                   kWidth, NEG kHeight);                                       \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_A##To##FMT_B(src_argb, kWidth * STRIDE_A,                              \
                     dst_argb_opt, kWidth * BPP_B,                             \
                     kWidth, NEG kHeight);                                     \
  }                                                                            \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kHeight * kWidth * BPP_B; ++i) {                         \
    int abs_diff =                                                             \
        abs(static_cast<int>(dst_argb_c[i]) -                                  \
            static_cast<int>(dst_argb_opt[i]));                                \
    if (abs_diff > max_diff) {                                                 \
      max_diff = abs_diff;                                                     \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 2);                                                      \
  free_aligned_buffer_16(src_argb)                                             \
  free_aligned_buffer_16(dst_argb_c)                                           \
  free_aligned_buffer_16(dst_argb_opt)                                         \
}
#define TESTATOB(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B)                         \
    TESTATOBI(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B, , +)                       \
    TESTATOBI(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B, Invert, -)

TESTATOB(ARGB, 4, 4, ARGB, 4)
TESTATOB(ARGB, 4, 4, BGRA, 4)
TESTATOB(ARGB, 4, 4, ABGR, 4)
TESTATOB(ARGB, 4, 4, RGBA, 4)
TESTATOB(ARGB, 4, 4, RAW, 3)
TESTATOB(ARGB, 4, 4, RGB24, 3)
TESTATOB(ARGB, 4, 4, RGB565, 2)
TESTATOB(ARGB, 4, 4, ARGB1555, 2)
TESTATOB(ARGB, 4, 4, ARGB4444, 2)
TESTATOB(BGRA, 4, 4, ARGB, 4)
TESTATOB(ABGR, 4, 4, ARGB, 4)
TESTATOB(RGBA, 4, 4, ARGB, 4)
TESTATOB(RAW, 3, 3, ARGB, 4)
TESTATOB(RGB24, 3, 3, ARGB, 4)
TESTATOB(RGB565, 2, 2, ARGB, 4)
TESTATOB(ARGB1555, 2, 2, ARGB, 4)
TESTATOB(ARGB4444, 2, 2, ARGB, 4)
TESTATOB(YUY2, 2, 2, ARGB, 4)
TESTATOB(UYVY, 2, 2, ARGB, 4)
TESTATOB(M420, 3 / 2, 1, ARGB, 4)

static const int kReadPad = 16;  // Allow overread of 16 bytes.
#define TESTATOBRANDOM(FMT_A, BPP_A, STRIDE_A, FMT_B, BPP_B)                   \
TEST_F(libyuvTest, FMT_A##To##FMT_B##_Random) {                                \
  srandom(time(NULL));                                                         \
  for (int times = 0; times < benchmark_iterations_; ++times) {                \
    const int kWidth = (random() & 63) + 1;                                    \
    const int kHeight = (random() & 31) + 1;                                   \
    align_buffer_page_end(src_argb, (kWidth * BPP_A) * kHeight + kReadPad);    \
    align_buffer_page_end(dst_argb_c, (kWidth * BPP_B) * kHeight);             \
    align_buffer_page_end(dst_argb_opt, (kWidth * BPP_B) * kHeight);           \
    for (int i = 0; i < kHeight * kWidth * BPP_A; ++i) {                       \
      src_argb[i] = (random() & 0xff);                                         \
    }                                                                          \
    MaskCpuFlags(kCpuInitialized);                                             \
    FMT_A##To##FMT_B(src_argb, kWidth * STRIDE_A,                              \
                     dst_argb_c, kWidth * BPP_B,                               \
                     kWidth, kHeight);                                         \
    MaskCpuFlags(-1);                                                          \
    FMT_A##To##FMT_B(src_argb, kWidth * STRIDE_A,                              \
                     dst_argb_opt, kWidth * BPP_B,                             \
                     kWidth, kHeight);                                         \
    int max_diff = 0;                                                          \
    for (int i = 0; i < kHeight * kWidth * BPP_B; ++i) {                       \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_argb_c[i]) -                                \
              static_cast<int>(dst_argb_opt[i]));                              \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, 2);                                                    \
    free_aligned_buffer_page_end(src_argb)                                     \
    free_aligned_buffer_page_end(dst_argb_c)                                   \
    free_aligned_buffer_page_end(dst_argb_opt)                                 \
  }                                                                            \
}

TESTATOBRANDOM(ARGB, 4, 4, ARGB, 4)
TESTATOBRANDOM(ARGB, 4, 4, BGRA, 4)
TESTATOBRANDOM(ARGB, 4, 4, ABGR, 4)
TESTATOBRANDOM(ARGB, 4, 4, RGBA, 4)
TESTATOBRANDOM(ARGB, 4, 4, RAW, 3)
TESTATOBRANDOM(ARGB, 4, 4, RGB24, 3)
TESTATOBRANDOM(ARGB, 4, 4, RGB565, 2)
TESTATOBRANDOM(ARGB, 4, 4, ARGB1555, 2)
TESTATOBRANDOM(ARGB, 4, 4, ARGB4444, 2)

TESTATOBRANDOM(BGRA, 4, 4, ARGB, 4)
TESTATOBRANDOM(ABGR, 4, 4, ARGB, 4)
TESTATOBRANDOM(RGBA, 4, 4, ARGB, 4)
TESTATOBRANDOM(RAW, 3, 3, ARGB, 4)
TESTATOBRANDOM(RGB24, 3, 3, ARGB, 4)
TESTATOBRANDOM(RGB565, 2, 2, ARGB, 4)
TESTATOBRANDOM(ARGB1555, 2, 2, ARGB, 4)
TESTATOBRANDOM(ARGB4444, 2, 2, ARGB, 4)

TEST_F(libyuvTest, TestAttenuate) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);
  SIMD_ALIGNED(uint8 atten_pixels[256][4]);
  SIMD_ALIGNED(uint8 unatten_pixels[256][4]);
  SIMD_ALIGNED(uint8 atten2_pixels[256][4]);

  // Test unattenuation clamps
  orig_pixels[0][0] = 200u;
  orig_pixels[0][1] = 129u;
  orig_pixels[0][2] = 127u;
  orig_pixels[0][3] = 128u;
  // Test unattenuation transparent and opaque are unaffected
  orig_pixels[1][0] = 16u;
  orig_pixels[1][1] = 64u;
  orig_pixels[1][2] = 192u;
  orig_pixels[1][3] = 0u;
  orig_pixels[2][0] = 16u;
  orig_pixels[2][1] = 64u;
  orig_pixels[2][2] = 192u;
  orig_pixels[2][3] = 255u;
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 128u;
  ARGBUnattenuate(&orig_pixels[0][0], 0, &unatten_pixels[0][0], 0, 4, 1);
  EXPECT_EQ(255u, unatten_pixels[0][0]);
  EXPECT_EQ(255u, unatten_pixels[0][1]);
  EXPECT_EQ(254u, unatten_pixels[0][2]);
  EXPECT_EQ(128u, unatten_pixels[0][3]);
  EXPECT_EQ(16u, unatten_pixels[1][0]);
  EXPECT_EQ(64u, unatten_pixels[1][1]);
  EXPECT_EQ(192u, unatten_pixels[1][2]);
  EXPECT_EQ(0u, unatten_pixels[1][3]);
  EXPECT_EQ(16u, unatten_pixels[2][0]);
  EXPECT_EQ(64u, unatten_pixels[2][1]);
  EXPECT_EQ(192u, unatten_pixels[2][2]);
  EXPECT_EQ(255u, unatten_pixels[2][3]);
  EXPECT_EQ(32u, unatten_pixels[3][0]);
  EXPECT_EQ(128u, unatten_pixels[3][1]);
  EXPECT_EQ(255u, unatten_pixels[3][2]);
  EXPECT_EQ(128u, unatten_pixels[3][3]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  ARGBAttenuate(&orig_pixels[0][0], 0, &atten_pixels[0][0], 0, 256, 1);
  ARGBUnattenuate(&atten_pixels[0][0], 0, &unatten_pixels[0][0], 0, 256, 1);
  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBAttenuate(&unatten_pixels[0][0], 0, &atten2_pixels[0][0], 0, 256, 1);
  }
  for (int i = 0; i < 256; ++i) {
    EXPECT_NEAR(atten_pixels[i][0], atten2_pixels[i][0], 2);
    EXPECT_NEAR(atten_pixels[i][1], atten2_pixels[i][1], 2);
    EXPECT_NEAR(atten_pixels[i][2], atten2_pixels[i][2], 2);
    EXPECT_NEAR(atten_pixels[i][3], atten2_pixels[i][3], 2);
  }
  // Make sure transparent, 50% and opaque are fully accurate.
  EXPECT_EQ(0, atten_pixels[0][0]);
  EXPECT_EQ(0, atten_pixels[0][1]);
  EXPECT_EQ(0, atten_pixels[0][2]);
  EXPECT_EQ(0, atten_pixels[0][3]);
  EXPECT_EQ(64, atten_pixels[128][0]);
  EXPECT_EQ(32, atten_pixels[128][1]);
  EXPECT_EQ(21,  atten_pixels[128][2]);
  EXPECT_EQ(128, atten_pixels[128][3]);
  EXPECT_EQ(255, atten_pixels[255][0]);
  EXPECT_EQ(127, atten_pixels[255][1]);
  EXPECT_EQ(85,  atten_pixels[255][2]);
  EXPECT_EQ(255, atten_pixels[255][3]);
}

TEST_F(libyuvTest, TestARGBComputeCumulativeSum) {
  SIMD_ALIGNED(uint8 orig_pixels[16][16][4]);
  SIMD_ALIGNED(int32 added_pixels[16][16][4]);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      orig_pixels[y][x][0] = 1u;
      orig_pixels[y][x][1] = 2u;
      orig_pixels[y][x][2] = 3u;
      orig_pixels[y][x][3] = 255u;
    }
  }

  ARGBComputeCumulativeSum(&orig_pixels[0][0][0], 16 * 4,
                           &added_pixels[0][0][0], 16 * 4,
                           16, 16);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      EXPECT_EQ((x + 1) * (y + 1), added_pixels[y][x][0]);
      EXPECT_EQ((x + 1) * (y + 1) * 2, added_pixels[y][x][1]);
      EXPECT_EQ((x + 1) * (y + 1) * 3, added_pixels[y][x][2]);
      EXPECT_EQ((x + 1) * (y + 1) * 255, added_pixels[y][x][3]);
    }
  }
}

TEST_F(libyuvTest, TestARGBGray) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  EXPECT_EQ(27u, orig_pixels[0][0]);
  EXPECT_EQ(27u, orig_pixels[0][1]);
  EXPECT_EQ(27u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(151u, orig_pixels[1][0]);
  EXPECT_EQ(151u, orig_pixels[1][1]);
  EXPECT_EQ(151u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(75u, orig_pixels[2][0]);
  EXPECT_EQ(75u, orig_pixels[2][1]);
  EXPECT_EQ(75u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(96u, orig_pixels[3][0]);
  EXPECT_EQ(96u, orig_pixels[3][1]);
  EXPECT_EQ(96u, orig_pixels[3][2]);
  EXPECT_EQ(224u, orig_pixels[3][3]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBGray(&orig_pixels[0][0], 0, 0, 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestARGBGrayTo) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);
  SIMD_ALIGNED(uint8 gray_pixels[256][4]);

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 16, 1);
  EXPECT_EQ(27u, gray_pixels[0][0]);
  EXPECT_EQ(27u, gray_pixels[0][1]);
  EXPECT_EQ(27u, gray_pixels[0][2]);
  EXPECT_EQ(128u, gray_pixels[0][3]);
  EXPECT_EQ(151u, gray_pixels[1][0]);
  EXPECT_EQ(151u, gray_pixels[1][1]);
  EXPECT_EQ(151u, gray_pixels[1][2]);
  EXPECT_EQ(0u, gray_pixels[1][3]);
  EXPECT_EQ(75u, gray_pixels[2][0]);
  EXPECT_EQ(75u, gray_pixels[2][1]);
  EXPECT_EQ(75u, gray_pixels[2][2]);
  EXPECT_EQ(255u, gray_pixels[2][3]);
  EXPECT_EQ(96u, gray_pixels[3][0]);
  EXPECT_EQ(96u, gray_pixels[3][1]);
  EXPECT_EQ(96u, gray_pixels[3][2]);
  EXPECT_EQ(224u, gray_pixels[3][3]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestARGBSepia) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  EXPECT_EQ(33u, orig_pixels[0][0]);
  EXPECT_EQ(43u, orig_pixels[0][1]);
  EXPECT_EQ(47u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(135u, orig_pixels[1][0]);
  EXPECT_EQ(175u, orig_pixels[1][1]);
  EXPECT_EQ(195u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(69u, orig_pixels[2][0]);
  EXPECT_EQ(89u, orig_pixels[2][1]);
  EXPECT_EQ(99u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(88u, orig_pixels[3][0]);
  EXPECT_EQ(114u, orig_pixels[3][1]);
  EXPECT_EQ(127u, orig_pixels[3][2]);
  EXPECT_EQ(224u, orig_pixels[3][3]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestARGBColorMatrix) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);

  // Matrix for Sepia.
  static const int8 kARGBToSepia[] = {
    17, 68, 35, 0,
    22, 88, 45, 0,
    24, 98, 50, 0,
  };

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBColorMatrix(&orig_pixels[0][0], 0, &kARGBToSepia[0], 0, 0, 16, 1);
  EXPECT_EQ(33u, orig_pixels[0][0]);
  EXPECT_EQ(43u, orig_pixels[0][1]);
  EXPECT_EQ(47u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(135u, orig_pixels[1][0]);
  EXPECT_EQ(175u, orig_pixels[1][1]);
  EXPECT_EQ(195u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(69u, orig_pixels[2][0]);
  EXPECT_EQ(89u, orig_pixels[2][1]);
  EXPECT_EQ(99u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(88u, orig_pixels[3][0]);
  EXPECT_EQ(114u, orig_pixels[3][1]);
  EXPECT_EQ(127u, orig_pixels[3][2]);
  EXPECT_EQ(224u, orig_pixels[3][3]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBColorMatrix(&orig_pixels[0][0], 0, &kARGBToSepia[0], 0, 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestARGBColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8 kARGBTable[256 * 4] = {
    1u, 2u, 3u, 4u,
    5u, 6u, 7u, 8u,
    9u, 10u, 11u, 12u,
    13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  EXPECT_EQ(1u, orig_pixels[0][0]);
  EXPECT_EQ(2u, orig_pixels[0][1]);
  EXPECT_EQ(3u, orig_pixels[0][2]);
  EXPECT_EQ(4u, orig_pixels[0][3]);
  EXPECT_EQ(5u, orig_pixels[1][0]);
  EXPECT_EQ(6u, orig_pixels[1][1]);
  EXPECT_EQ(7u, orig_pixels[1][2]);
  EXPECT_EQ(8u, orig_pixels[1][3]);
  EXPECT_EQ(9u, orig_pixels[2][0]);
  EXPECT_EQ(10u, orig_pixels[2][1]);
  EXPECT_EQ(11u, orig_pixels[2][2]);
  EXPECT_EQ(12u, orig_pixels[2][3]);
  EXPECT_EQ(1u, orig_pixels[3][0]);
  EXPECT_EQ(6u, orig_pixels[3][1]);
  EXPECT_EQ(11u, orig_pixels[3][2]);
  EXPECT_EQ(16u, orig_pixels[3][3]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestARGBQuantize) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  ARGBQuantize(&orig_pixels[0][0], 0,
               (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0, 256, 1);

  for (int i = 0; i < 256; ++i) {
    EXPECT_EQ(i / 8 * 8 + 8 / 2, orig_pixels[i][0]);
    EXPECT_EQ(i / 2 / 8 * 8 + 8 / 2, orig_pixels[i][1]);
    EXPECT_EQ(i / 3 / 8 * 8 + 8 / 2, orig_pixels[i][2]);
    EXPECT_EQ(i, orig_pixels[i][3]);
  }
  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBQuantize(&orig_pixels[0][0], 0,
                 (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestARGBMirror) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);
  SIMD_ALIGNED(uint8 dst_pixels[256][4]);

  for (int i = 0; i < 256; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i / 4;
  }
  ARGBMirror(&orig_pixels[0][0], 0, &dst_pixels[0][0], 0, 256, 1);

  for (int i = 0; i < 256; ++i) {
    EXPECT_EQ(i, dst_pixels[255 - i][0]);
    EXPECT_EQ(i / 2, dst_pixels[255 - i][1]);
    EXPECT_EQ(i / 3, dst_pixels[255 - i][2]);
    EXPECT_EQ(i / 4, dst_pixels[255 - i][3]);
  }
  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBMirror(&orig_pixels[0][0], 0, &dst_pixels[0][0], 0, 256, 1);
  }
}

TEST_F(libyuvTest, TestShade) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);
  SIMD_ALIGNED(uint8 shade_pixels[256][4]);

  orig_pixels[0][0] = 10u;
  orig_pixels[0][1] = 20u;
  orig_pixels[0][2] = 40u;
  orig_pixels[0][3] = 80u;
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 0u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 255u;
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 0u;
  orig_pixels[2][3] = 0u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 0u;
  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 4, 1, 0x80ffffff);
  EXPECT_EQ(10u, shade_pixels[0][0]);
  EXPECT_EQ(20u, shade_pixels[0][1]);
  EXPECT_EQ(40u, shade_pixels[0][2]);
  EXPECT_EQ(40u, shade_pixels[0][3]);
  EXPECT_EQ(0u, shade_pixels[1][0]);
  EXPECT_EQ(0u, shade_pixels[1][1]);
  EXPECT_EQ(0u, shade_pixels[1][2]);
  EXPECT_EQ(128u, shade_pixels[1][3]);
  EXPECT_EQ(0u, shade_pixels[2][0]);
  EXPECT_EQ(0u, shade_pixels[2][1]);
  EXPECT_EQ(0u, shade_pixels[2][2]);
  EXPECT_EQ(0u, shade_pixels[2][3]);
  EXPECT_EQ(0u, shade_pixels[3][0]);
  EXPECT_EQ(0u, shade_pixels[3][1]);
  EXPECT_EQ(0u, shade_pixels[3][2]);
  EXPECT_EQ(0u, shade_pixels[3][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 4, 1, 0x80808080);
  EXPECT_EQ(5u, shade_pixels[0][0]);
  EXPECT_EQ(10u, shade_pixels[0][1]);
  EXPECT_EQ(20u, shade_pixels[0][2]);
  EXPECT_EQ(40u, shade_pixels[0][3]);

  for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
    ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 256, 1,
              0x80808080);
  }
}

TEST_F(libyuvTest, TestInterpolate) {
  SIMD_ALIGNED(uint8 orig_pixels_0[256][4]);
  SIMD_ALIGNED(uint8 orig_pixels_1[256][4]);
  SIMD_ALIGNED(uint8 interpolate_pixels[256][4]);

  orig_pixels_0[0][0] = 16u;
  orig_pixels_0[0][1] = 32u;
  orig_pixels_0[0][2] = 64u;
  orig_pixels_0[0][3] = 128u;
  orig_pixels_0[1][0] = 0u;
  orig_pixels_0[1][1] = 0u;
  orig_pixels_0[1][2] = 0u;
  orig_pixels_0[1][3] = 255u;
  orig_pixels_0[2][0] = 0u;
  orig_pixels_0[2][1] = 0u;
  orig_pixels_0[2][2] = 0u;
  orig_pixels_0[2][3] = 0u;
  orig_pixels_0[3][0] = 0u;
  orig_pixels_0[3][1] = 0u;
  orig_pixels_0[3][2] = 0u;
  orig_pixels_0[3][3] = 0u;

  orig_pixels_1[0][0] = 0u;
  orig_pixels_1[0][1] = 0u;
  orig_pixels_1[0][2] = 0u;
  orig_pixels_1[0][3] = 0u;
  orig_pixels_1[1][0] = 0u;
  orig_pixels_1[1][1] = 0u;
  orig_pixels_1[1][2] = 0u;
  orig_pixels_1[1][3] = 0u;
  orig_pixels_1[2][0] = 0u;
  orig_pixels_1[2][1] = 0u;
  orig_pixels_1[2][2] = 0u;
  orig_pixels_1[2][3] = 0u;
  orig_pixels_1[3][0] = 255u;
  orig_pixels_1[3][1] = 255u;
  orig_pixels_1[3][2] = 255u;
  orig_pixels_1[3][3] = 255u;

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 128);
  EXPECT_EQ(8u, interpolate_pixels[0][0]);
  EXPECT_EQ(16u, interpolate_pixels[0][1]);
  EXPECT_EQ(32u, interpolate_pixels[0][2]);
  EXPECT_EQ(64u, interpolate_pixels[0][3]);
  EXPECT_EQ(0u, interpolate_pixels[1][0]);
  EXPECT_EQ(0u, interpolate_pixels[1][1]);
  EXPECT_EQ(0u, interpolate_pixels[1][2]);
  EXPECT_NEAR(128u, interpolate_pixels[1][3], 1);  // C = 127, SSE = 128.
  EXPECT_EQ(0u, interpolate_pixels[2][0]);
  EXPECT_EQ(0u, interpolate_pixels[2][1]);
  EXPECT_EQ(0u, interpolate_pixels[2][2]);
  EXPECT_EQ(0u, interpolate_pixels[2][3]);
  EXPECT_NEAR(128u, interpolate_pixels[3][0], 1);
  EXPECT_NEAR(128u, interpolate_pixels[3][1], 1);
  EXPECT_NEAR(128u, interpolate_pixels[3][2], 1);
  EXPECT_NEAR(128u, interpolate_pixels[3][3], 1);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 0);
  EXPECT_EQ(16u, interpolate_pixels[0][0]);
  EXPECT_EQ(32u, interpolate_pixels[0][1]);
  EXPECT_EQ(64u, interpolate_pixels[0][2]);
  EXPECT_EQ(128u, interpolate_pixels[0][3]);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 192);

  EXPECT_EQ(4u, interpolate_pixels[0][0]);
  EXPECT_EQ(8u, interpolate_pixels[0][1]);
  EXPECT_EQ(16u, interpolate_pixels[0][2]);
  EXPECT_EQ(32u, interpolate_pixels[0][3]);

  for (int i = 0; i < benchmark_iterations_ * (1280 * 720 / 256); ++i) {
    ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                    &interpolate_pixels[0][0], 0, 256, 1, 128);
  }
}

TEST_F(libyuvTest, TestAffine) {
  SIMD_ALIGNED(uint8 orig_pixels_0[256][4]);
  SIMD_ALIGNED(uint8 interpolate_pixels_C[256][4]);
#if defined(HAS_ARGBAFFINEROW_SSE2)
  SIMD_ALIGNED(uint8 interpolate_pixels_Opt[256][4]);
#endif

  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 4; ++j) {
      orig_pixels_0[i][j] = i;
    }
  }

  float uv_step[4] = { 0.f, 0.f, 0.75f, 0.f };

  ARGBAffineRow_C(&orig_pixels_0[0][0], 0, &interpolate_pixels_C[0][0],
                  uv_step, 256);
  EXPECT_EQ(0u, interpolate_pixels_C[0][0]);
  EXPECT_EQ(96u, interpolate_pixels_C[128][0]);
  EXPECT_EQ(191u, interpolate_pixels_C[255][3]);

#if defined(HAS_ARGBAFFINEROW_SSE2)
  ARGBAffineRow_SSE2(&orig_pixels_0[0][0], 0, &interpolate_pixels_Opt[0][0],
                     uv_step, 256);
  EXPECT_EQ(0, memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 256 * 4));
#endif

#if defined(HAS_ARGBAFFINEROW_SSE2)
  int has_sse2 = TestCpuFlag(kCpuHasSSE2);
  if (has_sse2) {
    for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
      ARGBAffineRow_SSE2(&orig_pixels_0[0][0], 0, &interpolate_pixels_Opt[0][0],
                         uv_step, 256);
    }
  } else {
#endif
    for (int i = 0; i < benchmark_iterations_ * 1280 * 720 / 256; ++i) {
      ARGBAffineRow_C(&orig_pixels_0[0][0], 0, &interpolate_pixels_C[0][0],
                      uv_step, 256);
    }
#if defined(HAS_ARGBAFFINEROW_SSE2)
  }
#endif
}

}  // namespace libyuv
