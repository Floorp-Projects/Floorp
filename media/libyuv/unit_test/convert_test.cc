/*
 *  Copyright 2011 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "libyuv/compare.h"
#include "libyuv/convert.h"
#include "libyuv/convert_argb.h"
#include "libyuv/convert_from.h"
#include "libyuv/convert_from_argb.h"
#include "libyuv/cpu_id.h"
#include "libyuv/format_conversion.h"
#ifdef HAVE_JPEG
#include "libyuv/mjpeg_decoder.h"
#endif
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/row.h"
#include "../unit_test/unit_test.h"

#if defined(_MSC_VER)
#define SIMD_ALIGNED(var) __declspec(align(16)) var
#else  // __GNUC__
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
#endif

namespace libyuv {

#define SUBSAMPLE(v, a) ((((v) + (a) - 1)) / (a))

#define TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,           \
                       FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, W1280, N, NEG, OFF)   \
TEST_F(libyuvTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {                        \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  align_buffer_64(src_y, kWidth * kHeight + OFF);                              \
  align_buffer_64(src_u,                                                       \
                  SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *                           \
                  SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) + OFF);                    \
  align_buffer_64(src_v,                                                       \
                  SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *                           \
                  SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) + OFF);                    \
  align_buffer_64(dst_y_c, kWidth * kHeight);                                  \
  align_buffer_64(dst_u_c,                                                     \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_v_c,                                                     \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_y_opt, kWidth * kHeight);                                \
  align_buffer_64(dst_u_opt,                                                   \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_v_opt,                                                   \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kWidth; ++j)                                           \
      src_y[(i * kWidth) + j + OFF] = (random() & 0xff);                       \
  for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {                \
    for (int j = 0; j < SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {               \
      src_u[(i * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =                \
          (random() & 0xff);                                                   \
      src_v[(i * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =                \
          (random() & 0xff);                                                   \
    }                                                                          \
  }                                                                            \
  memset(dst_y_c, 1, kWidth * kHeight);                                        \
  memset(dst_u_c, 2, SUBSAMPLE(kWidth, SUBSAMP_X) *                            \
                     SUBSAMPLE(kHeight, SUBSAMP_Y));                           \
  memset(dst_v_c, 3, SUBSAMPLE(kWidth, SUBSAMP_X) *                            \
                     SUBSAMPLE(kHeight, SUBSAMP_Y));                           \
  memset(dst_y_opt, 101, kWidth * kHeight);                                    \
  memset(dst_u_opt, 102, SUBSAMPLE(kWidth, SUBSAMP_X) *                        \
                         SUBSAMPLE(kHeight, SUBSAMP_Y));                       \
  memset(dst_v_opt, 103, SUBSAMPLE(kWidth, SUBSAMP_X) *                        \
                         SUBSAMPLE(kHeight, SUBSAMP_Y));                       \
  MaskCpuFlags(0);                                                             \
  SRC_FMT_PLANAR##To##FMT_PLANAR(src_y + OFF, kWidth,                          \
                                 src_u + OFF,                                  \
                                 SUBSAMPLE(kWidth, SRC_SUBSAMP_X),             \
                                 src_v + OFF,                                  \
                                 SUBSAMPLE(kWidth, SRC_SUBSAMP_X),             \
                                 dst_y_c, kWidth,                              \
                                 dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X),        \
                                 dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X),        \
                                 kWidth, NEG kHeight);                         \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    SRC_FMT_PLANAR##To##FMT_PLANAR(src_y + OFF, kWidth,                        \
                                   src_u + OFF,                                \
                                       SUBSAMPLE(kWidth, SRC_SUBSAMP_X),       \
                                   src_v + OFF,                                \
                                       SUBSAMPLE(kWidth, SRC_SUBSAMP_X),       \
                                   dst_y_opt, kWidth,                          \
                                   dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X),    \
                                   dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X),    \
                                   kWidth, NEG kHeight);                       \
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
  EXPECT_LE(max_diff, 0);                                                      \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                   \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_u_c[i *                                     \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -            \
              static_cast<int>(dst_u_opt[i *                                   \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 3);                                                      \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                   \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_v_c[i *                                     \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -            \
              static_cast<int>(dst_v_opt[i *                                   \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 3);                                                      \
  free_aligned_buffer_64(dst_y_c);                                             \
  free_aligned_buffer_64(dst_u_c);                                             \
  free_aligned_buffer_64(dst_v_c);                                             \
  free_aligned_buffer_64(dst_y_opt);                                           \
  free_aligned_buffer_64(dst_u_opt);                                           \
  free_aligned_buffer_64(dst_v_opt);                                           \
  free_aligned_buffer_64(src_y);                                               \
  free_aligned_buffer_64(src_u);                                               \
  free_aligned_buffer_64(src_v);                                               \
}

#define TESTPLANARTOP(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,            \
                      FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)                        \
    TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,               \
                   FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                           \
                   benchmark_width_ - 4, _Any, +, 0)                           \
    TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,               \
                   FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                           \
                   benchmark_width_, _Unaligned, +, 1)                         \
    TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,               \
                   FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                           \
                   benchmark_width_, _Invert, -, 0)                            \
    TESTPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,               \
                   FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                           \
                   benchmark_width_, _Opt, +, 0)

TESTPLANARTOP(I420, 2, 2, I420, 2, 2)
TESTPLANARTOP(I422, 2, 1, I420, 2, 2)
TESTPLANARTOP(I444, 1, 1, I420, 2, 2)
TESTPLANARTOP(I411, 4, 1, I420, 2, 2)
TESTPLANARTOP(I420, 2, 2, I422, 2, 1)
TESTPLANARTOP(I420, 2, 2, I444, 1, 1)
TESTPLANARTOP(I420, 2, 2, I411, 4, 1)
TESTPLANARTOP(I420, 2, 2, I420Mirror, 2, 2)
TESTPLANARTOP(I422, 2, 1, I422, 2, 1)
TESTPLANARTOP(I444, 1, 1, I444, 1, 1)

#define TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,          \
                       FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, W1280, N, NEG, OFF)   \
TEST_F(libyuvTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {                        \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  align_buffer_64(src_y, kWidth * kHeight + OFF);                              \
  align_buffer_64(src_u,                                                       \
                  SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *                           \
                  SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) + OFF);                    \
  align_buffer_64(src_v,                                                       \
                  SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *                           \
                  SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) + OFF);                    \
  align_buffer_64(dst_y_c, kWidth * kHeight);                                  \
  align_buffer_64(dst_uv_c, SUBSAMPLE(kWidth * 2, SUBSAMP_X) *                 \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_y_opt, kWidth * kHeight);                                \
  align_buffer_64(dst_uv_opt, SUBSAMPLE(kWidth * 2, SUBSAMP_X) *               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kWidth; ++j)                                           \
      src_y[(i * kWidth) + j + OFF] = (random() & 0xff);                       \
  for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {                \
    for (int j = 0; j < SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {               \
      src_u[(i * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =                \
          (random() & 0xff);                                                   \
      src_v[(i * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =                \
          (random() & 0xff);                                                   \
    }                                                                          \
  }                                                                            \
  memset(dst_y_c, 1, kWidth * kHeight);                                        \
  memset(dst_uv_c, 2, SUBSAMPLE(kWidth * 2, SUBSAMP_X) *                       \
                      SUBSAMPLE(kHeight, SUBSAMP_Y));                          \
  memset(dst_y_opt, 101, kWidth * kHeight);                                    \
  memset(dst_uv_opt, 102, SUBSAMPLE(kWidth * 2, SUBSAMP_X) *                   \
                          SUBSAMPLE(kHeight, SUBSAMP_Y));                      \
  MaskCpuFlags(0);                                                             \
  SRC_FMT_PLANAR##To##FMT_PLANAR(src_y + OFF, kWidth,                          \
                                 src_u + OFF,                                  \
                                 SUBSAMPLE(kWidth, SRC_SUBSAMP_X),             \
                                 src_v + OFF,                                  \
                                 SUBSAMPLE(kWidth, SRC_SUBSAMP_X),             \
                                 dst_y_c, kWidth,                              \
                                 dst_uv_c, SUBSAMPLE(kWidth * 2, SUBSAMP_X),   \
                                 kWidth, NEG kHeight);                         \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    SRC_FMT_PLANAR##To##FMT_PLANAR(src_y + OFF, kWidth,                        \
                                   src_u + OFF,                                \
                                   SUBSAMPLE(kWidth, SRC_SUBSAMP_X),           \
                                   src_v + OFF,                                \
                                   SUBSAMPLE(kWidth, SRC_SUBSAMP_X),           \
                                   dst_y_opt, kWidth,                          \
                                   dst_uv_opt,                                 \
                                   SUBSAMPLE(kWidth * 2, SUBSAMP_X),           \
                                   kWidth, NEG kHeight);                       \
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
  EXPECT_LE(max_diff, 1);                                                      \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth * 2, SUBSAMP_X); ++j) {               \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_uv_c[i *                                    \
                               SUBSAMPLE(kWidth * 2, SUBSAMP_X) + j]) -        \
              static_cast<int>(dst_uv_opt[i *                                  \
                               SUBSAMPLE(kWidth * 2, SUBSAMP_X) + j]));        \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 1);                                                      \
  free_aligned_buffer_64(dst_y_c);                                             \
  free_aligned_buffer_64(dst_uv_c);                                            \
  free_aligned_buffer_64(dst_y_opt);                                           \
  free_aligned_buffer_64(dst_uv_opt);                                          \
  free_aligned_buffer_64(src_y);                                               \
  free_aligned_buffer_64(src_u);                                               \
  free_aligned_buffer_64(src_v);                                               \
}

#define TESTPLANARTOBP(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,           \
                       FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)                       \
    TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,              \
                    FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                          \
                    benchmark_width_ - 4, _Any, +, 0)                          \
    TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,              \
                    FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                          \
                    benchmark_width_, _Unaligned, +, 1)                        \
    TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,              \
                    FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                          \
                    benchmark_width_, _Invert, -, 0)                           \
    TESTPLANARTOBPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,              \
                    FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                          \
                    benchmark_width_, _Opt, +, 0)

TESTPLANARTOBP(I420, 2, 2, NV12, 2, 2)
TESTPLANARTOBP(I420, 2, 2, NV21, 2, 2)

#define TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,         \
                         FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, W1280, N, NEG, OFF) \
TEST_F(libyuvTest, SRC_FMT_PLANAR##To##FMT_PLANAR##N) {                        \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  align_buffer_64(src_y, kWidth * kHeight + OFF);                              \
  align_buffer_64(src_uv, 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X) *               \
                  SUBSAMPLE(kHeight, SRC_SUBSAMP_Y) + OFF);                    \
  align_buffer_64(dst_y_c, kWidth * kHeight);                                  \
  align_buffer_64(dst_u_c,                                                     \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_v_c,                                                     \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_y_opt, kWidth * kHeight);                                \
  align_buffer_64(dst_u_opt,                                                   \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_v_opt,                                                   \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kWidth; ++j)                                           \
      src_y[(i * kWidth) + j + OFF] = (random() & 0xff);                       \
  for (int i = 0; i < SUBSAMPLE(kHeight, SRC_SUBSAMP_Y); ++i) {                \
    for (int j = 0; j < 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X); ++j) {           \
      src_uv[(i * 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X)) + j + OFF] =           \
          (random() & 0xff);                                                   \
    }                                                                          \
  }                                                                            \
  memset(dst_y_c, 1, kWidth * kHeight);                                        \
  memset(dst_u_c, 2, SUBSAMPLE(kWidth, SUBSAMP_X) *                            \
                     SUBSAMPLE(kHeight, SUBSAMP_Y));                           \
  memset(dst_v_c, 3, SUBSAMPLE(kWidth, SUBSAMP_X) *                            \
                     SUBSAMPLE(kHeight, SUBSAMP_Y));                           \
  memset(dst_y_opt, 101, kWidth * kHeight);                                    \
  memset(dst_u_opt, 102, SUBSAMPLE(kWidth, SUBSAMP_X) *                        \
                         SUBSAMPLE(kHeight, SUBSAMP_Y));                       \
  memset(dst_v_opt, 103, SUBSAMPLE(kWidth, SUBSAMP_X) *                        \
                         SUBSAMPLE(kHeight, SUBSAMP_Y));                       \
  MaskCpuFlags(0);                                                             \
  SRC_FMT_PLANAR##To##FMT_PLANAR(src_y + OFF, kWidth,                          \
                                 src_uv + OFF,                                 \
                                 2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X),         \
                                 dst_y_c, kWidth,                              \
                                 dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X),        \
                                 dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X),        \
                                 kWidth, NEG kHeight);                         \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    SRC_FMT_PLANAR##To##FMT_PLANAR(src_y + OFF, kWidth,                        \
                                   src_uv + OFF,                               \
                                   2 * SUBSAMPLE(kWidth, SRC_SUBSAMP_X),       \
                                   dst_y_opt, kWidth,                          \
                                   dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X),    \
                                   dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X),    \
                                   kWidth, NEG kHeight);                       \
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
  EXPECT_LE(max_diff, 1);                                                      \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                   \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_u_c[i *                                     \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -            \
              static_cast<int>(dst_u_opt[i *                                   \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 1);                                                      \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                   \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_v_c[i *                                     \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -            \
              static_cast<int>(dst_v_opt[i *                                   \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 1);                                                      \
  free_aligned_buffer_64(dst_y_c);                                             \
  free_aligned_buffer_64(dst_u_c);                                             \
  free_aligned_buffer_64(dst_v_c);                                             \
  free_aligned_buffer_64(dst_y_opt);                                           \
  free_aligned_buffer_64(dst_u_opt);                                           \
  free_aligned_buffer_64(dst_v_opt);                                           \
  free_aligned_buffer_64(src_y);                                               \
  free_aligned_buffer_64(src_uv);                                              \
}

#define TESTBIPLANARTOP(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,          \
                        FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)                      \
    TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,             \
                     FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                         \
                     benchmark_width_ - 4, _Any, +, 0)                         \
    TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,             \
                     FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                         \
                     benchmark_width_, _Unaligned, +, 1)                       \
    TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,             \
                     FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                         \
                     benchmark_width_, _Invert, -, 0)                          \
    TESTBIPLANARTOPI(SRC_FMT_PLANAR, SRC_SUBSAMP_X, SRC_SUBSAMP_Y,             \
                     FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,                         \
                     benchmark_width_, _Opt, +, 0)

TESTBIPLANARTOP(NV12, 2, 2, I420, 2, 2)
TESTBIPLANARTOP(NV21, 2, 2, I420, 2, 2)

#define ALIGNINT(V, ALIGN) (((V) + (ALIGN) - 1) / (ALIGN) * (ALIGN))

#define TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,  \
                       YALIGN, W1280, DIFF, N, NEG, OFF, FMT_C, BPP_C)         \
TEST_F(libyuvTest, FMT_PLANAR##To##FMT_B##N) {                                 \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                     \
  const int kStrideB = ALIGNINT(kWidth * BPP_B, ALIGN);                        \
  const int kSizeUV =                                                          \
    SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y);              \
  align_buffer_64(src_y, kWidth * kHeight + OFF);                              \
  align_buffer_64(src_u, kSizeUV + OFF);                                       \
  align_buffer_64(src_v, kSizeUV + OFF);                                       \
  align_buffer_64(dst_argb_c, kStrideB * kHeight);                             \
  align_buffer_64(dst_argb_opt, kStrideB * kHeight);                           \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kWidth * kHeight; ++i) {                                 \
    src_y[i + OFF] = (random() & 0xff);                                        \
  }                                                                            \
  for (int i = 0; i < kSizeUV; ++i) {                                          \
    src_u[i + OFF] = (random() & 0xff);                                        \
    src_v[i + OFF] = (random() & 0xff);                                        \
  }                                                                            \
  memset(dst_argb_c, 1, kStrideB * kHeight);                                   \
  memset(dst_argb_opt, 101, kStrideB * kHeight);                               \
  MaskCpuFlags(0);                                                             \
  FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth,                                   \
                        src_u + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),             \
                        src_v + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),             \
                        dst_argb_c, kStrideB,                                  \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth,                                 \
                          src_u + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),           \
                          src_v + OFF, SUBSAMPLE(kWidth, SUBSAMP_X),           \
                          dst_argb_opt, kStrideB,                              \
                          kWidth, NEG kHeight);                                \
  }                                                                            \
  int max_diff = 0;                                                            \
  /* Convert to ARGB so 565 is expanded to bytes that can be compared. */      \
  align_buffer_64(dst_argb32_c, kWidth * BPP_C  * kHeight);                    \
  align_buffer_64(dst_argb32_opt, kWidth * BPP_C  * kHeight);                  \
  memset(dst_argb32_c, 2, kWidth * BPP_C  * kHeight);                          \
  memset(dst_argb32_opt, 102, kWidth * BPP_C  * kHeight);                      \
  FMT_B##To##FMT_C(dst_argb_c, kStrideB,                                       \
                   dst_argb32_c, kWidth * BPP_C ,                              \
                   kWidth, kHeight);                                           \
  FMT_B##To##FMT_C(dst_argb_opt, kStrideB,                                     \
                   dst_argb32_opt, kWidth * BPP_C ,                            \
                   kWidth, kHeight);                                           \
  for (int i = 0; i < kWidth * BPP_C * kHeight; ++i) {                         \
    int abs_diff =                                                             \
        abs(static_cast<int>(dst_argb32_c[i]) -                                \
            static_cast<int>(dst_argb32_opt[i]));                              \
    if (abs_diff > max_diff) {                                                 \
      max_diff = abs_diff;                                                     \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, DIFF);                                                   \
  free_aligned_buffer_64(src_y);                                               \
  free_aligned_buffer_64(src_u);                                               \
  free_aligned_buffer_64(src_v);                                               \
  free_aligned_buffer_64(dst_argb_c);                                          \
  free_aligned_buffer_64(dst_argb_opt);                                        \
  free_aligned_buffer_64(dst_argb32_c);                                        \
  free_aligned_buffer_64(dst_argb32_opt);                                      \
}

