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
#include "libyuv/scale_argb.h"
#include "libyuv/row.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

// Test scaling with C vs Opt and return maximum pixel difference. 0 = exact.
static int ARGBTestFilter(int src_width, int src_height,
                          int dst_width, int dst_height,
                          FilterMode f, int benchmark_iterations) {
  const int b = 128;
  int i, j;
  int src_argb_plane_size = (Abs(src_width) + b * 2) *
      (Abs(src_height) + b * 2) * 4;
  int src_stride_argb = (b * 2 + Abs(src_width)) * 4;

  align_buffer_64(src_argb, src_argb_plane_size);
  srandom(time(NULL));
  MemRandomize(src_argb, src_argb_plane_size);

  int dst_argb_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 4;
  int dst_stride_argb = (b * 2 + dst_width) * 4;

  align_buffer_64(dst_argb_c, dst_argb_plane_size);
  align_buffer_64(dst_argb_opt, dst_argb_plane_size);
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  // Warm up both versions for consistent benchmarks.
  MaskCpuFlags(0);  // Disable all CPU optimization.
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height,
            dst_argb_c + (dst_stride_argb * b) + b * 4, dst_stride_argb,
            dst_width, dst_height, f);
  MaskCpuFlags(-1);  // Enable all CPU optimization.
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height,
            dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
            dst_width, dst_height, f);

  MaskCpuFlags(0);  // Disable all CPU optimization.
  double c_time = get_time();
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height,
            dst_argb_c + (dst_stride_argb * b) + b * 4, dst_stride_argb,
            dst_width, dst_height, f);

  c_time = (get_time() - c_time);

  MaskCpuFlags(-1);  // Enable all CPU optimization.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
              src_width, src_height,
              dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
              dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;

  // Report performance of C vs OPT
  printf("filter %d - %8d us C - %8d us OPT\n",
         f, static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // C version may be a little off from the optimized. Order of
  //  operations may introduce rounding somewhere. So do a difference
  //  of the buffers and look to see that the max difference isn't
  //  over 2.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b * 4; j < (dst_width + b) * 4; ++j) {
      int abs_diff = Abs(dst_argb_c[(i * dst_stride_argb) + j] -
                         dst_argb_opt[(i * dst_stride_argb) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  free_aligned_buffer_64(src_argb);
  return max_diff;
}

static const int kTileX = 8;
static const int kTileY = 8;

static int TileARGBScale(const uint8* src_argb, int src_stride_argb,
                         int src_width, int src_height,
                         uint8* dst_argb, int dst_stride_argb,
                         int dst_width, int dst_height,
                         FilterMode filtering) {
  for (int y = 0; y < dst_height; y += kTileY) {
    for (int x = 0; x < dst_width; x += kTileX) {
      int clip_width = kTileX;
      if (x + clip_width > dst_width) {
        clip_width = dst_width - x;
      }
      int clip_height = kTileY;
      if (y + clip_height > dst_height) {
        clip_height = dst_height - y;
      }
      int r = ARGBScaleClip(src_argb, src_stride_argb,
                            src_width, src_height,
                            dst_argb, dst_stride_argb,
                            dst_width, dst_height,
                            x, y, clip_width, clip_height, filtering);
      if (r) {
        return r;
      }
    }
  }
  return 0;
}

static int ARGBClipTestFilter(int src_width, int src_height,
                              int dst_width, int dst_height,
                              FilterMode f, int benchmark_iterations) {
  const int b = 128;
  int src_argb_plane_size = (Abs(src_width) + b * 2) *
      (Abs(src_height) + b * 2) * 4;
  int src_stride_argb = (b * 2 + Abs(src_width)) * 4;

  align_buffer_64(src_argb, src_argb_plane_size);
  memset(src_argb, 1, src_argb_plane_size);

  int dst_argb_plane_size = (dst_width + b * 2) * (dst_height + b * 2) * 4;
  int dst_stride_argb = (b * 2 + dst_width) * 4;

  srandom(time(NULL));

  int i, j;
  for (i = b; i < (Abs(src_height) + b); ++i) {
    for (j = b; j < (Abs(src_width) + b) * 4; ++j) {
      src_argb[(i * src_stride_argb) + j] = (random() & 0xff);
    }
  }

  align_buffer_64(dst_argb_c, dst_argb_plane_size);
  align_buffer_64(dst_argb_opt, dst_argb_plane_size);
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  // Do full image, no clipping.
  double c_time = get_time();
  ARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
            src_width, src_height,
            dst_argb_c + (dst_stride_argb * b) + b * 4, dst_stride_argb,
            dst_width, dst_height, f);
  c_time = (get_time() - c_time);

  // Do tiled image, clipping scale to a tile at a time.
  double opt_time = get_time();
  for (i = 0; i < benchmark_iterations; ++i) {
    TileARGBScale(src_argb + (src_stride_argb * b) + b * 4, src_stride_argb,
                  src_width, src_height,
                  dst_argb_opt + (dst_stride_argb * b) + b * 4, dst_stride_argb,
                  dst_width, dst_height, f);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations;

  // Report performance of Full vs Tiled.
  printf("filter %d - %8d us Full - %8d us Tiled\n",
         f, static_cast<int>(c_time * 1e6), static_cast<int>(opt_time * 1e6));

  // Compare full scaled image vs tiled image.
  int max_diff = 0;
  for (i = b; i < (dst_height + b); ++i) {
    for (j = b * 4; j < (dst_width + b) * 4; ++j) {
      int abs_diff = Abs(dst_argb_c[(i * dst_stride_argb) + j] -
                         dst_argb_opt[(i * dst_stride_argb) + j]);
      if (abs_diff > max_diff) {
        max_diff = abs_diff;
      }
    }
  }

  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  free_aligned_buffer_64(src_argb);
  return max_diff;
}

#define TEST_FACTOR1(name, filter, hfactor, vfactor, max_diff)                 \
    TEST_F(libyuvTest, ARGBScaleDownBy##name##_##filter) {                     \
      int diff = ARGBTestFilter(benchmark_width_, benchmark_height_,           \
                                Abs(benchmark_width_) * hfactor,               \
                                Abs(benchmark_height_) * vfactor,              \
                                kFilter##filter, benchmark_iterations_);       \
      EXPECT_LE(diff, max_diff);                                               \
    }                                                                          \
    TEST_F(libyuvTest, ARGBScaleDownClipBy##name##_##filter) {                 \
      int diff = ARGBClipTestFilter(benchmark_width_, benchmark_height_,       \
                                Abs(benchmark_width_) * hfactor,               \
                                Abs(benchmark_height_) * vfactor,              \
                                kFilter##filter, benchmark_iterations_);       \
      EXPECT_LE(diff, max_diff);                                               \
    }

// Test a scale factor with 2 filters.  Expect unfiltered to be exact, but
// filtering is different fixed point implementations for SSSE3, Neon and C.
#define TEST_FACTOR(name, hfactor, vfactor)                                    \
    TEST_FACTOR1(name, None, hfactor, vfactor, 2)                              \
    TEST_FACTOR1(name, Linear, hfactor, vfactor, 2)                            \
    TEST_FACTOR1(name, Bilinear, hfactor, vfactor, 2)                          \
    TEST_FACTOR1(name, Box, hfactor, vfactor, 2)

TEST_FACTOR(2, 1 / 2, 1 / 2)
TEST_FACTOR(4, 1 / 4, 1 / 4)
TEST_FACTOR(8, 1 / 8, 1 / 8)
TEST_FACTOR(3by4, 3 / 4, 3 / 4)
#undef TEST_FACTOR1
#undef TEST_FACTOR

#define TEST_SCALETO1(name, width, height, filter, max_diff)                   \
    TEST_F(libyuvTest, name##To##width##x##height##_##filter) {                \
      int diff = ARGBTestFilter(benchmark_width_, benchmark_height_,           \
                                width, height,                                 \
                                kFilter##filter, benchmark_iterations_);       \
      EXPECT_LE(diff, max_diff);                                               \
    }                                                                          \
    TEST_F(libyuvTest, name##From##width##x##height##_##filter) {              \
      int diff = ARGBTestFilter(width, height,                                 \
                                Abs(benchmark_width_), Abs(benchmark_height_), \
                                kFilter##filter, benchmark_iterations_);       \
      EXPECT_LE(diff, max_diff);                                               \
    }                                                                          \
    TEST_F(libyuvTest, name##ClipTo##width##x##height##_##filter) {            \
      int diff = ARGBClipTestFilter(benchmark_width_, benchmark_height_,       \
                                width, height,                                 \
                                kFilter##filter, benchmark_iterations_);       \
      EXPECT_LE(diff, max_diff);                                               \
    }                                                                          \
    TEST_F(libyuvTest, name##ClipFrom##width##x##height##_##filter) {          \
      int diff = ARGBClipTestFilter(width, height,                             \
                                Abs(benchmark_width_), Abs(benchmark_height_), \
                                kFilter##filter, benchmark_iterations_);       \
      EXPECT_LE(diff, max_diff);                                               \
    }

/// Test scale to a specified size with all 4 filters.
#define TEST_SCALETO(name, width, height)                                      \
    TEST_SCALETO1(name, width, height, None, 0)                                \
    TEST_SCALETO1(name, width, height, Linear, 3)                              \
    TEST_SCALETO1(name, width, height, Bilinear, 3)                            \
    TEST_SCALETO1(name, width, height, Box, 3)

TEST_SCALETO(ARGBScale, 1, 1)
TEST_SCALETO(ARGBScale, 320, 240)
TEST_SCALETO(ARGBScale, 352, 288)
TEST_SCALETO(ARGBScale, 640, 360)
TEST_SCALETO(ARGBScale, 1280, 720)
#undef TEST_SCALETO1
#undef TEST_SCALETO

}  // namespace libyuv
