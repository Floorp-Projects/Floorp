/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <limits.h>
#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "vpx/vpx_integer.h"
#include "vpx_mem/vpx_mem.h"

using libvpx_test::ACMRandom;

typedef void (*VpxPostProcDownAndAcrossMbRowFunc)(
    unsigned char *src_ptr, unsigned char *dst_ptr, int src_pixels_per_line,
    int dst_pixels_per_line, int cols, unsigned char *flimit, int size);

typedef void (*VpxMbPostProcAcrossIpFunc)(unsigned char *src, int pitch,
                                          int rows, int cols, int flimit);

typedef void (*VpxMbPostProcDownFunc)(unsigned char *dst, int pitch, int rows,
                                      int cols, int flimit);

namespace {

// Compute the filter level used in post proc from the loop filter strength
int q2mbl(int x) {
  if (x < 20) x = 20;

  x = 50 + (x - 50) * 10 / 8;
  return x * x / 3;
}

class VpxPostProcDownAndAcrossMbRowTest
    : public ::testing::TestWithParam<VpxPostProcDownAndAcrossMbRowFunc> {
 public:
  virtual void TearDown() { libvpx_test::ClearSystemState(); }
};

// Test routine for the VPx post-processing function
// vpx_post_proc_down_and_across_mb_row_c.

TEST_P(VpxPostProcDownAndAcrossMbRowTest, CheckFilterOutput) {
  // Size of the underlying data block that will be filtered.
  const int block_width = 16;
  const int block_height = 16;

  // 5-tap filter needs 2 padding rows above and below the block in the input.
  const int input_width = block_width;
  const int input_height = block_height + 4;
  const int input_stride = input_width;
  const int input_size = input_width * input_height;

  // Filter extends output block by 8 samples at left and right edges.
  const int output_width = block_width + 16;
  const int output_height = block_height;
  const int output_stride = output_width;
  const int output_size = output_width * output_height;

  uint8_t *const src_image = new uint8_t[input_size];
  ASSERT_TRUE(src_image != NULL);

  // Though the left padding is only 8 bytes, the assembly code tries to
  // read 16 bytes before the pointer.
  uint8_t *const dst_image = new uint8_t[output_size + 8];
  ASSERT_TRUE(dst_image != NULL);

  // Pointers to top-left pixel of block in the input and output images.
  uint8_t *const src_image_ptr = src_image + (input_stride << 1);

  // The assembly works in increments of 16. The first read may be offset by
  // this amount.
  uint8_t *const dst_image_ptr = dst_image + 16;
  uint8_t *const flimits =
      reinterpret_cast<uint8_t *>(vpx_memalign(16, block_width));
  (void)memset(flimits, 255, block_width);

  // Initialize pixels in the input:
  //   block pixels to value 1,
  //   border pixels to value 10.
  (void)memset(src_image, 10, input_size);
  uint8_t *pixel_ptr = src_image_ptr;
  for (int i = 0; i < block_height; ++i) {
    for (int j = 0; j < block_width; ++j) {
      pixel_ptr[j] = 1;
    }
    pixel_ptr += input_stride;
  }

  // Initialize pixels in the output to 99.
  (void)memset(dst_image, 99, output_size);

  ASM_REGISTER_STATE_CHECK(GetParam()(src_image_ptr, dst_image_ptr,
                                      input_stride, output_stride, block_width,
                                      flimits, 16));

  static const uint8_t kExpectedOutput[block_height] = {
    4, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 4
  };

  pixel_ptr = dst_image_ptr;
  for (int i = 0; i < block_height; ++i) {
    for (int j = 0; j < block_width; ++j) {
      ASSERT_EQ(kExpectedOutput[i], pixel_ptr[j]) << "at (" << i << ", " << j
                                                  << ")";
    }
    pixel_ptr += output_stride;
  }

  delete[] src_image;
  delete[] dst_image;
  vpx_free(flimits);
};

TEST_P(VpxPostProcDownAndAcrossMbRowTest, CheckCvsAssembly) {
  // Size of the underlying data block that will be filtered.
  // Y blocks are always a multiple of 16 wide and exactly 16 high. U and V
  // blocks are always a multiple of 8 wide and exactly 8 high.
  const int block_width = 136;
  const int block_height = 16;

  // 5-tap filter needs 2 padding rows above and below the block in the input.
  // SSE2 reads in blocks of 16. Pad an extra 8 in case the width is not %16.
  const int input_width = block_width;
  const int input_height = block_height + 4 + 8;
  const int input_stride = input_width;
  const int input_size = input_stride * input_height;

  // Filter extends output block by 8 samples at left and right edges.
  // SSE2 reads in blocks of 16. Pad an extra 8 in case the width is not %16.
  const int output_width = block_width + 24;
  const int output_height = block_height;
  const int output_stride = output_width;
  const int output_size = output_stride * output_height;

  uint8_t *const src_image = new uint8_t[input_size];
  ASSERT_TRUE(src_image != NULL);

  // Though the left padding is only 8 bytes, the assembly code tries to
  // read 16 bytes before the pointer.
  uint8_t *const dst_image = new uint8_t[output_size + 8];
  ASSERT_TRUE(dst_image != NULL);
  uint8_t *const dst_image_ref = new uint8_t[output_size + 8];
  ASSERT_TRUE(dst_image_ref != NULL);

  // Pointers to top-left pixel of block in the input and output images.
  uint8_t *const src_image_ptr = src_image + (input_stride << 1);

  // The assembly works in increments of 16. The first read may be offset by
  // this amount.
  uint8_t *const dst_image_ptr = dst_image + 16;
  uint8_t *const dst_image_ref_ptr = dst_image + 16;

  // Filter values are set in blocks of 16 for Y and 8 for U/V. Each macroblock
  // can have a different filter. SSE2 assembly reads flimits in blocks of 16 so
  // it must be padded out.
  const int flimits_width = block_width % 16 ? block_width + 8 : block_width;
  uint8_t *const flimits =
      reinterpret_cast<uint8_t *>(vpx_memalign(16, flimits_width));

  ACMRandom rnd;
  rnd.Reset(ACMRandom::DeterministicSeed());
  // Initialize pixels in the input:
  //   block pixels to random values.
  //   border pixels to value 10.
  (void)memset(src_image, 10, input_size);
  uint8_t *pixel_ptr = src_image_ptr;
  for (int i = 0; i < block_height; ++i) {
    for (int j = 0; j < block_width; ++j) {
      pixel_ptr[j] = rnd.Rand8();
    }
    pixel_ptr += input_stride;
  }

  for (int blocks = 0; blocks < block_width; blocks += 8) {
    (void)memset(flimits, 0, sizeof(*flimits) * flimits_width);

    for (int f = 0; f < 255; f++) {
      (void)memset(flimits + blocks, f, sizeof(*flimits) * 8);

      (void)memset(dst_image, 0, output_size);
      (void)memset(dst_image_ref, 0, output_size);

      vpx_post_proc_down_and_across_mb_row_c(
          src_image_ptr, dst_image_ref_ptr, input_stride, output_stride,
          block_width, flimits, block_height);
      ASM_REGISTER_STATE_CHECK(GetParam()(src_image_ptr, dst_image_ptr,
                                          input_stride, output_stride,
                                          block_width, flimits, 16));

      for (int i = 0; i < block_height; ++i) {
        for (int j = 0; j < block_width; ++j) {
          ASSERT_EQ(dst_image_ref_ptr[j + i * output_stride],
                    dst_image_ptr[j + i * output_stride])
              << "at (" << i << ", " << j << ")";
        }
      }
    }
  }

  delete[] src_image;
  delete[] dst_image;
  delete[] dst_image_ref;
  vpx_free(flimits);
}

class VpxMbPostProcAcrossIpTest
    : public ::testing::TestWithParam<VpxMbPostProcAcrossIpFunc> {
 public:
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void SetCols(unsigned char *s, int rows, int cols, int src_width) {
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        s[c] = c;
      }
      s += src_width;
    }
  }

  void RunComparison(const unsigned char *expected_output, unsigned char *src_c,
                     int rows, int cols, int src_pitch) {
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        ASSERT_EQ(expected_output[c], src_c[c]) << "at (" << r << ", " << c
                                                << ")";
      }
      src_c += src_pitch;
    }
  }

  void RunFilterLevel(unsigned char *s, int rows, int cols, int src_width,
                      int filter_level, const unsigned char *expected_output) {
    ASM_REGISTER_STATE_CHECK(
        GetParam()(s, src_width, rows, cols, filter_level));
    RunComparison(expected_output, s, rows, cols, src_width);
  }
};

