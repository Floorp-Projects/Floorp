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

#include "libyuv/cpu_id.h"
#include "libyuv/scale.h"

namespace libyuv {

static int TestFilter(int src_width, int src_height,
                      int dst_width, int dst_height,
                      FilterMode f) {

  int b = 128;
  int src_width_uv = (src_width + 1) >> 1;
  int src_height_uv = (src_height + 1) >> 1;

  int src_y_plane_size = (src_width + (2 * b)) * (src_height + (2 * b));
  int src_uv_plane_size = (src_width_uv + (2 * b)) * (src_height_uv + (2 * b));

  int src_stride_y = 2 * b + src_width;
  int src_stride_uv = 2 * b + src_width_uv;

  align_buffer_16(src_y, src_y_plane_size)
  align_buffer_16(src_u, src_uv_plane_size)
  align_buffer_16(src_v, src_uv_plane_size)

  int dst_width_uv = (dst_width + 1) >> 1;
  int dst_height_uv = (dst_height + 1) >> 1;

  int dst_y_plane_size = (dst_width + (2 * b)) * (dst_height + (2 * b));
  int dst_uv_plane_size = (dst_width_uv + (2 * b)) * (dst_height_uv + (2 * b));

  int dst_stride_y = 2 * b + dst_width;
  int dst_stride_uv = 2 * b + dst_width_uv;

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

  const int runs = 128;
  align_buffer_16(dst_y_c, dst_y_plane_size)
  align_buffer_16(dst_u_c, dst_uv_plane_size)
  align_buffer_16(dst_v_c, dst_uv_plane_size)
  align_buffer_16(dst_y_opt, dst_y_plane_size)
  align_buffer_16(dst_u_opt, dst_uv_plane_size)
  align_buffer_16(dst_v_opt, dst_uv_plane_size)

  MaskCpuFlags(kCpuInitialized);
  double c_time = get_time();

  for (i = 0; i < runs; ++i)
    I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
              src_u + (src_stride_uv * b) + b, src_stride_uv,
              src_v + (src_stride_uv * b) + b, src_stride_uv,
              src_width, src_height,
              dst_y_c + (dst_stride_y * b) + b, dst_stride_y,
              dst_u_c + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_v_c + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_width, dst_height, f);

  c_time = (get_time() - c_time) / runs;

  MaskCpuFlags(-1);
  double opt_time = get_time();

  for (i = 0; i < runs; ++i)
    I420Scale(src_y + (src_stride_y * b) + b, src_stride_y,
              src_u + (src_stride_uv * b) + b, src_stride_uv,
              src_v + (src_stride_uv * b) + b, src_stride_uv,
              src_width, src_height,
              dst_y_opt + (dst_stride_y * b) + b, dst_stride_y,
              dst_u_opt + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_v_opt + (dst_stride_uv * b) + b, dst_stride_uv,
              dst_width, dst_height, f);

  opt_time = (get_time() - opt_time) / runs;

  printf ("filter %d - %8d us c - %8d us opt\n",
          f, (int)(c_time*1e6), (int)(opt_time*1e6));

  // C version may be a little off from the optimized.  Order of
  //  operations may introduce rounding somewhere.  So do a difference
  //  of the buffers and look to see that the max difference isn't
  //  over 2.
  int err = 0;
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b; j < (dst_width + b); ++j) {
      int abs_diff = abs(dst_y_c[(i * dst_stride_y) + j] -
                          dst_y_opt[(i * dst_stride_y) + j]);
      if (abs_diff > max_diff)
        max_diff = abs_diff;
    }
  }

  for (i = b; i < (dst_height_uv + b); ++i) {
    for (j = b; j < (dst_width_uv + b); ++j) {
      int abs_diff = abs(dst_u_c[(i * dst_stride_uv) + j] -
                          dst_u_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff)
        max_diff = abs_diff;
      abs_diff = abs(dst_v_c[(i * dst_stride_uv) + j] -
                      dst_v_opt[(i * dst_stride_uv) + j]);
      if (abs_diff > max_diff)
        max_diff = abs_diff;

    }
  }

  if (max_diff > 2)
    err++;

  free_aligned_buffer_16(dst_y_c)
  free_aligned_buffer_16(dst_u_c)
  free_aligned_buffer_16(dst_v_c)
  free_aligned_buffer_16(dst_y_opt)
  free_aligned_buffer_16(dst_u_opt)
  free_aligned_buffer_16(dst_v_opt)

  free_aligned_buffer_16(src_y)
  free_aligned_buffer_16(src_u)
  free_aligned_buffer_16(src_v)

  return err;
}

TEST_F(libyuvTest, ScaleDownBy2) {

  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width >> 1;
  const int dst_height = src_height >> 1;
  int err = 0;

  for (int f = 0; f < 3; ++f)
    err += TestFilter (src_width, src_height,
                       dst_width, dst_height,
                       static_cast<FilterMode>(f));

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, ScaleDownBy4) {

  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = src_width >> 2;
  const int dst_height = src_height >> 2;
  int err = 0;

  for (int f = 0; f < 3; ++f)
    err += TestFilter (src_width, src_height,
                       dst_width, dst_height,
                       static_cast<FilterMode>(f));

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, ScaleDownBy34) {

  const int src_width = 1280;
  const int src_height = 720;
  const int dst_width = (src_width*3) >> 2;
  const int dst_height = (src_height*3) >> 2;
  int err = 0;

  for (int f = 0; f < 3; ++f)
    err += TestFilter (src_width, src_height,
                       dst_width, dst_height,
                       static_cast<FilterMode>(f));

  EXPECT_EQ(0, err);
}

TEST_F(libyuvTest, ScaleDownBy38) {
  int src_width = 1280;
  int src_height = 720;
  int dst_width = (src_width*3) >> 3;
  int dst_height = (src_height*3) >> 3;

  int err = 0;

  for (int f = 0; f < 3; ++f)
    err += TestFilter (src_width, src_height,
                       dst_width, dst_height,
                       static_cast<FilterMode>(f));

  EXPECT_EQ(0, err);
}

}  // namespace libyuv