#define TESTPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,   \
                      YALIGN, DIFF, FMT_C, BPP_C)                              \
    TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
        YALIGN, benchmark_width_ - 4, DIFF, _Any, +, 0, FMT_C, BPP_C)          \
    TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
        YALIGN, benchmark_width_, DIFF, _Unaligned, +, 1, FMT_C, BPP_C)        \
    TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
        YALIGN, benchmark_width_, DIFF, _Invert, -, 0, FMT_C, BPP_C)           \
    TESTPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, ALIGN,      \
        YALIGN, benchmark_width_, DIFF, _Opt, +, 0, FMT_C, BPP_C)

// TODO(fbarchard): Make vertical alignment unnecessary on bayer.
TESTPLANARTOB(I420, 2, 2, ARGB, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, BGRA, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, ABGR, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, RGBA, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, RAW, 3, 3, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, RGB24, 3, 3, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, RGB565, 2, 2, 1, 9, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, ARGB1555, 2, 2, 1, 9, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, ARGB4444, 2, 2, 1, 17, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, ARGB, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, BGRA, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, ABGR, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, RGBA, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I411, 4, 1, ARGB, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I444, 1, 1, ARGB, 4, 4, 1, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, YUY2, 2, 4, 1, 1, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, UYVY, 2, 4, 1, 1, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, YUY2, 2, 4, 1, 0, ARGB, 4)
TESTPLANARTOB(I422, 2, 1, UYVY, 2, 4, 1, 0, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, I400, 1, 1, 1, 0, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, BayerBGGR, 1, 2, 2, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, BayerRGGB, 1, 2, 2, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, BayerGBRG, 1, 2, 2, 2, ARGB, 4)
TESTPLANARTOB(I420, 2, 2, BayerGRBG, 1, 2, 2, 2, ARGB, 4)

