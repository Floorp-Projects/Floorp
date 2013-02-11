/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "libyuv/cpu_id.h"
#include "libyuv/rotate_argb.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

static int ARGBTestRotate(int src_width, int src_height,
                          int dst_width, int dst_height,
                          libyuv::RotationMode mode, int runs) {
  const int b = 128;
  int src_argb_plane_size = (src_width + b * 2) * (src_height + b * 2) * 4;
  int src_stride_argb = (b * 2 + src_width) * 4;

  align_buffer_16(src_argb, src_argb_plane_size)
  memset(src_argb, 1, src_argb_plane_size);

  int dst_argb_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 4;
  int dst_stride_argb = (b * 2 + dst_width) * 4;

  srandom(time(NULL));

  int i, j;
  for (i = b; i < (src_height + b); ++i) {
    for (j = b; j < (src_width + b) * 4; ++j) {
      src_argb[(i * src_stride_argb) + j] = (random() & 0xff);
    }
  }

  align_buffer_16(dst_argb_c, dst_argb_plane_size)
  align_buffer_16(dst_argb_opt, dst_argb_plane_size)
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  // Warm up both versions for consistent benchmarks.
  MaskCpuFlags(0);  // Disable all CPU optimization.
  ARGBRotate(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
             dst_argb_c + (dst_stride_argb * b) + b * 4, dst_stride_argb,
             src_width, src_height, mode);
  MaskCpuFlags(-1);  // Enable all CPU optimization.
  ARGBRotate(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
             dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
             src_width, src_height, mode);

  MaskCpuFlags(0);  // Disable all CPU optimization.
  double c_time = get_time();
  for (i = 0; i < runs; ++i) {
    ARGBRotate(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
               dst_argb_c + (dst_stride_argb * b) + b * 4, dst_stride_argb,
               src_width, src_height, mode);
  }
  c_time = (get_time() - c_time) / runs;

  MaskCpuFlags(-1);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < runs; ++i) {
    ARGBRotate(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
               dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
               src_width, src_height, mode);
  }
  opt_time = (get_time() - opt_time) / runs;

  // Report performance of C vs OPT
  printf("filter %d - %8d us C - %8d us OPT\n",
         mode, static_cast<int>(c_time*1e6), static_cast<int>(opt_time*1e6));

  // C version may be a little off from the optimized.  Order of
  //  operations may introduce rounding somewhere.  So do a difference
  //  of the buffers and look to see that the max difference isn't
  //  over 2.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b * 4; j < (dst_width + b) * 4; ++j) {
      int abs_diff = abs(dst_argb_c[(i * dst_stride_argb) + j] -
                         dst_argb_opt[(i * dst_stride_argb) + j]);
      if (abs_diff > max_diff)
        max_diff = abs_diff;
    }
  }

  free_aligned_buffer_16(dst_argb_c)
  free_aligned_buffer_16(dst_argb_opt)
  free_aligned_buffer_16(src_argb)
  return max_diff;
}

TEST_F(libyuvTest, ARGBRotate0) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = 1280;
  const int dst_height = 720;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate0,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate90) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = 720;
  const int dst_height = 1280;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate90,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate180) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = 1280;
  const int dst_height = 720;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate180,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate270) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = 720;
  const int dst_height = 1280;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate270,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate0_Odd) {
  const int src_width = 1277;
  const int src_height = 719;
  const int dst_width = 1277;
  const int dst_height = 719;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate0,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate90_Odd) {
  const int src_width = 1277;
  const int src_height = 719;
  const int dst_width = 719;
  const int dst_height = 1277;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate90,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate180_Odd) {
  const int src_width = 1277;
  const int src_height = 719;
  const int dst_width = 1277;
  const int dst_height = 719;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate180,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

TEST_F(libyuvTest, ARGBRotate270_Odd) {
  const int src_width = 1277;
  const int src_height = 719;
  const int dst_width = 719;
  const int dst_height = 1277;

  int err = ARGBTestRotate(src_width, src_height,
                           dst_width, dst_height, kRotate270,
                           benchmark_iterations_);
  EXPECT_GE(1, err);
}

}  // namespace libyuv