TEST_P(VpxMbPostProcAcrossIpTest, CheckLowFilterOutput) {
  const int rows = 16;
  const int cols = 16;
  const int src_left_padding = 8;
  const int src_right_padding = 17;
  const int src_width = cols + src_left_padding + src_right_padding;
  const int src_size = rows * src_width;

  unsigned char *const src = new unsigned char[src_size];
  ASSERT_TRUE(src != NULL);
  memset(src, 10, src_size);
  unsigned char *const s = src + src_left_padding;
  SetCols(s, rows, cols, src_width);

  unsigned char *expected_output = new unsigned char[rows * cols];
  ASSERT_TRUE(expected_output != NULL);
  SetCols(expected_output, rows, cols, cols);

  RunFilterLevel(s, rows, cols, src_width, q2mbl(0), expected_output);
  delete[] src;
  delete[] expected_output;
}

TEST_P(VpxMbPostProcAcrossIpTest, CheckMediumFilterOutput) {
  const int rows = 16;
  const int cols = 16;
  const int src_left_padding = 8;
  const int src_right_padding = 17;
  const int src_width = cols + src_left_padding + src_right_padding;
  const int src_size = rows * src_width;

  unsigned char *const src = new unsigned char[src_size];
  ASSERT_TRUE(src != NULL);
  memset(src, 10, src_size);
  unsigned char *const s = src + src_left_padding;

  SetCols(s, rows, cols, src_width);
  static const unsigned char kExpectedOutput[cols] = {
    2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 13
  };

  RunFilterLevel(s, rows, cols, src_width, q2mbl(70), kExpectedOutput);

  delete[] src;
}