#define TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,       \
                         W1280, DIFF, N, NEG, OFF)                             \
TEST_F(libyuvTest, FMT_PLANAR##To##FMT_B##N) {                                 \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  const int kStrideB = kWidth * BPP_B;                                         \
  align_buffer_64(src_y, kWidth * kHeight + OFF);                              \
  align_buffer_64(src_uv,                                                      \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y) * 2 + OFF);                    \
  align_buffer_64(dst_argb_c, kStrideB * kHeight);                             \
  align_buffer_64(dst_argb_opt, kStrideB * kHeight);                           \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kWidth; ++j)                                           \
      src_y[(i * kWidth) + j + OFF] = (random() & 0xff);                       \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X) * 2; ++j) {               \
      src_uv[(i * SUBSAMPLE(kWidth, SUBSAMP_X)) * 2 + j + OFF] =               \
          (random() & 0xff);                                                   \
    }                                                                          \
  }                                                                            \
  memset(dst_argb_c, 1, kStrideB * kHeight);                                   \
  memset(dst_argb_opt, 101, kStrideB * kHeight);                               \
  MaskCpuFlags(0);                                                             \
  FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth,                                   \
                        src_uv + OFF, SUBSAMPLE(kWidth, SUBSAMP_X) * 2,        \
                        dst_argb_c, kWidth * BPP_B,                            \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_PLANAR##To##FMT_B(src_y + OFF, kWidth,                                 \
                          src_uv + OFF, SUBSAMPLE(kWidth, SUBSAMP_X) * 2,      \
                          dst_argb_opt, kWidth * BPP_B,                        \
                          kWidth, NEG kHeight);                                \
  }                                                                            \
  /* Convert to ARGB so 565 is expanded to bytes that can be compared. */      \
  align_buffer_64(dst_argb32_c, kWidth * 4 * kHeight);                         \
  align_buffer_64(dst_argb32_opt, kWidth * 4 * kHeight);                       \
  memset(dst_argb32_c, 2, kWidth * 4 * kHeight);                               \
  memset(dst_argb32_opt, 102, kWidth * 4 * kHeight);                           \
  FMT_B##ToARGB(dst_argb_c, kStrideB,                                          \
                dst_argb32_c, kWidth * 4,                                      \
                kWidth, kHeight);                                              \
  FMT_B##ToARGB(dst_argb_opt, kStrideB,                                        \
                dst_argb32_opt, kWidth * 4,                                    \
                kWidth, kHeight);                                              \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kHeight; ++i) {                                          \
    for (int j = 0; j < kWidth * 4; ++j) {                                     \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_argb32_c[i * kWidth * 4 + j]) -             \
              static_cast<int>(dst_argb32_opt[i * kWidth * 4 + j]));           \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, DIFF);                                                   \
  free_aligned_buffer_64(src_y);                                               \
  free_aligned_buffer_64(src_uv);                                              \
  free_aligned_buffer_64(dst_argb_c);                                          \
  free_aligned_buffer_64(dst_argb_opt);                                        \
  free_aligned_buffer_64(dst_argb32_c);                                        \
  free_aligned_buffer_64(dst_argb32_opt);                                      \
}

