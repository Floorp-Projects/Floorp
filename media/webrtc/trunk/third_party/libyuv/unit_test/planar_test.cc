/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test.h"

#include <stdlib.h>
#include <time.h>

#include "libyuv/rotate.h"
#include "libyuv/planar_functions.h"
#include "libyuv/cpu_id.h"

namespace libyuv {

TEST_F (libyuvTest, BenchmarkI420ToARGB_C) {
  align_buffer_16(src_y, _benchmark_width * _benchmark_height);
  align_buffer_16(src_u, ((_benchmark_width * _benchmark_height) >> 2));
  align_buffer_16(src_v, ((_benchmark_width * _benchmark_height) >> 2));
  align_buffer_16(dst_argb, ((_benchmark_width << 2) * _benchmark_height));

  MaskCpuFlags(kCpuInitialized);

  for (int i = 0; i < _benchmark_iterations; ++i)
    I420ToARGB(src_y, _benchmark_width,
               src_u, _benchmark_width >> 1,
               src_v, _benchmark_width >> 1,
               dst_argb, _benchmark_width << 2,
               _benchmark_width, _benchmark_height);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_y)
  free_aligned_buffer_16(src_u)
  free_aligned_buffer_16(src_v)
  free_aligned_buffer_16(dst_argb)
}

TEST_F (libyuvTest, BenchmarkI420ToARGB_OPT) {
  align_buffer_16(src_y, _benchmark_width * _benchmark_height);
  align_buffer_16(src_u, (_benchmark_width * _benchmark_height) >> 2);
  align_buffer_16(src_v, (_benchmark_width * _benchmark_height) >> 2);
  align_buffer_16(dst_argb, (_benchmark_width << 2) * _benchmark_height);

  for (int i = 0; i < _benchmark_iterations; ++i)
    I420ToARGB(src_y, _benchmark_width,
               src_u, _benchmark_width >> 1,
               src_v, _benchmark_width >> 1,
               dst_argb, _benchmark_width << 2,
               _benchmark_width, _benchmark_height);

  free_aligned_buffer_16(src_y)
  free_aligned_buffer_16(src_u)
  free_aligned_buffer_16(src_v)
  free_aligned_buffer_16(dst_argb)
}

#define TESTI420TO(FMT)                                                        \
TEST_F (libyuvTest, I420To##FMT##_CvsOPT) {                                    \
  const int src_width = 1280;                                                  \
  const int src_height = 720;                                                  \
  align_buffer_16(src_y, src_width * src_height);                              \
  align_buffer_16(src_u, (src_width * src_height) >> 2);                       \
  align_buffer_16(src_v, (src_width * src_height) >> 2);                       \
  align_buffer_16(dst_rgb_c, (src_width << 2) * src_height);                   \
  align_buffer_16(dst_rgb_opt, (src_width << 2) * src_height);                 \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < src_height; ++i)                                         \
    for (int j = 0; j < src_width; ++j)                                        \
      src_y[(i * src_height) + j] = (random() & 0xff);                         \
  for (int i = 0; i < src_height >> 1; ++i)                                    \
    for (int j = 0; j < src_width >> 1; ++j) {                                 \
      src_u[(i * src_height >> 1) + j] = (random() & 0xff);                    \
      src_v[(i * src_height >> 1) + j] = (random() & 0xff);                    \
    }                                                                          \
  MaskCpuFlags(kCpuInitialized);                                               \
  I420To##FMT(src_y, src_width,                                                \
              src_u, src_width >> 1,                                           \
              src_v, src_width >> 1,                                           \
              dst_rgb_c, src_width << 2,                                       \
              src_width, src_height);                                          \
  MaskCpuFlags(-1);                                                            \
  I420To##FMT(src_y, src_width,                                                \
              src_u, src_width >> 1,                                           \
              src_v, src_width >> 1,                                           \
              dst_rgb_opt, src_width << 2,                                     \
              src_width, src_height);                                          \
  int err = 0;                                                                 \
  int i = 0;                                                                   \
  for (int i = 0; i < src_height; ++i) {                                       \
    for (int j = 0; j < src_width << 2; ++j) {                                 \
      int diff = (int)(dst_rgb_c[i * src_height + j]) -                        \
                 (int)(dst_rgb_opt[i * src_height + j]);                       \
      if (abs(diff) > 2)                                                       \
        err++;                                                                 \
    }                                                                          \
  }                                                                            \
  EXPECT_EQ(err, 0);                                                           \
  free_aligned_buffer_16(src_y)                                                \
  free_aligned_buffer_16(src_u)                                                \
  free_aligned_buffer_16(src_v)                                                \
  free_aligned_buffer_16(dst_rgb_c)                                            \
  free_aligned_buffer_16(dst_rgb_opt)                                          \
}

TESTI420TO(ARGB)
TESTI420TO(BGRA)
TESTI420TO(ABGR)

}