TEST_P(VpxMbPostProcAcrossIpTest, CheckHighFilterOutput) {
  const int rows = 16;
  const int cols = 16;
  const int src_left_padding = 8;
  const int src_right_padding = 17;
  const int src_width = cols + src_left_padding + src_right_padding;
  const int src_size = rows * src_width;

  unsigned char *const src = new unsigned char[src_size];
  ASSERT_TRUE(src != NULL);
  unsigned char *const s = src + src_left_padding;

  memset(src, 10, src_size);
  SetCols(s, rows, cols, src_width);
  static const unsigned char kExpectedOutput[cols] = {
    2, 2, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 11, 12, 13, 13
  };

  RunFilterLevel(s, rows, cols, src_width, INT_MAX, kExpectedOutput);

  memset(src, 10, src_size);
  SetCols(s, rows, cols, src_width);
  RunFilterLevel(s, rows, cols, src_width, q2mbl(100), kExpectedOutput);

  delete[] src;
}

TEST_P(VpxMbPostProcAcrossIpTest, CheckCvsAssembly) {
  const int rows = 16;
  const int cols = 16;
  const int src_left_padding = 8;
  const int src_right_padding = 17;
  const int src_width = cols + src_left_padding + src_right_padding;
  const int src_size = rows * src_width;

  unsigned char *const c_mem = new unsigned char[src_size];
  unsigned char *const asm_mem = new unsigned char[src_size];
  ASSERT_TRUE(c_mem != NULL);
  ASSERT_TRUE(asm_mem != NULL);
  unsigned char *const src_c = c_mem + src_left_padding;
  unsigned char *const src_asm = asm_mem + src_left_padding;

  // When level >= 100, the filter behaves the same as the level = INT_MAX
  // When level < 20, it behaves the same as the level = 0
  for (int level = 0; level < 100; level++) {
    memset(c_mem, 10, src_size);
    memset(asm_mem, 10, src_size);
    SetCols(src_c, rows, cols, src_width);
    SetCols(src_asm, rows, cols, src_width);

    vpx_mbpost_proc_across_ip_c(src_c, src_width, rows, cols, q2mbl(level));
    ASM_REGISTER_STATE_CHECK(
        GetParam()(src_asm, src_width, rows, cols, q2mbl(level)));

    RunComparison(src_c, src_asm, rows, cols, src_width);
  }

  delete[] c_mem;
  delete[] asm_mem;
}