#define TESTBIPLANARTOB(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B, DIFF)  \
    TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,           \
                     benchmark_width_ - 4, DIFF, _Any, +, 0)                   \
    TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,           \
                     benchmark_width_, DIFF, _Unaligned, +, 1)                 \
    TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,           \
                     benchmark_width_, DIFF, _Invert, -, 0)                    \
    TESTBIPLANARTOBI(FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, FMT_B, BPP_B,           \
                     benchmark_width_, DIFF, _Opt, +, 0)

TESTBIPLANARTOB(NV12, 2, 2, ARGB, 4, 2)
TESTBIPLANARTOB(NV21, 2, 2, ARGB, 4, 2)
TESTBIPLANARTOB(NV12, 2, 2, RGB565, 2, 9)
TESTBIPLANARTOB(NV21, 2, 2, RGB565, 2, 9)

#define TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y, \
                       W1280, DIFF, N, NEG, OFF)                               \
TEST_F(libyuvTest, FMT_A##To##FMT_PLANAR##N) {                                 \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = ALIGNINT(benchmark_height_, YALIGN);                     \
  const int kStride =                                                          \
      (SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMP_X * 8 * BPP_A + 7) / 8;          \
  align_buffer_64(src_argb, kStride * kHeight + OFF);                          \
  align_buffer_64(dst_y_c, kWidth * kHeight);                                  \
  align_buffer_64(dst_u_c,                                                     \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_v_c,                                                     \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_y_opt, kWidth * kHeight);                                \
  align_buffer_64(dst_u_opt,                                                   \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_v_opt,                                                   \
                  SUBSAMPLE(kWidth, SUBSAMP_X) *                               \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  memset(dst_y_c, 1, kWidth * kHeight);                                        \
  memset(dst_u_c, 2,                                                           \
         SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
  memset(dst_v_c, 3,                                                           \
         SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
  memset(dst_y_opt, 101, kWidth * kHeight);                                    \
  memset(dst_u_opt, 102,                                                       \
         SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
  memset(dst_v_opt, 103,                                                       \
         SUBSAMPLE(kWidth, SUBSAMP_X) * SUBSAMPLE(kHeight, SUBSAMP_Y));        \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kStride; ++j)                                          \
      src_argb[(i * kStride) + j + OFF] = (random() & 0xff);                   \
  MaskCpuFlags(0);                                                             \
  FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride,                               \
                        dst_y_c, kWidth,                                       \
                        dst_u_c, SUBSAMPLE(kWidth, SUBSAMP_X),                 \
                        dst_v_c, SUBSAMPLE(kWidth, SUBSAMP_X),                 \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride,                             \
                          dst_y_opt, kWidth,                                   \
                          dst_u_opt, SUBSAMPLE(kWidth, SUBSAMP_X),             \
                          dst_v_opt, SUBSAMPLE(kWidth, SUBSAMP_X),             \
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
  EXPECT_LE(max_diff, DIFF);                                                   \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                   \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_u_c[i *                                     \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -            \
              static_cast<int>(dst_u_opt[i *                                   \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, DIFF);                                                   \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X); ++j) {                   \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_v_c[i *                                     \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]) -            \
              static_cast<int>(dst_v_opt[i *                                   \
                               SUBSAMPLE(kWidth, SUBSAMP_X) + j]));            \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, DIFF);                                                   \
  free_aligned_buffer_64(dst_y_c);                                             \
  free_aligned_buffer_64(dst_u_c);                                             \
  free_aligned_buffer_64(dst_v_c);                                             \
  free_aligned_buffer_64(dst_y_opt);                                           \
  free_aligned_buffer_64(dst_u_opt);                                           \
  free_aligned_buffer_64(dst_v_opt);                                           \
  free_aligned_buffer_64(src_argb);                                            \
}

#define TESTATOPLANAR(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,  \
                      DIFF)                                                    \
    TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,     \
                   benchmark_width_ - 4, DIFF, _Any, +, 0)                     \
    TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,     \
                   benchmark_width_, DIFF, _Unaligned, +, 1)                   \
    TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,     \
                   benchmark_width_, DIFF, _Invert, -, 0)                      \
    TESTATOPLANARI(FMT_A, BPP_A, YALIGN, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,     \
                   benchmark_width_, DIFF, _Opt, +, 0)

