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
#include <string.h>
#include <time.h>

#include "../unit_test/unit_test.h"
#include "libyuv/basic_types.h"
#include "libyuv/compare.h"
#include "libyuv/cpu_id.h"

namespace libyuv {

// hash seed of 5381 recommended.
static uint32 ReferenceHashDjb2(const uint8* src, uint64 count, uint32 seed) {
  uint32 hash = seed;
  if (count > 0) {
    do {
      hash = hash * 33 + *src++;
    } while (--count);
  }
  return hash;
}

TEST_F(libyuvTest, TestDjb2) {
  const int kMaxTest = 2049;
  align_buffer_16(src_a, kMaxTest)

  for (int i = 0; i < kMaxTest; ++i) {
    src_a[i] = i;
  }
  for (int i = 0; i < kMaxTest; ++i) {
    uint32 h1 = HashDjb2(src_a, kMaxTest, 5381);
    uint32 h2 = ReferenceHashDjb2(src_a, kMaxTest, 5381);
    EXPECT_EQ(h1, h2);
  }
  // Hash constant generator using for tables in compare
  int h = 1;
  for (int i = 0; i <= 16 ; ++i) {
    printf("%08x ", h);
    h *= 33;
  }
  printf("\n");

  free_aligned_buffer_16(src_a)
}

TEST_F(libyuvTest, BenchmakDjb2_C) {
  const int kMaxTest = 1280 * 720;
  align_buffer_16(src_a, kMaxTest)

  for (int i = 0; i < kMaxTest; ++i) {
    src_a[i] = i;
  }
  uint32 h2 = ReferenceHashDjb2(src_a, kMaxTest, 5381);
  uint32 h1;
  MaskCpuFlags(kCpuInitialized);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    h1 = HashDjb2(src_a, kMaxTest, 5381);
  }
  MaskCpuFlags(-1);
  EXPECT_EQ(h1, h2);
  free_aligned_buffer_16(src_a)
}

TEST_F(libyuvTest, BenchmakDjb2_OPT) {
  const int kMaxTest = 1280 * 720;
  align_buffer_16(src_a, kMaxTest)

  for (int i = 0; i < kMaxTest; ++i) {
    src_a[i] = i;
  }
  uint32 h2 = ReferenceHashDjb2(src_a, kMaxTest, 5381);
  uint32 h1;
  for (int i = 0; i < benchmark_iterations_; ++i) {
    h1 = HashDjb2(src_a, kMaxTest, 5381);
  }
  EXPECT_EQ(h1, h2);
  free_aligned_buffer_16(src_a)
}

TEST_F(libyuvTest, BenchmakDjb2_Unaligned_OPT) {
  const int kMaxTest = 1280 * 720;
  align_buffer_16(src_a, kMaxTest + 1)

  for (int i = 0; i < kMaxTest; ++i) {
    src_a[i + 1] = i;
  }
  uint32 h2 = ReferenceHashDjb2(src_a + 1, kMaxTest, 5381);
  uint32 h1;
  for (int i = 0; i < benchmark_iterations_; ++i) {
    h1 = HashDjb2(src_a + 1, kMaxTest, 5381);
  }
  EXPECT_EQ(h1, h2);
  free_aligned_buffer_16(src_a)
}