class VpxMbPostProcDownTest
    : public ::testing::TestWithParam<VpxMbPostProcDownFunc> {
 public:
  virtual void TearDown() { libvpx_test::ClearSystemState(); }

 protected:
  void SetRows(unsigned char *src_c, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
      memset(src_c, r, cols);
      src_c += cols;
    }
  }

  void SetRandom(unsigned char *src_c, unsigned char *src_asm, int rows,
                 int cols, int src_pitch) {
    ACMRandom rnd;
    rnd.Reset(ACMRandom::DeterministicSeed());

    // Add some random noise to the input
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        const int noise = rnd(4);
        src_c[c] = r + noise;
        src_asm[c] = r + noise;
      }
      src_c += src_pitch;
      src_asm += src_pitch;
    }
  }

  void SetRandomSaturation(unsigned char *src_c, unsigned char *src_asm,
                           int rows, int cols, int src_pitch) {
    ACMRandom rnd;
    rnd.Reset(ACMRandom::DeterministicSeed());

    // Add some random noise to the input
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        const int noise = 3 * rnd(2);
        src_c[c] = r + noise;
        src_asm[c] = r + noise;
      }
      src_c += src_pitch;
      src_asm += src_pitch;
    }
  }

  void RunComparison(const unsigned char *expected_output, unsigned char *src_c,
                     int rows, int cols, int src_pitch) {
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        ASSERT_EQ(expected_output[r * rows + c], src_c[c]) << "at (" << r
                                                           << ", " << c << ")";
      }
      src_c += src_pitch;
    }
  }

  void RunComparison(unsigned char *src_c, unsigned char *src_asm, int rows,
                     int cols, int src_pitch) {
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        ASSERT_EQ(src_c[c], src_asm[c]) << "at (" << r << ", " << c << ")";
      }
      src_c += src_pitch;
      src_asm += src_pitch;
    }
  }

  void RunFilterLevel(unsigned char *s, int rows, int cols, int src_width,
                      int filter_level, const unsigned char *expected_output) {
    ASM_REGISTER_STATE_CHECK(
        GetParam()(s, src_width, rows, cols, filter_level));
    RunComparison(expected_output, s, rows, cols, src_width);
  }
};