TESTATOPLANAR(ARGB, 4, 1, I420, 2, 2, 4)
#ifdef __arm__
TESTATOPLANAR(ARGB, 4, 1, J420, 2, 2, 4)
#else
TESTATOPLANAR(ARGB, 4, 1, J420, 2, 2, 0)
#endif
TESTATOPLANAR(BGRA, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(ABGR, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(RGBA, 4, 1, I420, 2, 2, 4)
TESTATOPLANAR(RAW, 3, 1, I420, 2, 2, 4)
TESTATOPLANAR(RGB24, 3, 1, I420, 2, 2, 4)
TESTATOPLANAR(RGB565, 2, 1, I420, 2, 2, 5)
// TODO(fbarchard): Make 1555 neon work same as C code, reduce to diff 9.
TESTATOPLANAR(ARGB1555, 2, 1, I420, 2, 2, 15)
TESTATOPLANAR(ARGB4444, 2, 1, I420, 2, 2, 17)
TESTATOPLANAR(ARGB, 4, 1, I411, 4, 1, 4)
TESTATOPLANAR(ARGB, 4, 1, I422, 2, 1, 2)
TESTATOPLANAR(ARGB, 4, 1, I444, 1, 1, 2)
TESTATOPLANAR(YUY2, 2, 1, I420, 2, 2, 2)
TESTATOPLANAR(UYVY, 2, 1, I420, 2, 2, 2)
TESTATOPLANAR(YUY2, 2, 1, I422, 2, 1, 2)
TESTATOPLANAR(UYVY, 2, 1, I422, 2, 1, 2)
TESTATOPLANAR(I400, 1, 1, I420, 2, 2, 2)
TESTATOPLANAR(BayerBGGR, 1, 2, I420, 2, 2, 4)
TESTATOPLANAR(BayerRGGB, 1, 2, I420, 2, 2, 4)
TESTATOPLANAR(BayerGBRG, 1, 2, I420, 2, 2, 4)
TESTATOPLANAR(BayerGRBG, 1, 2, I420, 2, 2, 4)

#define TESTATOBIPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,       \
                       W1280, N, NEG, OFF)                                     \
TEST_F(libyuvTest, FMT_A##To##FMT_PLANAR##N) {                                 \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  const int kStride = (kWidth * 8 * BPP_A + 7) / 8;                            \
  align_buffer_64(src_argb, kStride * kHeight + OFF);                          \
  align_buffer_64(dst_y_c, kWidth * kHeight);                                  \
  align_buffer_64(dst_uv_c,                                                    \
                  SUBSAMPLE(kWidth, SUBSAMP_X) * 2 *                           \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  align_buffer_64(dst_y_opt, kWidth * kHeight);                                \
  align_buffer_64(dst_uv_opt,                                                  \
                  SUBSAMPLE(kWidth, SUBSAMP_X) * 2 *                           \
                  SUBSAMPLE(kHeight, SUBSAMP_Y));                              \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kHeight; ++i)                                            \
    for (int j = 0; j < kStride; ++j)                                          \
      src_argb[(i * kStride) + j + OFF] = (random() & 0xff);                   \
  memset(dst_y_c, 1, kWidth * kHeight);                                        \
  memset(dst_uv_c, 2, SUBSAMPLE(kWidth, SUBSAMP_X) * 2 *                       \
                      SUBSAMPLE(kHeight, SUBSAMP_Y));                          \
  memset(dst_y_opt, 101, kWidth * kHeight);                                    \
  memset(dst_uv_opt, 102, SUBSAMPLE(kWidth, SUBSAMP_X) * 2 *                   \
                        SUBSAMPLE(kHeight, SUBSAMP_Y));                        \
  MaskCpuFlags(0);                                                             \
  FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride,                               \
                        dst_y_c, kWidth,                                       \
                        dst_uv_c, SUBSAMPLE(kWidth, SUBSAMP_X) * 2,            \
                        kWidth, NEG kHeight);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_A##To##FMT_PLANAR(src_argb + OFF, kStride,                             \
                          dst_y_opt, kWidth,                                   \
                          dst_uv_opt, SUBSAMPLE(kWidth, SUBSAMP_X) * 2,        \
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
  EXPECT_LE(max_diff, 4);                                                      \
  for (int i = 0; i < SUBSAMPLE(kHeight, SUBSAMP_Y); ++i) {                    \
    for (int j = 0; j < SUBSAMPLE(kWidth, SUBSAMP_X) * 2; ++j) {               \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_uv_c[i *                                    \
                               SUBSAMPLE(kWidth, SUBSAMP_X) * 2 + j]) -        \
              static_cast<int>(dst_uv_opt[i *                                  \
                               SUBSAMPLE(kWidth, SUBSAMP_X) * 2 + j]));        \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, 4);                                                      \
  free_aligned_buffer_64(dst_y_c);                                             \
  free_aligned_buffer_64(dst_uv_c);                                            \
  free_aligned_buffer_64(dst_y_opt);                                           \
  free_aligned_buffer_64(dst_uv_opt);                                          \
  free_aligned_buffer_64(src_argb);                                            \
}

