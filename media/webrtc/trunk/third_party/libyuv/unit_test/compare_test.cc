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
#include <string.h>
#include <time.h>

#include "libyuv/basic_types.h"
#include "libyuv/compare.h"
#include "libyuv/cpu_id.h"

namespace libyuv {

TEST_F(libyuvTest, BenchmarkSumSquareError_C) {
  const int max_width = 4096*3;

  align_buffer_16(src_a, max_width)
  align_buffer_16(src_b, max_width)

  MaskCpuFlags(kCpuInitialized);
  for (int i = 0; i < _benchmark_iterations; ++i)
    ComputeSumSquareError(src_a, src_b, max_width);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkSumSquareError_OPT) {
  const int max_width = 4096*3;

  align_buffer_16(src_a, max_width)
  align_buffer_16(src_b, max_width)

  for (int i = 0; i < _benchmark_iterations; ++i)
    ComputeSumSquareError(src_a, src_b, max_width);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, SumSquareError) {
  const int max_width = 4096*3;

  align_buffer_16(src_a, max_width)
  align_buffer_16(src_b, max_width)

  uint64 err;
  err = ComputeSumSquareError(src_a, src_b, max_width);

  EXPECT_EQ(err, 0);

  memset(src_a, 1, max_width);
  err = ComputeSumSquareError(src_a, src_b, max_width);

  EXPECT_EQ(err, max_width);

  memset(src_a, 190, max_width);
  memset(src_b, 193, max_width);
  err = ComputeSumSquareError(src_a, src_b, max_width);

  EXPECT_EQ(err, (max_width*3*3));

  srandom(time(NULL));

  memset(src_a, 0, max_width);
  memset(src_b, 0, max_width);

  for (int i = 0; i < max_width; ++i) {
    src_a[i] = (random() & 0xff);
    src_b[i] = (random() & 0xff);
  }

  MaskCpuFlags(kCpuInitialized);
  uint64 c_err = ComputeSumSquareError(src_a, src_b, max_width);

  MaskCpuFlags(-1);
  uint64 opt_err = ComputeSumSquareError(src_a, src_b, max_width);

  EXPECT_EQ(c_err, opt_err);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkPsnr_C) {
  align_buffer_16(src_a, _benchmark_width * _benchmark_height)
  align_buffer_16(src_b, _benchmark_width * _benchmark_height)

  MaskCpuFlags(kCpuInitialized);

  double c_time = get_time();
  for (int i = 0; i < _benchmark_iterations; ++i)
    CalcFramePsnr(src_a, _benchmark_width,
                  src_b, _benchmark_width,
                  _benchmark_width, _benchmark_height);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkPsnr_OPT) {
  align_buffer_16(src_a, _benchmark_width * _benchmark_height)
  align_buffer_16(src_b, _benchmark_width * _benchmark_height)

  MaskCpuFlags(kCpuInitialized);

  double c_time = get_time();
  for (int i = 0; i < _benchmark_iterations; ++i)
    CalcFramePsnr(src_a, _benchmark_width,
                  src_b, _benchmark_width,
                  _benchmark_width, _benchmark_height);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, Psnr) {
  const int src_width = 1280;
  const int src_height = 720;
  const int b = 128;

  const int src_plane_size = (src_width + (2 * b)) * (src_height + (2 * b));
  const int src_stride = 2 * b + src_width;


  align_buffer_16(src_a, src_plane_size)
  align_buffer_16(src_b, src_plane_size)

  double err;
  err = CalcFramePsnr(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_EQ(err, kMaxPsnr);

  memset(src_a, 255, src_plane_size);

  err = CalcFramePsnr(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_EQ(err, 0.0);

  memset(src_a, 1, src_plane_size);

  err = CalcFramePsnr(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_GT(err, 48.0);
  EXPECT_LT(err, 49.0);

  for (int i = 0; i < src_plane_size; ++i)
    src_a[i] = i;

  err = CalcFramePsnr(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_GT(err, 4.0);
  EXPECT_LT(err, 5.0);

  srandom(time(NULL));

  memset(src_a, 0, src_plane_size);
  memset(src_b, 0, src_plane_size);

  for (int i = b; i < (src_height + b); ++i) {
    for (int j = b; j < (src_width + b); ++j) {
      src_a[(i * src_stride) + j] = (random() & 0xff);
      src_b[(i * src_stride) + j] = (random() & 0xff);
    }
  }

  MaskCpuFlags(kCpuInitialized);
  double c_err, opt_err;

  c_err = CalcFramePsnr(src_a + (src_stride * b) + b, src_stride,
                        src_b + (src_stride * b) + b, src_stride,
                        src_width, src_height);

  MaskCpuFlags(-1);

  opt_err = CalcFramePsnr(src_a + (src_stride * b) + b, src_stride,
                          src_b + (src_stride * b) + b, src_stride,
                          src_width, src_height);

  EXPECT_EQ(opt_err, c_err);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkSsim_C) {
  align_buffer_16(src_a, _benchmark_width * _benchmark_height)
  align_buffer_16(src_b, _benchmark_width * _benchmark_height)

  MaskCpuFlags(kCpuInitialized);

  double c_time = get_time();
  for (int i = 0; i < _benchmark_iterations; ++i)
    CalcFrameSsim(src_a, _benchmark_width,
                  src_b, _benchmark_width,
                  _benchmark_width, _benchmark_height);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkSsim_OPT) {
  align_buffer_16(src_a, _benchmark_width * _benchmark_height)
  align_buffer_16(src_b, _benchmark_width * _benchmark_height)

  MaskCpuFlags(kCpuInitialized);

  double c_time = get_time();
  for (int i = 0; i < _benchmark_iterations; ++i)
    CalcFrameSsim(src_a, _benchmark_width,
                  src_b, _benchmark_width,
                  _benchmark_width, _benchmark_height);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, Ssim) {
  const int src_width = 1280;
  const int src_height = 720;
  const int b = 128;

  const int src_plane_size = (src_width + (2 * b)) * (src_height + (2 * b));
  const int src_stride = 2 * b + src_width;


  align_buffer_16(src_a, src_plane_size)
  align_buffer_16(src_b, src_plane_size)

  double err;
  err = CalcFrameSsim(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_EQ(err, 1.0);

  memset(src_a, 255, src_plane_size);

  err = CalcFrameSsim(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_LT(err, 0.0001);

  memset(src_a, 1, src_plane_size);

  err = CalcFrameSsim(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_GT(err, 0.8);
  EXPECT_LT(err, 0.9);

  for (int i = 0; i < src_plane_size; ++i)
    src_a[i] = i;

  err = CalcFrameSsim(src_a + (src_stride * b) + b, src_stride,
                      src_b + (src_stride * b) + b, src_stride,
                      src_width, src_height);

  EXPECT_GT(err, 0.008);
  EXPECT_LT(err, 0.009);

  srandom(time(NULL));

  memset(src_a, 0, src_plane_size);
  memset(src_b, 0, src_plane_size);

  for (int i = b; i < (src_height + b); ++i) {
    for (int j = b; j < (src_width + b); ++j) {
      src_a[(i * src_stride) + j] = (random() & 0xff);
      src_b[(i * src_stride) + j] = (random() & 0xff);
    }
  }

  MaskCpuFlags(kCpuInitialized);
  double c_err, opt_err;

  c_err = CalcFrameSsim(src_a + (src_stride * b) + b, src_stride,
                        src_b + (src_stride * b) + b, src_stride,
                        src_width, src_height);

  MaskCpuFlags(-1);

  opt_err = CalcFrameSsim(src_a + (src_stride * b) + b, src_stride,
                          src_b + (src_stride * b) + b, src_stride,
                          src_width, src_height);

  EXPECT_EQ(opt_err, c_err);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

}  // namespace libyuv