TEST_P(VpxMbPostProcDownTest, CheckHighFilterOutput) {
  const int rows = 16;
  const int cols = 16;
  const int src_pitch = cols;
  const int src_top_padding = 8;
  const int src_bottom_padding = 17;

  const int src_size = cols * (rows + src_top_padding + src_bottom_padding);
  unsigned char *const c_mem = new unsigned char[src_size];
  ASSERT_TRUE(c_mem != NULL);
  memset(c_mem, 10, src_size);
  unsigned char *const src_c = c_mem + src_top_padding * src_pitch;

  SetRows(src_c, rows, cols);

  static const unsigned char kExpectedOutput[rows * cols] = {
    2,  2,  1,  1,  2,  2,  2,  2,  2,  2,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  2,  2,  2,  2,  2,  2,  2,  3,  2,  2,  2,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  4,  4,  3,  4,  4,  3,  3,  3,
    4,  4,  3,  4,  4,  3,  3,  4,  5,  4,  4,  4,  4,  4,  4,  4,  5,  4,  4,
    4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
    5,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  7,  7,
    7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,
    8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,  8,  9,  9,  8,  8,  8,  9,
    9,  8,  9,  9,  8,  8,  8,  9,  9,  10, 10, 9,  9,  9,  10, 10, 9,  10, 10,
    9,  9,  9,  10, 10, 10, 11, 10, 10, 10, 11, 10, 11, 10, 11, 10, 10, 10, 11,
    10, 11, 11, 11, 11, 11, 11, 11, 12, 11, 11, 11, 11, 11, 11, 11, 12, 11, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 12,
    13, 12, 13, 12, 12, 12, 13, 12, 13, 12, 13, 12, 13, 13, 13, 14, 13, 13, 13,
    13, 13, 13, 13, 14, 13, 13, 13, 13
  };

  RunFilterLevel(src_c, rows, cols, src_pitch, INT_MAX, kExpectedOutput);

  memset(c_mem, 10, src_size);
  SetRows(src_c, rows, cols);
  RunFilterLevel(src_c, rows, cols, src_pitch, q2mbl(100), kExpectedOutput);

  delete[] c_mem;
}

TEST_P(VpxMbPostProcDownTest, CheckMediumFilterOutput) {
  const int rows = 16;
  const int cols = 16;
  const int src_pitch = cols;
  const int src_top_padding = 8;
  const int src_bottom_padding = 17;

  const int src_size = cols * (rows + src_top_padding + src_bottom_padding);
  unsigned char *const c_mem = new unsigned char[src_size];
  ASSERT_TRUE(c_mem != NULL);
  memset(c_mem, 10, src_size);
  unsigned char *const src_c = c_mem + src_top_padding * src_pitch;

  SetRows(src_c, rows, cols);

  static const unsigned char kExpectedOutput[rows * cols] = {
    2,  2,  1,  1,  2,  2,  2,  2,  2,  2,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  2,  2,  2,  2,  2,  2,  2,  3,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
    5,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  7,  7,
    7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,
    8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9,
    9,  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 12, 12, 13, 12,
    13, 12, 13, 12, 12, 12, 13, 12, 13, 12, 13, 12, 13, 13, 13, 14, 13, 13, 13,
    13, 13, 13, 13, 14, 13, 13, 13, 13
  };

  RunFilterLevel(src_c, rows, cols, src_pitch, q2mbl(70), kExpectedOutput);

  delete[] c_mem;
}

TEST_P(VpxMbPostProcDownTest, CheckLowFilterOutput) {
  const int rows = 16;
  const int cols = 16;
  const int src_pitch = cols;
  const int src_top_padding = 8;
  const int src_bottom_padding = 17;

  const int src_size = cols * (rows + src_top_padding + src_bottom_padding);
  unsigned char *const c_mem = new unsigned char[src_size];
  ASSERT_TRUE(c_mem != NULL);
  memset(c_mem, 10, src_size);
  unsigned char *const src_c = c_mem + src_top_padding * src_pitch;

  SetRows(src_c, rows, cols);

  unsigned char *expected_output = new unsigned char[rows * cols];
  ASSERT_TRUE(expected_output != NULL);
  SetRows(expected_output, rows, cols);

  RunFilterLevel(src_c, rows, cols, src_pitch, q2mbl(0), expected_output);

  delete[] c_mem;
  delete[] expected_output;
}