#define TESTATOBIPLANAR(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y)        \
    TESTATOBIPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,           \
                   benchmark_width_ - 4, _Any, +, 0)                           \
    TESTATOBIPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,           \
                   benchmark_width_, _Unaligned, +, 1)                         \
    TESTATOBIPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,           \
                   benchmark_width_, _Invert, -, 0)                            \
    TESTATOBIPLANARI(FMT_A, BPP_A, FMT_PLANAR, SUBSAMP_X, SUBSAMP_Y,           \
                   benchmark_width_, _Opt, +, 0)

TESTATOBIPLANAR(ARGB, 4, NV12, 2, 2)
TESTATOBIPLANAR(ARGB, 4, NV21, 2, 2)

#define TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                            \
                  FMT_B, BPP_B, STRIDE_B, HEIGHT_B,                            \
                  W1280, DIFF, N, NEG, OFF)                                    \
TEST_F(libyuvTest, FMT_A##To##FMT_B##N) {                                      \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;         \
  const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;         \
  const int kStrideA = (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;  \
  const int kStrideB = (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;  \
  align_buffer_64(src_argb, kStrideA * kHeightA + OFF);                        \
  align_buffer_64(dst_argb_c, kStrideB * kHeightB);                            \
  align_buffer_64(dst_argb_opt, kStrideB * kHeightB);                          \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kStrideA * kHeightA; ++i) {                              \
    src_argb[i + OFF] = (random() & 0xff);                                     \
  }                                                                            \
  memset(dst_argb_c, 1, kStrideB * kHeightB);                                  \
  memset(dst_argb_opt, 101, kStrideB * kHeightB);                              \
  MaskCpuFlags(0);                                                             \
  FMT_A##To##FMT_B(src_argb + OFF, kStrideA,                                   \
                   dst_argb_c, kStrideB,                                       \
                   kWidth, NEG kHeight);                                       \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    FMT_A##To##FMT_B(src_argb + OFF, kStrideA,                                 \
                     dst_argb_opt, kStrideB,                                   \
                     kWidth, NEG kHeight);                                     \
  }                                                                            \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kStrideB * kHeightB; ++i) {                              \
    int abs_diff =                                                             \
        abs(static_cast<int>(dst_argb_c[i]) -                                  \
            static_cast<int>(dst_argb_opt[i]));                                \
    if (abs_diff > max_diff) {                                                 \
      max_diff = abs_diff;                                                     \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, DIFF);                                                   \
  free_aligned_buffer_64(src_argb);                                            \
  free_aligned_buffer_64(dst_argb_c);                                          \
  free_aligned_buffer_64(dst_argb_opt);                                        \
}

