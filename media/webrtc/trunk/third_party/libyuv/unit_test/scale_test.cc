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

#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

static int TestFilter(int src_width, int src_height,
                      int dst_width, int dst_height,
                      FilterMode f, int rounding, int benchmark_iterations) {
  const int b = 128 * rounding;
  int src_width_uv = (src_width + rounding) >> 1;
  int src_height_uv = (src_height + rounding) >> 1;

  int src_y_plane_size = (src_width + b * 2) * (src_height + b * 2);
  int src_uv_plane_size = (src_width_uv + b * 2) * (src_height_uv + b * 2);

  int src_stride_y = b * 2 + src_width;
  int src_stride_uv = b * 2 + src_width_uv;

  align_buffer_page_end(src_y, src_y_plane_size)
  align_buffer_page_end(src_u, src_uv_plane_size)
  align_buffer_page_end(src_v, src_uv_plane_size)

  int dst_width_uv = (dst_width + rounding) >> 1;
  int dst_height_uv = (dst_height + rounding) >> 1;

  int dst_y_plane_size = (dst_width + b * 2) * (dst_height + b * 2);
  int dst_uv_plane_size = (dst_width_uv + b * 2) * (dst_height_uv + b * 2);

  int dst_stride_y = b * 2 + dst_width;
  int dst_stride_uv = b * 2 + dst_width_uv;

  srandom(time(NULL));

  int i, j;
  for (i = b; i < (src_height + b); ++i) {
    for (j = b; j < (src_width + b); ++j) {
      src_y[(i * src_stride_y) + j] = (random() & 0xff);
    }
  }

  for (i = b; i < (src_height_uv + b); ++i) {
    for (j = b; j < (src_width_uv + b); ++j) {
      src_u[(i * src_stride_uv) + j] = (random() & 0xff);
      src_v[(i * src_stride_uv) + j] = (random() & 0xff);
    }
  }

  align_buffer_page_end(dst_y_c, dst_y_plane_size)
  align_buffer_page_end(dst_u_c, dst_uv_plane_size)
  align_buffer_page_end(dst_v_c, dst_uv_plane_size)
  align_buffer_page_end(dst_y_opt, dst_y_plane_size)
  align_buffer_page_end(dst_u_opt, dst_uv_plane_size)
  align_buffer_page_end(dst_v_opt, dst_uv_plane_size)

  // Warm up both versions for consistent benchmarks.
  MaskCpuFlags(0);  // Disable all CPU optimization.
  I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
            src_u + (src_stride_uv * b) + b, src_stride_uv,
            src_v + (src_stride_uv * b) + b, src_stride_uv,
            src_width, src_height,
            dst_y_c + (dst_stride_y * b) + b, dst_stride_y,
            dst_u_c + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_v_c + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_width, dst_height, f);
  MaskCpuFlags(-1);  // Enable all CPU optimization.
  I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
            src_u + (src_stride_uv * b) + b, src_stride_uv,
            src_v + (src_stride_uv * b) + b, src_stride_uv,
            src_width, src_height,
            dst_y_opt + (dst_stride_y * b) + b, dst_stride_y,
            dst_u_opt + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_v_opt + (dst_stride_uv * b) + b, dst_stride_uv,
            dst_width, dst_height, f);

  MaskCpuFlags(0);  // Disable all CPU optimization.
  double c_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
              src_u + (src_stride_uv * b) + b, src_stride_uv,
              src_v + (src_stride_uv * b) + b, src_stride_uv,
              src_width, src_height,
              dst_y_c + (dst_stride_y * b) + b, dst_stride_y,
              dst_u_c + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_v_c + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_width, dst_height, f);
  }
  c_time = (get_time() - c_time) / benchmark_iterations;

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
         f, static_cast<int>(c_time*1e6), static_cast<int>(opt_time*1e6));

  // C version may be a little off from the optimized.  Order of
  //  operations may introduce rounding somewhere.  So do a difference
  //  of the buffers and look to see that the max difference isn't
  //  over 2.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b; j < (dst_width + b); ++j) {
      int abs_diff = abs(dst_y_c[(i * dst_stride_y) + j] -
                         dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  for (i = b; i < (dst_height_uv + b); ++i) {
    for (j = b; j < (dst_width_uv + b); ++j) {
      int abs_diff = abs(dst_u_c[(i * dst_stride_uv) + j] -
                         dst_u_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
      abs_diff = abs(dst_v_c[(i * dst_stride_uv) + j] -
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

TEST_F(libyuvTest, ScaleDownBy2) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width / 2;
  const int dst_height = src_height / 2;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleDownBy4) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width / 4;
  const int dst_height = src_height / 4;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 2);  // This is the only scale factor with error of 2.
  }
}

TEST_F(libyuvTest, ScaleDownBy5) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width / 5;
  const int dst_height = src_height / 5;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleDownBy8) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width / 8;
  const int dst_height = src_height / 8;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleDownBy16) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width / 16;
  const int dst_height = src_height / 16;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleDownBy34) {
  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width * 3 / 4;
  const int dst_height = src_height * 3 / 4;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleDownBy38) {
  int src_width = 1280;
  int src_height = 720;
  int dst_width = src_width * 3 / 8;
  int dst_height = src_height * 3 / 8;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleTo1366) {
  int src_width = 1280;
  int src_height = 720;
  int dst_width = 1366;
  int dst_height = 768;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleTo4074) {
  int src_width = 2880 * 2;
  int src_height = 1800;
  int dst_width = 4074;
  int dst_height = 1272;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleTo853) {
  int src_width = 1280;
  int src_height = 720;
  int dst_width = 853;
  int dst_height = 480;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleTo853Wrong) {
  int src_width = 1280;
  int src_height = 720;
  int dst_width = 853;
  int dst_height = 480;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 0,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

// A one off test for a screen cast resolution scale.
TEST_F(libyuvTest, ScaleTo684) {
  int src_width = 686;
  int src_height = 557;
  int dst_width = 684;
  int dst_height = 552;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleTo342) {
  int src_width = 686;
  int src_height = 557;
  int dst_width = 342;
  int dst_height = 276;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

TEST_F(libyuvTest, ScaleToHalf342) {
  int src_width = 684;
  int src_height = 552;
  int dst_width = 342;
  int dst_height = 276;

  for (int f = 0; f < 3; ++f) {
    int max_diff = TestFilter(src_width, src_height,
                              dst_width, dst_height,
                              static_cast<FilterMode>(f), 1,
                              benchmark_iterations_);
    EXPECT_LE(max_diff, 1);
  }
}

}  // namespace libyuv