TEST_F(libyuvTest, BenchmarkSumSquareError_C) {
  const int kMaxWidth = 4096 * 3;
  align_buffer_16(src_a, kMaxWidth)
  align_buffer_16(src_b, kMaxWidth)

  for (int i = 0; i < kMaxWidth; ++i) {
    src_a[i] = i;
    src_b[i] = i;
  }

  MaskCpuFlags(kCpuInitialized);
  for (int i = 0; i < benchmark_iterations_; ++i) {
    ComputeSumSquareError(src_a, src_b, kMaxWidth);
  }

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkSumSquareError_OPT) {
  const int kMaxWidth = 4096 * 3;
  align_buffer_16(src_a, kMaxWidth)
  align_buffer_16(src_b, kMaxWidth)

  for (int i = 0; i < kMaxWidth; ++i) {
    src_a[i] = i;
    src_b[i] = i;
  }

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ComputeSumSquareError(src_a, src_b, kMaxWidth);
  }

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, SumSquareError) {
  const int kMaxWidth = 4096 * 3;
  align_buffer_16(src_a, kMaxWidth)
  align_buffer_16(src_b, kMaxWidth)

  memset(src_a, 0, kMaxWidth);
  memset(src_b, 0, kMaxWidth);

  uint64 err;
  err = ComputeSumSquareError(src_a, src_b, kMaxWidth);

  EXPECT_EQ(err, 0);

  memset(src_a, 1, kMaxWidth);
  err = ComputeSumSquareError(src_a, src_b, kMaxWidth);

  EXPECT_EQ(err, kMaxWidth);

  memset(src_a, 190, kMaxWidth);
  memset(src_b, 193, kMaxWidth);
  err = ComputeSumSquareError(src_a, src_b, kMaxWidth);

  EXPECT_EQ(err, (kMaxWidth * 3 * 3));

  srandom(time(NULL));

  for (int i = 0; i < kMaxWidth; ++i) {
    src_a[i] = (random() & 0xff);
    src_b[i] = (random() & 0xff);
  }

  MaskCpuFlags(kCpuInitialized);
  uint64 c_err = ComputeSumSquareError(src_a, src_b, kMaxWidth);

  MaskCpuFlags(-1);
  uint64 opt_err = ComputeSumSquareError(src_a, src_b, kMaxWidth);

  EXPECT_EQ(c_err, opt_err);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkPsnr_C) {
  align_buffer_16(src_a, benchmark_width_ * benchmark_height_)
  align_buffer_16(src_b, benchmark_width_ * benchmark_height_)

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    src_a[i] = i;
    src_b[i] = i;
  }

  MaskCpuFlags(kCpuInitialized);

  double c_time = get_time();
  for (int i = 0; i < benchmark_iterations_; ++i)
    CalcFramePsnr(src_a, benchmark_width_,
                  src_b, benchmark_width_,
                  benchmark_width_, benchmark_height_);

  c_time = (get_time() - c_time) / benchmark_iterations_;
  printf("BenchmarkPsnr_C - %8.2f us c\n", c_time * 1e6);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkPsnr_OPT) {
  align_buffer_16(src_a, benchmark_width_ * benchmark_height_)
  align_buffer_16(src_b, benchmark_width_ * benchmark_height_)

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    src_a[i] = i;
    src_b[i] = i;
  }

  MaskCpuFlags(-1);

  double opt_time = get_time();
  for (int i = 0; i < benchmark_iterations_; ++i)
    CalcFramePsnr(src_a, benchmark_width_,
                  src_b, benchmark_width_,
                  benchmark_width_, benchmark_height_);

  opt_time = (get_time() - opt_time) / benchmark_iterations_;
  printf("BenchmarkPsnr_OPT - %8.2f us opt\n", opt_time * 1e6);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, Psnr) {
  const int kSrcWidth = 1280;
  const int kSrcHeight = 720;
  const int b = 128;
  const int kSrcPlaneSize = (kSrcWidth + b * 2) * (kSrcHeight + b * 2);
  const int kSrcStride = 2 * b + kSrcWidth;
  align_buffer_16(src_a, kSrcPlaneSize)
  align_buffer_16(src_b, kSrcPlaneSize)

  memset(src_a, 0, kSrcPlaneSize);
  memset(src_b, 0, kSrcPlaneSize);

  double err;
  err = CalcFramePsnr(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_EQ(err, kMaxPsnr);

  memset(src_a, 255, kSrcPlaneSize);

  err = CalcFramePsnr(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_EQ(err, 0.0);

  memset(src_a, 1, kSrcPlaneSize);

  err = CalcFramePsnr(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_GT(err, 48.0);
  EXPECT_LT(err, 49.0);

  for (int i = 0; i < kSrcPlaneSize; ++i)
    src_a[i] = i;

  err = CalcFramePsnr(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_GT(err, 4.0);
  EXPECT_LT(err, 5.0);

  srandom(time(NULL));

  memset(src_a, 0, kSrcPlaneSize);
  memset(src_b, 0, kSrcPlaneSize);

  for (int i = b; i < (kSrcHeight + b); ++i) {
    for (int j = b; j < (kSrcWidth + b); ++j) {
      src_a[(i * kSrcStride) + j] = (random() & 0xff);
      src_b[(i * kSrcStride) + j] = (random() & 0xff);
    }
  }

  MaskCpuFlags(kCpuInitialized);
  double c_err, opt_err;

  c_err = CalcFramePsnr(src_a + kSrcStride * b + b, kSrcStride,
                        src_b + kSrcStride * b + b, kSrcStride,
                        kSrcWidth, kSrcHeight);

  MaskCpuFlags(-1);

  opt_err = CalcFramePsnr(src_a + kSrcStride * b + b, kSrcStride,
                          src_b + kSrcStride * b + b, kSrcStride,
                          kSrcWidth, kSrcHeight);

  EXPECT_EQ(opt_err, c_err);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkSsim_C) {
  align_buffer_16(src_a, benchmark_width_ * benchmark_height_)
  align_buffer_16(src_b, benchmark_width_ * benchmark_height_)

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    src_a[i] = i;
    src_b[i] = i;
  }

  MaskCpuFlags(kCpuInitialized);

  double c_time = get_time();
  for (int i = 0; i < benchmark_iterations_; ++i)
    CalcFrameSsim(src_a, benchmark_width_,
                  src_b, benchmark_width_,
                  benchmark_width_, benchmark_height_);

  c_time = (get_time() - c_time) / benchmark_iterations_;
  printf("BenchmarkSsim_C - %8.2f us c\n", c_time * 1e6);

  MaskCpuFlags(-1);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, BenchmarkSsim_OPT) {
  align_buffer_16(src_a, benchmark_width_ * benchmark_height_)
  align_buffer_16(src_b, benchmark_width_ * benchmark_height_)

  for (int i = 0; i < benchmark_width_ * benchmark_height_; ++i) {
    src_a[i] = i;
    src_b[i] = i;
  }

  MaskCpuFlags(-1);

  double opt_time = get_time();
  for (int i = 0; i < benchmark_iterations_; ++i)
    CalcFrameSsim(src_a, benchmark_width_,
                  src_b, benchmark_width_,
                  benchmark_width_, benchmark_height_);

  opt_time = (get_time() - opt_time) / benchmark_iterations_;
  printf("BenchmarkPsnr_OPT - %8.2f us opt\n", opt_time * 1e6);

  EXPECT_EQ(0, 0);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

TEST_F(libyuvTest, Ssim) {
  const int kSrcWidth = 1280;
  const int kSrcHeight = 720;
  const int b = 128;
  const int kSrcPlaneSize = (kSrcWidth + b * 2) * (kSrcHeight + b * 2);
  const int kSrcStride = 2 * b + kSrcWidth;
  align_buffer_16(src_a, kSrcPlaneSize)
  align_buffer_16(src_b, kSrcPlaneSize)

  memset(src_a, 0, kSrcPlaneSize);
  memset(src_b, 0, kSrcPlaneSize);

  double err;
  err = CalcFrameSsim(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_EQ(err, 1.0);

  memset(src_a, 255, kSrcPlaneSize);

  err = CalcFrameSsim(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_LT(err, 0.0001);

  memset(src_a, 1, kSrcPlaneSize);

  err = CalcFrameSsim(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_GT(err, 0.8);
  EXPECT_LT(err, 0.9);

  for (int i = 0; i < kSrcPlaneSize; ++i)
    src_a[i] = i;

  err = CalcFrameSsim(src_a + kSrcStride * b + b, kSrcStride,
                      src_b + kSrcStride * b + b, kSrcStride,
                      kSrcWidth, kSrcHeight);

  EXPECT_GT(err, 0.008);
  EXPECT_LT(err, 0.009);

  srandom(time(NULL));
  for (int i = b; i < (kSrcHeight + b); ++i) {
    for (int j = b; j < (kSrcWidth + b); ++j) {
      src_a[(i * kSrcStride) + j] = (random() & 0xff);
      src_b[(i * kSrcStride) + j] = (random() & 0xff);
    }
  }

  MaskCpuFlags(kCpuInitialized);
  double c_err, opt_err;

  c_err = CalcFrameSsim(src_a + kSrcStride * b + b, kSrcStride,
                        src_b + kSrcStride * b + b, kSrcStride,
                        kSrcWidth, kSrcHeight);

  MaskCpuFlags(-1);

  opt_err = CalcFrameSsim(src_a + kSrcStride * b + b, kSrcStride,
                          src_b + kSrcStride * b + b, kSrcStride,
                          kSrcWidth, kSrcHeight);

  EXPECT_EQ(opt_err, c_err);

  free_aligned_buffer_16(src_a)
  free_aligned_buffer_16(src_b)
}

}  // namespace libyuv