#define TESTATOBRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                       \
                       FMT_B, BPP_B, STRIDE_B, HEIGHT_B, DIFF)                 \
TEST_F(libyuvTest, FMT_A##To##FMT_B##_Random) {                                \
  srandom(time(NULL));                                                         \
  for (int times = 0; times < benchmark_iterations_; ++times) {                \
    const int kWidth = (random() & 63) + 1;                                    \
    const int kHeight = (random() & 31) + 1;                                   \
    const int kHeightA = (kHeight + HEIGHT_A - 1) / HEIGHT_A * HEIGHT_A;       \
    const int kHeightB = (kHeight + HEIGHT_B - 1) / HEIGHT_B * HEIGHT_B;       \
    const int kStrideA = (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;\
    const int kStrideB = (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;\
    align_buffer_page_end(src_argb, kStrideA * kHeightA);                      \
    align_buffer_page_end(dst_argb_c, kStrideB * kHeightB);                    \
    align_buffer_page_end(dst_argb_opt, kStrideB * kHeightB);                  \
    for (int i = 0; i < kStrideA * kHeightA; ++i) {                            \
      src_argb[i] = (random() & 0xff);                                         \
    }                                                                          \
    memset(dst_argb_c, 123, kStrideB * kHeightB);                              \
    memset(dst_argb_opt, 123, kStrideB * kHeightB);                            \
    MaskCpuFlags(0);                                                           \
    FMT_A##To##FMT_B(src_argb, kStrideA,                                       \
                     dst_argb_c, kStrideB,                                     \
                     kWidth, kHeight);                                         \
    MaskCpuFlags(-1);                                                          \
    FMT_A##To##FMT_B(src_argb, kStrideA,                                       \
                     dst_argb_opt, kStrideB,                                   \
                     kWidth, kHeight);                                         \
    int max_diff = 0;                                                          \
    for (int i = 0; i < kStrideB * kHeightB; ++i) {                            \
      int abs_diff =                                                           \
          abs(static_cast<int>(dst_argb_c[i]) -                                \
              static_cast<int>(dst_argb_opt[i]));                              \
      if (abs_diff > max_diff) {                                               \
        max_diff = abs_diff;                                                   \
      }                                                                        \
    }                                                                          \
    EXPECT_LE(max_diff, DIFF);                                                 \
    free_aligned_buffer_page_end(src_argb);                                    \
    free_aligned_buffer_page_end(dst_argb_c);                                  \
    free_aligned_buffer_page_end(dst_argb_opt);                                \
  }                                                                            \
}

#define TESTATOB(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                             \
                 FMT_B, BPP_B, STRIDE_B, HEIGHT_B, DIFF)                       \
    TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                                \
              FMT_B, BPP_B, STRIDE_B, HEIGHT_B,                                \
              benchmark_width_ - 4, DIFF, _Any, +, 0)                          \
    TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                                \
              FMT_B, BPP_B, STRIDE_B, HEIGHT_B,                                \
              benchmark_width_, DIFF, _Unaligned, +, 1)                        \
    TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                                \
              FMT_B, BPP_B, STRIDE_B, HEIGHT_B,                                \
              benchmark_width_, DIFF, _Invert, -, 0)                           \
    TESTATOBI(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                                \
              FMT_B, BPP_B, STRIDE_B, HEIGHT_B,                                \
              benchmark_width_, DIFF, _Opt, +, 0)                              \
    TESTATOBRANDOM(FMT_A, BPP_A, STRIDE_A, HEIGHT_A,                           \
                   FMT_B, BPP_B, STRIDE_B, HEIGHT_B, DIFF)

