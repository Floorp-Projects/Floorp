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

#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int TestFilter(int src_width, int src_height,
                      int dst_width, int dst_height,
                      FilterMode f, int benchmark_iterations) {
  int i, j;
  const int b = 128;
  int src_width_uv = (Abs(src_width) + 1) >> 1;
  int src_height_uv = (Abs(src_height) + 1) >> 1;

  int src_y_plane_size = (Abs(src_width) + b * 2) * (Abs(src_height) + b * 2);
  int src_uv_plane_size = (src_width_uv + b * 2) * (src_height_uv + b * 2);

  int src_stride_y = b * 2 + Abs(src_width);
  int src_stride_uv = b * 2 + src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size)
  align_buffer_page_end(src_u, src_uv_plane_size)
  align_buffer_page_end(src_v, src_uv_plane_size)
  srandom(time(NULL));
  MemRandomize(src_y, src_y_plane_size);
  MemRandomize(src_u, src_uv_plane_size);
  MemRandomize(src_v, src_uv_plane_size);

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int dst_y_plane_size = (dst_width + b * 2) * (dst_height + b * 2);
  int dst_uv_plane_size = (dst_width_uv + b * 2) * (dst_height_uv + b * 2);

  int dst_stride_y = b * 2 + dst_width;
  int dst_stride_uv = b * 2 + dst_width_uv;

  align_buffer_page_end(dst_y_c, dst_y_plane_size)
  align_buffer_page_end(dst_u_c, dst_uv_plane_size)
  align_buffer_page_end(dst_v_c, dst_uv_plane_size)
  align_buffer_page_end(dst_y_opt, dst_y_plane_size)
  align_buffer_page_end(dst_u_opt, dst_uv_plane_size)
  align_buffer_page_end(dst_v_opt, dst_uv_plane_size)


  MaskCpuFlags(0);  // Disable all CPU optimization.
  double c_time = get_time();
  I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
            src_u + (src_stride_uv * b) + b, src_stride_uv,
            src_v + (src_stride_uv * b) + b, src_stride_uv,
            src_width, src_height,
            dst_y_c + (dst_stride_y * b) + b, dst_stride_y,
            dst_u_c + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_v_c + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  MaskCpuFlags(-1);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
              src_u + (src_stride_uv * b) + b, src_stride_uv,
              src_v + (src_stride_uv * b) + b, src_stride_uv,
              src_width, src_height,
              dst_y_opt + (dst_stride_y * b) + b, dst_stride_y,
              dst_u_opt + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_v_opt + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;
  // Report performance of C vs OPT
  printf("filter %d - %8d us C - %8d us OPT\n",
         f,
         static_cast<int>(c_time * 1e6),
         static_cast<int>(opt_time * 1e6));

  // C version may be a little off from the optimized. Order of
  //  operations may introduce rounding somewhere. So do a difference
  //  of the buffers and look to see that the max difference isn't
  //  over 2.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b; j < (dst_width + b); ++j) {
      int abs_diff = Abs(dst_y_c[(i * dst_stride_y) + j] -
                         dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = b; i < (dst_height_uv + b); ++i) {
    for (j = b; j < (dst_width_uv + b); ++j) {
      int abs_diff = Abs(dst_u_c[(i * dst_stride_uv) + j] -
                         dst_u_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
      abs_diff = Abs(dst_v_c[(i * dst_stride_uv) + j] -
                     dst_v_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_page_end(dst_y_c)
  free_aligned_buffer_page_end(dst_u_c)
  free_aligned_buffer_page_end(dst_v_c)
  free_aligned_buffer_page_end(dst_y_opt)
  free_aligned_buffer_page_end(dst_u_opt)
  free_aligned_buffer_page_end(dst_v_opt)

  free_aligned_buffer_page_end(src_y)
  free_aligned_buffer_page_end(src_u)
  free_aligned_buffer_page_end(src_v)

  return max_diff;
}

#define TEST_FACTOR1(name, filter, hfactor, vfactor, max_diff)                 \
    TEST_F(libyuvTest, ScaleDownBy##name##_##filter) {                         \
      int diff = TestFilter(benchmark_width_, benchmark_height_,               \
                            Abs(benchmark_width_) * hfactor,                   \
                            Abs(benchmark_height_) * vfactor,                  \
                            kFilter##filter, benchmark_iterations_);           \
      EXPECT_LE(diff, max_diff);                                               \
    }

// Test a scale factor with all 4 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#define TEST_FACTOR(name, hfactor, vfactor)                                    \
    TEST_FACTOR1(name, None, hfactor, vfactor, 0)                              \
    TEST_FACTOR1(name, Linear, hfactor, vfactor, 3)                            \
    TEST_FACTOR1(name, Bilinear, hfactor, vfactor, 3)                          \
    TEST_FACTOR1(name, Box, hfactor, vfactor, 3)                               \

TEST_FACTOR(2, 1 / 2, 1 / 2)
TEST_FACTOR(4, 1 / 4, 1 / 4)
TEST_FACTOR(8, 1 / 8, 1 / 8)
TEST_FACTOR(3by4, 3 / 4, 3 / 4)
#undef TEST_FACTOR1
#undef TEST_FACTOR

#define TEST_SCALETO1(name, width, height, filter, max_diff)                   \
    TEST_F(libyuvTest, name##To##width##x##height##_##filter) {                \
      int diff = TestFilter(benchmark_width_, benchmark_height_,               \
                            width, height,                                     \
                            kFilter##filter, benchmark_iterations_);           \
      EXPECT_LE(diff, max_diff);                                               \
    }                                                                          \
    TEST_F(libyuvTest, name##From##width##x##height##_##filter) {              \
      int diff = TestFilter(width, height,                                     \
                            Abs(benchmark_width_), Abs(benchmark_height_),     \
                            kFilter##filter, benchmark_iterations_);           \
      EXPECT_LE(diff, max_diff);                                               \
    }

// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)                                      \
    TEST_SCALETO1(name, width, height, None, 0)                                \
    TEST_SCALETO1(name, width, height, Linear, 3)                              \
    TEST_SCALETO1(name, width, height, Bilinear, 3)                            \
    TEST_SCALETO1(name, width, height, Box, 3)

TEST_SCALETO(Scale, 1, 1)
TEST_SCALETO(Scale, 320, 240)
TEST_SCALETO(Scale, 352, 288)
TEST_SCALETO(Scale, 640, 360)
TEST_SCALETO(Scale, 1280, 720)
#undef TEST_SCALETO1
#undef TEST_SCALETO

}  // namespace libyuv