TEST_P(VpxMbPostProcDownTest, CheckCvsAssembly) {
  const int rows = 16;
  const int cols = 16;
  const int src_pitch = cols;
  const int src_top_padding = 8;
  const int src_bottom_padding = 17;
  const int src_size = cols * (rows + src_top_padding + src_bottom_padding);
  unsigned char *const c_mem = new unsigned char[src_size];
  unsigned char *const asm_mem = new unsigned char[src_size];
  ASSERT_TRUE(c_mem != NULL);
  ASSERT_TRUE(asm_mem != NULL);
  unsigned char *const src_c = c_mem + src_top_padding * src_pitch;
  unsigned char *const src_asm = asm_mem + src_top_padding * src_pitch;

  for (int level = 0; level < 100; level++) {
    memset(c_mem, 10, src_size);
    memset(asm_mem, 10, src_size);
    SetRandom(src_c, src_asm, rows, cols, src_pitch);
    vpx_mbpost_proc_down_c(src_c, src_pitch, rows, cols, q2mbl(level));
    ASM_REGISTER_STATE_CHECK(
        GetParam()(src_asm, src_pitch, rows, cols, q2mbl(level)));
    RunComparison(src_c, src_asm, rows, cols, src_pitch);

    memset(c_mem, 10, src_size);
    memset(asm_mem, 10, src_size);
    SetRandomSaturation(src_c, src_asm, rows, cols, src_pitch);
    vpx_mbpost_proc_down_c(src_c, src_pitch, rows, cols, q2mbl(level));
    ASM_REGISTER_STATE_CHECK(
        GetParam()(src_asm, src_pitch, rows, cols, q2mbl(level)));
    RunComparison(src_c, src_asm, rows, cols, src_pitch);
  }

  delete[] c_mem;
  delete[] asm_mem;
}

INSTANTIATE_TEST_CASE_P(
    C, VpxPostProcDownAndAcrossMbRowTest,
    ::testing::Values(vpx_post_proc_down_and_across_mb_row_c));

INSTANTIATE_TEST_CASE_P(C, VpxMbPostProcAcrossIpTest,
                        ::testing::Values(vpx_mbpost_proc_across_ip_c));

INSTANTIATE_TEST_CASE_P(C, VpxMbPostProcDownTest,
                        ::testing::Values(vpx_mbpost_proc_down_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(
    SSE2, VpxPostProcDownAndAcrossMbRowTest,
    ::testing::Values(vpx_post_proc_down_and_across_mb_row_sse2));

INSTANTIATE_TEST_CASE_P(SSE2, VpxMbPostProcAcrossIpTest,
                        ::testing::Values(vpx_mbpost_proc_across_ip_sse2));

INSTANTIATE_TEST_CASE_P(SSE2, VpxMbPostProcDownTest,
                        ::testing::Values(vpx_mbpost_proc_down_sse2));
#endif  // HAVE_SSE2

#if HAVE_NEON
INSTANTIATE_TEST_CASE_P(
    NEON, VpxPostProcDownAndAcrossMbRowTest,
    ::testing::Values(vpx_post_proc_down_and_across_mb_row_neon));

INSTANTIATE_TEST_CASE_P(NEON, VpxMbPostProcAcrossIpTest,
                        ::testing::Values(vpx_mbpost_proc_across_ip_neon));
#endif  // HAVE_NEON

#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(
    MSA, VpxPostProcDownAndAcrossMbRowTest,
    ::testing::Values(vpx_post_proc_down_and_across_mb_row_msa));

INSTANTIATE_TEST_CASE_P(MSA, VpxMbPostProcAcrossIpTest,
                        ::testing::Values(vpx_mbpost_proc_across_ip_msa));

INSTANTIATE_TEST_CASE_P(MSA, VpxMbPostProcDownTest,
                        ::testing::Values(vpx_mbpost_proc_down_msa));
#endif  // HAVE_MSA

}  // namespace