TESTATOB(ARGB, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, BGRA, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ABGR, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, RGBA, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, RAW, 3, 3, 1, 0)
TESTATOB(ARGB, 4, 4, 1, RGB24, 3, 3, 1, 0)
TESTATOB(ARGB, 4, 4, 1, RGB565, 2, 2, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ARGB1555, 2, 2, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ARGB4444, 2, 2, 1, 0)
TESTATOB(ARGB, 4, 4, 1, BayerBGGR, 1, 1, 1, 0)
TESTATOB(ARGB, 4, 4, 1, BayerRGGB, 1, 1, 1, 0)
TESTATOB(ARGB, 4, 4, 1, BayerGBRG, 1, 1, 1, 0)
TESTATOB(ARGB, 4, 4, 1, BayerGRBG, 1, 1, 1, 0)
TESTATOB(ARGB, 4, 4, 1, YUY2, 2, 4, 1, 4)
TESTATOB(ARGB, 4, 4, 1, UYVY, 2, 4, 1, 4)
TESTATOB(ARGB, 4, 4, 1, I400, 1, 1, 1, 2)
TESTATOB(ARGB, 4, 4, 1, J400, 1, 1, 1, 2)
TESTATOB(BGRA, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ABGR, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(RGBA, 4, 4, 1, ARGB, 4, 4, 1, 0)
TESTATOB(RAW, 3, 3, 1, ARGB, 4, 4, 1, 0)
TESTATOB(RGB24, 3, 3, 1, ARGB, 4, 4, 1, 0)
TESTATOB(RGB565, 2, 2, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ARGB1555, 2, 2, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ARGB4444, 2, 2, 1, ARGB, 4, 4, 1, 0)
TESTATOB(YUY2, 2, 4, 1, ARGB, 4, 4, 1, 4)
TESTATOB(UYVY, 2, 4, 1, ARGB, 4, 4, 1, 4)
TESTATOB(BayerBGGR, 1, 2, 2, ARGB, 4, 4, 1, 0)
TESTATOB(BayerRGGB, 1, 2, 2, ARGB, 4, 4, 1, 0)
TESTATOB(BayerGBRG, 1, 2, 2, ARGB, 4, 4, 1, 0)
TESTATOB(BayerGRBG, 1, 2, 2, ARGB, 4, 4, 1, 0)
TESTATOB(I400, 1, 1, 1, ARGB, 4, 4, 1, 0)
TESTATOB(I400, 1, 1, 1, I400, 1, 1, 1, 0)
TESTATOB(I400, 1, 1, 1, I400Mirror, 1, 1, 1, 0)
TESTATOB(Y, 1, 1, 1, ARGB, 4, 4, 1, 0)
TESTATOB(ARGB, 4, 4, 1, ARGBMirror, 4, 4, 1, 0)

TEST_F(libyuvTest, Test565) {
  SIMD_ALIGNED(uint8 orig_pixels[256][4]);
  SIMD_ALIGNED(uint8 pixels565[256][2]);

  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 4; ++j) {
      orig_pixels[i][j] = i;
    }
  }
  ARGBToRGB565(&orig_pixels[0][0], 0, &pixels565[0][0], 0, 256, 1);
  uint32 checksum = HashDjb2(&pixels565[0][0], sizeof(pixels565), 5381);
  EXPECT_EQ(610919429u, checksum);
}

#ifdef HAVE_JPEG
TEST_F(libyuvTest, ValidateJpeg) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg ?
    benchmark_width_ * benchmark_height_ : kMinJpeg;
  const int kSize = kImageSize + kOff;
  align_buffer_64(orig_pixels, kSize);

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kSize);

  // EOI, SOI. Expect pass.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_TRUE(ValidateJpeg(orig_pixels, kSize));
  }
  free_aligned_buffer_page_end(orig_pixels);
}

TEST_F(libyuvTest, InvalidateJpeg) {
  const int kOff = 10;
  const int kMinJpeg = 64;
  const int kImageSize = benchmark_width_ * benchmark_height_ >= kMinJpeg ?
    benchmark_width_ * benchmark_height_ : kMinJpeg;
  const int kSize = kImageSize + kOff;
  align_buffer_64(orig_pixels, kSize);

  // No SOI or EOI. Expect fail.
  memset(orig_pixels, 0, kSize);
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  // SOI but no EOI. Expect fail.
  orig_pixels[0] = 0xff;
  orig_pixels[1] = 0xd8;  // SOI.
  for (int times = 0; times < benchmark_iterations_; ++times) {
    EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));
  }
  // EOI but no SOI. Expect fail.
  orig_pixels[0] = 0;
  orig_pixels[1] = 0;
  orig_pixels[kSize - kOff + 0] = 0xff;
  orig_pixels[kSize - kOff + 1] = 0xd9;  // EOI.
  EXPECT_FALSE(ValidateJpeg(orig_pixels, kSize));

  free_aligned_buffer_page_end(orig_pixels);
}

#endif

}  // namespace libyuv
