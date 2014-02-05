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
#include "libyuv/planar_functions.h"
#include "libyuv/rotate.h"
#include "libyuv/row.h"  // For Sobel
#include "../unit_test/unit_test.h"

#if defined(_MSC_VER)
#define SIMD_ALIGNED(var) __declspec(align(16)) var
#else  // __GNUC__
#define SIMD_ALIGNED(var) var __attribute__((aligned(16)))
#endif

namespace libyuv {

TEST_F(libyuvTest, TestAttenuate) {
  const int kSize = 1280 * 4;
  align_buffer_64(orig_pixels, kSize);
  align_buffer_64(atten_pixels, kSize);
  align_buffer_64(unatten_pixels, kSize);
  align_buffer_64(atten2_pixels, kSize);

  // Test unattenuation clamps
  orig_pixels[0 * 4 + 0] = 200u;
  orig_pixels[0 * 4 + 1] = 129u;
  orig_pixels[0 * 4 + 2] = 127u;
  orig_pixels[0 * 4 + 3] = 128u;
  // Test unattenuation transparent and opaque are unaffected
  orig_pixels[1 * 4 + 0] = 16u;
  orig_pixels[1 * 4 + 1] = 64u;
  orig_pixels[1 * 4 + 2] = 192u;
  orig_pixels[1 * 4 + 3] = 0u;
  orig_pixels[2 * 4 + 0] = 16u;
  orig_pixels[2 * 4 + 1] = 64u;
  orig_pixels[2 * 4 + 2] = 192u;
  orig_pixels[2 * 4 + 3] = 255u;
  orig_pixels[3 * 4 + 0] = 16u;
  orig_pixels[3 * 4 + 1] = 64u;
  orig_pixels[3 * 4 + 2] = 192u;
  orig_pixels[3 * 4 + 3] = 128u;
  ARGBUnattenuate(orig_pixels, 0, unatten_pixels, 0, 4, 1);
  EXPECT_EQ(255u, unatten_pixels[0 * 4 + 0]);
  EXPECT_EQ(255u, unatten_pixels[0 * 4 + 1]);
  EXPECT_EQ(254u, unatten_pixels[0 * 4 + 2]);
  EXPECT_EQ(128u, unatten_pixels[0 * 4 + 3]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 0]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 1]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 2]);
  EXPECT_EQ(0u, unatten_pixels[1 * 4 + 3]);
  EXPECT_EQ(16u, unatten_pixels[2 * 4 + 0]);
  EXPECT_EQ(64u, unatten_pixels[2 * 4 + 1]);
  EXPECT_EQ(192u, unatten_pixels[2 * 4 + 2]);
  EXPECT_EQ(255u, unatten_pixels[2 * 4 + 3]);
  EXPECT_EQ(32u, unatten_pixels[3 * 4 + 0]);
  EXPECT_EQ(128u, unatten_pixels[3 * 4 + 1]);
  EXPECT_EQ(255u, unatten_pixels[3 * 4 + 2]);
  EXPECT_EQ(128u, unatten_pixels[3 * 4 + 3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i * 4 + 0] = i;
    orig_pixels[i * 4 + 1] = i / 2;
    orig_pixels[i * 4 + 2] = i / 3;
    orig_pixels[i * 4 + 3] = i;
  }
  ARGBAttenuate(orig_pixels, 0, atten_pixels, 0, 1280, 1);
  ARGBUnattenuate(atten_pixels, 0, unatten_pixels, 0, 1280, 1);
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBAttenuate(unatten_pixels, 0, atten2_pixels, 0, 1280, 1);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_NEAR(atten_pixels[i * 4 + 0], atten2_pixels[i * 4 + 0], 2);
    EXPECT_NEAR(atten_pixels[i * 4 + 1], atten2_pixels[i * 4 + 1], 2);
    EXPECT_NEAR(atten_pixels[i * 4 + 2], atten2_pixels[i * 4 + 2], 2);
    EXPECT_NEAR(atten_pixels[i * 4 + 3], atten2_pixels[i * 4 + 3], 2);
  }
  // Make sure transparent, 50% and opaque are fully accurate.
  EXPECT_EQ(0, atten_pixels[0 * 4 + 0]);
  EXPECT_EQ(0, atten_pixels[0 * 4 + 1]);
  EXPECT_EQ(0, atten_pixels[0 * 4 + 2]);
  EXPECT_EQ(0, atten_pixels[0 * 4 + 3]);
  EXPECT_EQ(64, atten_pixels[128 * 4 + 0]);
  EXPECT_EQ(32, atten_pixels[128 * 4 + 1]);
  EXPECT_EQ(21,  atten_pixels[128 * 4 + 2]);
  EXPECT_EQ(128, atten_pixels[128 * 4 + 3]);
  EXPECT_NEAR(255, atten_pixels[255 * 4 + 0], 1);
  EXPECT_NEAR(127, atten_pixels[255 * 4 + 1], 1);
  EXPECT_NEAR(85,  atten_pixels[255 * 4 + 2], 1);
  EXPECT_EQ(255, atten_pixels[255 * 4 + 3]);

  free_aligned_buffer_64(atten2_pixels);
  free_aligned_buffer_64(unatten_pixels);
  free_aligned_buffer_64(atten_pixels);
  free_aligned_buffer_64(orig_pixels);
}

static int TestAttenuateI(int width, int height, int benchmark_iterations,
                          int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBAttenuate(src_argb + off, kStride,
                dst_argb_c, kStride,
                width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAttenuate(src_argb + off, kStride,
                  dst_argb_opt, kStride,
                  width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBAttenuate_Any) {
  int max_diff = TestAttenuateI(benchmark_width_ - 1, benchmark_height_,
                                benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, ARGBAttenuate_Unaligned) {
  int max_diff = TestAttenuateI(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, +1, 1);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, ARGBAttenuate_Invert) {
  int max_diff = TestAttenuateI(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, -1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, ARGBAttenuate_Opt) {
  int max_diff = TestAttenuateI(benchmark_width_, benchmark_height_,
                                benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

static int TestUnattenuateI(int width, int height, int benchmark_iterations,
                            int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb[i + off] = (random() & 0xff);
  }
  ARGBAttenuate(src_argb + off, kStride,
                src_argb + off, kStride,
                width, height);
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBUnattenuate(src_argb + off, kStride,
                  dst_argb_c, kStride,
                  width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBUnattenuate(src_argb + off, kStride,
                    dst_argb_opt, kStride,
                    width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBUnattenuate_Any) {
  int max_diff = TestUnattenuateI(benchmark_width_ - 1, benchmark_height_,
                                  benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, ARGBUnattenuate_Unaligned) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, +1, 1);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, ARGBUnattenuate_Invert) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, -1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, ARGBUnattenuate_Opt) {
  int max_diff = TestUnattenuateI(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 2);
}

TEST_F(libyuvTest, TestARGBComputeCumulativeSum) {
  SIMD_ALIGNED(uint8 orig_pixels[16][16][4]);
  SIMD_ALIGNED(int32 added_pixels[16][16][4]);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      orig_pixels[y][x][0] = 1u;
      orig_pixels[y][x][1] = 2u;
      orig_pixels[y][x][2] = 3u;
      orig_pixels[y][x][3] = 255u;
    }
  }

  ARGBComputeCumulativeSum(&orig_pixels[0][0][0], 16 * 4,
                           &added_pixels[0][0][0], 16 * 4,
                           16, 16);

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      EXPECT_EQ((x + 1) * (y + 1), added_pixels[y][x][0]);
      EXPECT_EQ((x + 1) * (y + 1) * 2, added_pixels[y][x][1]);
      EXPECT_EQ((x + 1) * (y + 1) * 3, added_pixels[y][x][2]);
      EXPECT_EQ((x + 1) * (y + 1) * 255, added_pixels[y][x][3]);
    }
  }
}

TEST_F(libyuvTest, TestARGBGray) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBGray(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  EXPECT_EQ(30u, orig_pixels[0][0]);
  EXPECT_EQ(30u, orig_pixels[0][1]);
  EXPECT_EQ(30u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(149u, orig_pixels[1][0]);
  EXPECT_EQ(149u, orig_pixels[1][1]);
  EXPECT_EQ(149u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(76u, orig_pixels[2][0]);
  EXPECT_EQ(76u, orig_pixels[2][1]);
  EXPECT_EQ(76u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(0u, orig_pixels[3][0]);
  EXPECT_EQ(0u, orig_pixels[3][1]);
  EXPECT_EQ(0u, orig_pixels[3][2]);
  EXPECT_EQ(255u, orig_pixels[3][3]);
  EXPECT_EQ(255u, orig_pixels[4][0]);
  EXPECT_EQ(255u, orig_pixels[4][1]);
  EXPECT_EQ(255u, orig_pixels[4][2]);
  EXPECT_EQ(255u, orig_pixels[4][3]);
  EXPECT_EQ(96u, orig_pixels[5][0]);
  EXPECT_EQ(96u, orig_pixels[5][1]);
  EXPECT_EQ(96u, orig_pixels[5][2]);
  EXPECT_EQ(224u, orig_pixels[5][3]);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGray(&orig_pixels[0][0], 0, 0, 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestARGBGrayTo) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 gray_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 16, 1);
  EXPECT_EQ(30u, gray_pixels[0][0]);
  EXPECT_EQ(30u, gray_pixels[0][1]);
  EXPECT_EQ(30u, gray_pixels[0][2]);
  EXPECT_EQ(128u, gray_pixels[0][3]);
  EXPECT_EQ(149u, gray_pixels[1][0]);
  EXPECT_EQ(149u, gray_pixels[1][1]);
  EXPECT_EQ(149u, gray_pixels[1][2]);
  EXPECT_EQ(0u, gray_pixels[1][3]);
  EXPECT_EQ(76u, gray_pixels[2][0]);
  EXPECT_EQ(76u, gray_pixels[2][1]);
  EXPECT_EQ(76u, gray_pixels[2][2]);
  EXPECT_EQ(255u, gray_pixels[2][3]);
  EXPECT_EQ(0u, gray_pixels[3][0]);
  EXPECT_EQ(0u, gray_pixels[3][1]);
  EXPECT_EQ(0u, gray_pixels[3][2]);
  EXPECT_EQ(255u, gray_pixels[3][3]);
  EXPECT_EQ(255u, gray_pixels[4][0]);
  EXPECT_EQ(255u, gray_pixels[4][1]);
  EXPECT_EQ(255u, gray_pixels[4][2]);
  EXPECT_EQ(255u, gray_pixels[4][3]);
  EXPECT_EQ(96u, gray_pixels[5][0]);
  EXPECT_EQ(96u, gray_pixels[5][1]);
  EXPECT_EQ(96u, gray_pixels[5][2]);
  EXPECT_EQ(224u, gray_pixels[5][3]);
  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBGrayTo(&orig_pixels[0][0], 0, &gray_pixels[0][0], 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestARGBSepia) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test black
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 255u;
  // Test white
  orig_pixels[4][0] = 255u;
  orig_pixels[4][1] = 255u;
  orig_pixels[4][2] = 255u;
  orig_pixels[4][3] = 255u;
  // Test color
  orig_pixels[5][0] = 16u;
  orig_pixels[5][1] = 64u;
  orig_pixels[5][2] = 192u;
  orig_pixels[5][3] = 224u;
  // Do 16 to test asm version.
  ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 16, 1);
  EXPECT_EQ(33u, orig_pixels[0][0]);
  EXPECT_EQ(43u, orig_pixels[0][1]);
  EXPECT_EQ(47u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(135u, orig_pixels[1][0]);
  EXPECT_EQ(175u, orig_pixels[1][1]);
  EXPECT_EQ(195u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(69u, orig_pixels[2][0]);
  EXPECT_EQ(89u, orig_pixels[2][1]);
  EXPECT_EQ(99u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(0u, orig_pixels[3][0]);
  EXPECT_EQ(0u, orig_pixels[3][1]);
  EXPECT_EQ(0u, orig_pixels[3][2]);
  EXPECT_EQ(255u, orig_pixels[3][3]);
  EXPECT_EQ(239u, orig_pixels[4][0]);
  EXPECT_EQ(255u, orig_pixels[4][1]);
  EXPECT_EQ(255u, orig_pixels[4][2]);
  EXPECT_EQ(255u, orig_pixels[4][3]);
  EXPECT_EQ(88u, orig_pixels[5][0]);
  EXPECT_EQ(114u, orig_pixels[5][1]);
  EXPECT_EQ(127u, orig_pixels[5][2]);
  EXPECT_EQ(224u, orig_pixels[5][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBSepia(&orig_pixels[0][0], 0, 0, 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestARGBColorMatrix) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_c[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8 kRGBToSepia[]) = {
    17 / 2, 68 / 2, 35 / 2, 0,
    22 / 2, 88 / 2, 45 / 2, 0,
    24 / 2, 98 / 2, 50 / 2, 0,
    0, 0, 0, 64,  // Copy alpha.
  };
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                  &kRGBToSepia[0], 16, 1);
  EXPECT_EQ(31u, dst_pixels_opt[0][0]);
  EXPECT_EQ(43u, dst_pixels_opt[0][1]);
  EXPECT_EQ(47u, dst_pixels_opt[0][2]);
  EXPECT_EQ(128u, dst_pixels_opt[0][3]);
  EXPECT_EQ(135u, dst_pixels_opt[1][0]);
  EXPECT_EQ(175u, dst_pixels_opt[1][1]);
  EXPECT_EQ(195u, dst_pixels_opt[1][2]);
  EXPECT_EQ(0u, dst_pixels_opt[1][3]);
  EXPECT_EQ(67u, dst_pixels_opt[2][0]);
  EXPECT_EQ(87u, dst_pixels_opt[2][1]);
  EXPECT_EQ(99u, dst_pixels_opt[2][2]);
  EXPECT_EQ(255u, dst_pixels_opt[2][3]);
  EXPECT_EQ(87u, dst_pixels_opt[3][0]);
  EXPECT_EQ(112u, dst_pixels_opt[3][1]);
  EXPECT_EQ(127u, dst_pixels_opt[3][2]);
  EXPECT_EQ(224u, dst_pixels_opt[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  MaskCpuFlags(0);
  ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                  &kRGBToSepia[0], 1280, 1);
  MaskCpuFlags(-1);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBColorMatrix(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                    &kRGBToSepia[0], 1280, 1);
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    EXPECT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    EXPECT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    EXPECT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }
}

TEST_F(libyuvTest, TestRGBColorMatrix) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);

  // Matrix for Sepia.
  SIMD_ALIGNED(static const int8 kRGBToSepia[]) = {
    17, 68, 35, 0,
    22, 88, 45, 0,
    24, 98, 50, 0,
    0, 0, 0, 0,  // Unused but makes matrix 16 bytes.
  };
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  RGBColorMatrix(&orig_pixels[0][0], 0, &kRGBToSepia[0], 0, 0, 16, 1);
  EXPECT_EQ(31u, orig_pixels[0][0]);
  EXPECT_EQ(43u, orig_pixels[0][1]);
  EXPECT_EQ(47u, orig_pixels[0][2]);
  EXPECT_EQ(128u, orig_pixels[0][3]);
  EXPECT_EQ(135u, orig_pixels[1][0]);
  EXPECT_EQ(175u, orig_pixels[1][1]);
  EXPECT_EQ(195u, orig_pixels[1][2]);
  EXPECT_EQ(0u, orig_pixels[1][3]);
  EXPECT_EQ(67u, orig_pixels[2][0]);
  EXPECT_EQ(87u, orig_pixels[2][1]);
  EXPECT_EQ(99u, orig_pixels[2][2]);
  EXPECT_EQ(255u, orig_pixels[2][3]);
  EXPECT_EQ(87u, orig_pixels[3][0]);
  EXPECT_EQ(112u, orig_pixels[3][1]);
  EXPECT_EQ(127u, orig_pixels[3][2]);
  EXPECT_EQ(224u, orig_pixels[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    RGBColorMatrix(&orig_pixels[0][0], 0, &kRGBToSepia[0], 0, 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestARGBColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8 kARGBTable[256 * 4] = {
    1u, 2u, 3u, 4u,
    5u, 6u, 7u, 8u,
    9u, 10u, 11u, 12u,
    13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  EXPECT_EQ(1u, orig_pixels[0][0]);
  EXPECT_EQ(2u, orig_pixels[0][1]);
  EXPECT_EQ(3u, orig_pixels[0][2]);
  EXPECT_EQ(4u, orig_pixels[0][3]);
  EXPECT_EQ(5u, orig_pixels[1][0]);
  EXPECT_EQ(6u, orig_pixels[1][1]);
  EXPECT_EQ(7u, orig_pixels[1][2]);
  EXPECT_EQ(8u, orig_pixels[1][3]);
  EXPECT_EQ(9u, orig_pixels[2][0]);
  EXPECT_EQ(10u, orig_pixels[2][1]);
  EXPECT_EQ(11u, orig_pixels[2][2]);
  EXPECT_EQ(12u, orig_pixels[2][3]);
  EXPECT_EQ(1u, orig_pixels[3][0]);
  EXPECT_EQ(6u, orig_pixels[3][1]);
  EXPECT_EQ(11u, orig_pixels[3][2]);
  EXPECT_EQ(16u, orig_pixels[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 1280, 1);
  }
}

// Same as TestARGBColorTable except alpha does not change.
TEST_F(libyuvTest, TestRGBColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  // Matrix for Sepia.
  static const uint8 kARGBTable[256 * 4] = {
    1u, 2u, 3u, 4u,
    5u, 6u, 7u, 8u,
    9u, 10u, 11u, 12u,
    13u, 14u, 15u, 16u,
  };

  orig_pixels[0][0] = 0u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 0u;
  orig_pixels[1][0] = 1u;
  orig_pixels[1][1] = 1u;
  orig_pixels[1][2] = 1u;
  orig_pixels[1][3] = 1u;
  orig_pixels[2][0] = 2u;
  orig_pixels[2][1] = 2u;
  orig_pixels[2][2] = 2u;
  orig_pixels[2][3] = 2u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 1u;
  orig_pixels[3][2] = 2u;
  orig_pixels[3][3] = 3u;
  // Do 16 to test asm version.
  RGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 16, 1);
  EXPECT_EQ(1u, orig_pixels[0][0]);
  EXPECT_EQ(2u, orig_pixels[0][1]);
  EXPECT_EQ(3u, orig_pixels[0][2]);
  EXPECT_EQ(0u, orig_pixels[0][3]);  // Alpha unchanged.
  EXPECT_EQ(5u, orig_pixels[1][0]);
  EXPECT_EQ(6u, orig_pixels[1][1]);
  EXPECT_EQ(7u, orig_pixels[1][2]);
  EXPECT_EQ(1u, orig_pixels[1][3]);  // Alpha unchanged.
  EXPECT_EQ(9u, orig_pixels[2][0]);
  EXPECT_EQ(10u, orig_pixels[2][1]);
  EXPECT_EQ(11u, orig_pixels[2][2]);
  EXPECT_EQ(2u, orig_pixels[2][3]);  // Alpha unchanged.
  EXPECT_EQ(1u, orig_pixels[3][0]);
  EXPECT_EQ(6u, orig_pixels[3][1]);
  EXPECT_EQ(11u, orig_pixels[3][2]);
  EXPECT_EQ(3u, orig_pixels[3][3]);  // Alpha unchanged.

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    RGBColorTable(&orig_pixels[0][0], 0, &kARGBTable[0], 0, 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestARGBQuantize) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }
  ARGBQuantize(&orig_pixels[0][0], 0,
               (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0, 1280, 1);

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ((i / 8 * 8 + 8 / 2) & 255, orig_pixels[i][0]);
    EXPECT_EQ((i / 2 / 8 * 8 + 8 / 2) & 255, orig_pixels[i][1]);
    EXPECT_EQ((i / 3 / 8 * 8 + 8 / 2) & 255, orig_pixels[i][2]);
    EXPECT_EQ(i & 255, orig_pixels[i][3]);
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBQuantize(&orig_pixels[0][0], 0,
                 (65536 + (8 / 2)) / 8, 8, 8 / 2, 0, 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestARGBMirror) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i / 4;
  }
  ARGBMirror(&orig_pixels[0][0], 0, &dst_pixels[0][0], 0, 1280, 1);

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(i & 255, dst_pixels[1280 - 1 - i][0]);
    EXPECT_EQ((i / 2) & 255, dst_pixels[1280 - 1 - i][1]);
    EXPECT_EQ((i / 3) & 255, dst_pixels[1280 - 1 - i][2]);
    EXPECT_EQ((i / 4) & 255, dst_pixels[1280 - 1 - i][3]);
  }
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBMirror(&orig_pixels[0][0], 0, &dst_pixels[0][0], 0, 1280, 1);
  }
}

TEST_F(libyuvTest, TestShade) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 shade_pixels[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  orig_pixels[0][0] = 10u;
  orig_pixels[0][1] = 20u;
  orig_pixels[0][2] = 40u;
  orig_pixels[0][3] = 80u;
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 0u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 255u;
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 0u;
  orig_pixels[2][3] = 0u;
  orig_pixels[3][0] = 0u;
  orig_pixels[3][1] = 0u;
  orig_pixels[3][2] = 0u;
  orig_pixels[3][3] = 0u;
  // Do 8 pixels to allow opt version to be used.
  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x80ffffff);
  EXPECT_EQ(10u, shade_pixels[0][0]);
  EXPECT_EQ(20u, shade_pixels[0][1]);
  EXPECT_EQ(40u, shade_pixels[0][2]);
  EXPECT_EQ(40u, shade_pixels[0][3]);
  EXPECT_EQ(0u, shade_pixels[1][0]);
  EXPECT_EQ(0u, shade_pixels[1][1]);
  EXPECT_EQ(0u, shade_pixels[1][2]);
  EXPECT_EQ(128u, shade_pixels[1][3]);
  EXPECT_EQ(0u, shade_pixels[2][0]);
  EXPECT_EQ(0u, shade_pixels[2][1]);
  EXPECT_EQ(0u, shade_pixels[2][2]);
  EXPECT_EQ(0u, shade_pixels[2][3]);
  EXPECT_EQ(0u, shade_pixels[3][0]);
  EXPECT_EQ(0u, shade_pixels[3][1]);
  EXPECT_EQ(0u, shade_pixels[3][2]);
  EXPECT_EQ(0u, shade_pixels[3][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x80808080);
  EXPECT_EQ(5u, shade_pixels[0][0]);
  EXPECT_EQ(10u, shade_pixels[0][1]);
  EXPECT_EQ(20u, shade_pixels[0][2]);
  EXPECT_EQ(40u, shade_pixels[0][3]);

  ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 8, 1, 0x10204080);
  EXPECT_EQ(5u, shade_pixels[0][0]);
  EXPECT_EQ(5u, shade_pixels[0][1]);
  EXPECT_EQ(5u, shade_pixels[0][2]);
  EXPECT_EQ(5u, shade_pixels[0][3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBShade(&orig_pixels[0][0], 0, &shade_pixels[0][0], 0, 1280, 1,
              0x80808080);
  }
}

TEST_F(libyuvTest, TestInterpolate) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8 orig_pixels_1[1280][4]);
  SIMD_ALIGNED(uint8 interpolate_pixels[1280][4]);
  memset(orig_pixels_0, 0, sizeof(orig_pixels_0));
  memset(orig_pixels_1, 0, sizeof(orig_pixels_1));

  orig_pixels_0[0][0] = 16u;
  orig_pixels_0[0][1] = 32u;
  orig_pixels_0[0][2] = 64u;
  orig_pixels_0[0][3] = 128u;
  orig_pixels_0[1][0] = 0u;
  orig_pixels_0[1][1] = 0u;
  orig_pixels_0[1][2] = 0u;
  orig_pixels_0[1][3] = 255u;
  orig_pixels_0[2][0] = 0u;
  orig_pixels_0[2][1] = 0u;
  orig_pixels_0[2][2] = 0u;
  orig_pixels_0[2][3] = 0u;
  orig_pixels_0[3][0] = 0u;
  orig_pixels_0[3][1] = 0u;
  orig_pixels_0[3][2] = 0u;
  orig_pixels_0[3][3] = 0u;

  orig_pixels_1[0][0] = 0u;
  orig_pixels_1[0][1] = 0u;
  orig_pixels_1[0][2] = 0u;
  orig_pixels_1[0][3] = 0u;
  orig_pixels_1[1][0] = 0u;
  orig_pixels_1[1][1] = 0u;
  orig_pixels_1[1][2] = 0u;
  orig_pixels_1[1][3] = 0u;
  orig_pixels_1[2][0] = 0u;
  orig_pixels_1[2][1] = 0u;
  orig_pixels_1[2][2] = 0u;
  orig_pixels_1[2][3] = 0u;
  orig_pixels_1[3][0] = 255u;
  orig_pixels_1[3][1] = 255u;
  orig_pixels_1[3][2] = 255u;
  orig_pixels_1[3][3] = 255u;

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 128);
  EXPECT_EQ(8u, interpolate_pixels[0][0]);
  EXPECT_EQ(16u, interpolate_pixels[0][1]);
  EXPECT_EQ(32u, interpolate_pixels[0][2]);
  EXPECT_EQ(64u, interpolate_pixels[0][3]);
  EXPECT_EQ(0u, interpolate_pixels[1][0]);
  EXPECT_EQ(0u, interpolate_pixels[1][1]);
  EXPECT_EQ(0u, interpolate_pixels[1][2]);
  EXPECT_NEAR(128u, interpolate_pixels[1][3], 1);  // C = 127, SSE = 128.
  EXPECT_EQ(0u, interpolate_pixels[2][0]);
  EXPECT_EQ(0u, interpolate_pixels[2][1]);
  EXPECT_EQ(0u, interpolate_pixels[2][2]);
  EXPECT_EQ(0u, interpolate_pixels[2][3]);
  EXPECT_NEAR(128u, interpolate_pixels[3][0], 1);
  EXPECT_NEAR(128u, interpolate_pixels[3][1], 1);
  EXPECT_NEAR(128u, interpolate_pixels[3][2], 1);
  EXPECT_NEAR(128u, interpolate_pixels[3][3], 1);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 0);
  EXPECT_EQ(16u, interpolate_pixels[0][0]);
  EXPECT_EQ(32u, interpolate_pixels[0][1]);
  EXPECT_EQ(64u, interpolate_pixels[0][2]);
  EXPECT_EQ(128u, interpolate_pixels[0][3]);

  ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                  &interpolate_pixels[0][0], 0, 4, 1, 192);

  EXPECT_EQ(4u, interpolate_pixels[0][0]);
  EXPECT_EQ(8u, interpolate_pixels[0][1]);
  EXPECT_EQ(16u, interpolate_pixels[0][2]);
  EXPECT_EQ(32u, interpolate_pixels[0][3]);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBInterpolate(&orig_pixels_0[0][0], 0, &orig_pixels_1[0][0], 0,
                    &interpolate_pixels[0][0], 0, 1280, 1, 128);
  }
}

#define TESTTERP(FMT_A, BPP_A, STRIDE_A,                                       \
                 FMT_B, BPP_B, STRIDE_B,                                       \
                 W1280, TERP, DIFF, N, NEG, OFF)                               \
TEST_F(libyuvTest, ARGBInterpolate##TERP##N) {                                 \
  const int kWidth = ((W1280) > 0) ? (W1280) : 1;                              \
  const int kHeight = benchmark_height_;                                       \
  const int kStrideA = (kWidth * BPP_A + STRIDE_A - 1) / STRIDE_A * STRIDE_A;  \
  const int kStrideB = (kWidth * BPP_B + STRIDE_B - 1) / STRIDE_B * STRIDE_B;  \
  align_buffer_64(src_argb_a, kStrideA * kHeight + OFF);                       \
  align_buffer_64(src_argb_b, kStrideA * kHeight + OFF);                       \
  align_buffer_64(dst_argb_c, kStrideB * kHeight);                             \
  align_buffer_64(dst_argb_opt, kStrideB * kHeight);                           \
  srandom(time(NULL));                                                         \
  for (int i = 0; i < kStrideA * kHeight; ++i) {                               \
    src_argb_a[i + OFF] = (random() & 0xff);                                   \
    src_argb_b[i + OFF] = (random() & 0xff);                                   \
  }                                                                            \
  MaskCpuFlags(0);                                                             \
  ARGBInterpolate(src_argb_a + OFF, kStrideA,                                  \
                  src_argb_b + OFF, kStrideA,                                  \
                  dst_argb_c, kStrideB,                                        \
                  kWidth, NEG kHeight, TERP);                                  \
  MaskCpuFlags(-1);                                                            \
  for (int i = 0; i < benchmark_iterations_; ++i) {                            \
    ARGBInterpolate(src_argb_a + OFF, kStrideA,                                \
                    src_argb_b + OFF, kStrideA,                                \
                    dst_argb_opt, kStrideB,                                    \
                    kWidth, NEG kHeight, TERP);                                \
  }                                                                            \
  int max_diff = 0;                                                            \
  for (int i = 0; i < kStrideB * kHeight; ++i) {                               \
    int abs_diff =                                                             \
        abs(static_cast<int>(dst_argb_c[i]) -                                  \
            static_cast<int>(dst_argb_opt[i]));                                \
    if (abs_diff > max_diff) {                                                 \
      max_diff = abs_diff;                                                     \
    }                                                                          \
  }                                                                            \
  EXPECT_LE(max_diff, DIFF);                                                   \
  free_aligned_buffer_64(src_argb_a);                                          \
  free_aligned_buffer_64(src_argb_b);                                          \
  free_aligned_buffer_64(dst_argb_c);                                          \
  free_aligned_buffer_64(dst_argb_opt);                                        \
}

#define TESTINTERPOLATE(TERP)                                                  \
    TESTTERP(ARGB, 4, 1, ARGB, 4, 1,                                           \
             benchmark_width_ - 1, TERP, 1, _Any, +, 0)                        \
    TESTTERP(ARGB, 4, 1, ARGB, 4, 1,                                           \
             benchmark_width_, TERP, 1, _Unaligned, +, 1)                      \
    TESTTERP(ARGB, 4, 1, ARGB, 4, 1,                                           \
             benchmark_width_, TERP, 1, _Invert, -, 0)                         \
    TESTTERP(ARGB, 4, 1, ARGB, 4, 1,                                           \
             benchmark_width_, TERP, 1, _Opt, +, 0)                            \
    TESTTERP(ARGB, 4, 1, ARGB, 4, 1,                                           \
             benchmark_width_ - 1, TERP, 1, _Any_Invert, -, 0)

TESTINTERPOLATE(0)
TESTINTERPOLATE(64)
TESTINTERPOLATE(128)
TESTINTERPOLATE(192)
TESTINTERPOLATE(255)

static int TestBlend(int width, int height, int benchmark_iterations,
                     int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = width * kBpp;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(src_argb_b, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
    src_argb_b[i + off] = (random() & 0xff);
  }
  ARGBAttenuate(src_argb_a + off, kStride, src_argb_a + off, kStride, width,
                height);
  ARGBAttenuate(src_argb_b + off, kStride, src_argb_b + off, kStride, width,
                height);
  memset(dst_argb_c, 255, kStride * height);
  memset(dst_argb_opt, 255, kStride * height);

  MaskCpuFlags(0);
  ARGBBlend(src_argb_a + off, kStride,
            src_argb_b + off, kStride,
            dst_argb_c, kStride,
            width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlend(src_argb_a + off, kStride,
              src_argb_b + off, kStride,
              dst_argb_opt, kStride,
              width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(src_argb_b);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBBlend_Any) {
  int max_diff = TestBlend(benchmark_width_ - 4, benchmark_height_,
                           benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlend_Unaligned) {
  int max_diff = TestBlend(benchmark_width_, benchmark_height_,
                           benchmark_iterations_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlend_Invert) {
  int max_diff = TestBlend(benchmark_width_, benchmark_height_,
                           benchmark_iterations_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlend_Opt) {
  int max_diff = TestBlend(benchmark_width_, benchmark_height_,
                           benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, TestAffine) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280][4]);
  SIMD_ALIGNED(uint8 interpolate_pixels_C[1280][4]);

  for (int i = 0; i < 1280; ++i) {
    for (int j = 0; j < 4; ++j) {
      orig_pixels_0[i][j] = i;
    }
  }

  float uv_step[4] = { 0.f, 0.f, 0.75f, 0.f };

  ARGBAffineRow_C(&orig_pixels_0[0][0], 0, &interpolate_pixels_C[0][0],
                  uv_step, 1280);
  EXPECT_EQ(0u, interpolate_pixels_C[0][0]);
  EXPECT_EQ(96u, interpolate_pixels_C[128][0]);
  EXPECT_EQ(191u, interpolate_pixels_C[255][3]);

#if defined(HAS_ARGBAFFINEROW_SSE2)
  SIMD_ALIGNED(uint8 interpolate_pixels_Opt[1280][4]);
  ARGBAffineRow_SSE2(&orig_pixels_0[0][0], 0, &interpolate_pixels_Opt[0][0],
                     uv_step, 1280);
  EXPECT_EQ(0, memcmp(interpolate_pixels_Opt, interpolate_pixels_C, 1280 * 4));

  int has_sse2 = TestCpuFlag(kCpuHasSSE2);
  if (has_sse2) {
    for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
      ARGBAffineRow_SSE2(&orig_pixels_0[0][0], 0, &interpolate_pixels_Opt[0][0],
                         uv_step, 1280);
    }
  }
#endif
}

TEST_F(libyuvTest, TestSobelX) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280 + 2]);
  SIMD_ALIGNED(uint8 orig_pixels_1[1280 + 2]);
  SIMD_ALIGNED(uint8 orig_pixels_2[1280 + 2]);
  SIMD_ALIGNED(uint8 sobel_pixels_c[1280]);
  SIMD_ALIGNED(uint8 sobel_pixels_opt[1280]);

  for (int i = 0; i < 1280 + 2; ++i) {
    orig_pixels_0[i] = i;
    orig_pixels_1[i] = i * 2;
    orig_pixels_2[i] = i * 3;
  }

  SobelXRow_C(orig_pixels_0, orig_pixels_1, orig_pixels_2,
              sobel_pixels_c, 1280);

  EXPECT_EQ(16u, sobel_pixels_c[0]);
  EXPECT_EQ(16u, sobel_pixels_c[100]);
  EXPECT_EQ(255u, sobel_pixels_c[255]);

  void (*SobelXRow)(const uint8* src_y0, const uint8* src_y1,
                    const uint8* src_y2, uint8* dst_sobely, int width) =
      SobelXRow_C;
#if defined(HAS_SOBELXROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SobelXRow = SobelXRow_SSE2;
  }
#endif
#if defined(HAS_SOBELXROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SobelXRow = SobelXRow_NEON;
  }
#endif
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    SobelXRow(orig_pixels_0, orig_pixels_1, orig_pixels_2,
              sobel_pixels_opt, 1280);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(sobel_pixels_c[i], sobel_pixels_opt[i]);
  }
}

TEST_F(libyuvTest, TestSobelY) {
  SIMD_ALIGNED(uint8 orig_pixels_0[1280 + 2]);
  SIMD_ALIGNED(uint8 orig_pixels_1[1280 + 2]);
  SIMD_ALIGNED(uint8 sobel_pixels_c[1280]);
  SIMD_ALIGNED(uint8 sobel_pixels_opt[1280]);

  for (int i = 0; i < 1280 + 2; ++i) {
    orig_pixels_0[i] = i;
    orig_pixels_1[i] = i * 2;
  }

  SobelYRow_C(orig_pixels_0, orig_pixels_1, sobel_pixels_c, 1280);

  EXPECT_EQ(4u, sobel_pixels_c[0]);
  EXPECT_EQ(255u, sobel_pixels_c[100]);
  EXPECT_EQ(0u, sobel_pixels_c[255]);
  void (*SobelYRow)(const uint8* src_y0, const uint8* src_y1,
                    uint8* dst_sobely, int width) = SobelYRow_C;
#if defined(HAS_SOBELYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SobelYRow = SobelYRow_SSE2;
  }
#endif
#if defined(HAS_SOBELYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SobelYRow = SobelYRow_NEON;
  }
#endif
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    SobelYRow(orig_pixels_0, orig_pixels_1, sobel_pixels_opt, 1280);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(sobel_pixels_c[i], sobel_pixels_opt[i]);
  }
}

TEST_F(libyuvTest, TestSobel) {
  SIMD_ALIGNED(uint8 orig_sobelx[1280]);
  SIMD_ALIGNED(uint8 orig_sobely[1280]);
  SIMD_ALIGNED(uint8 sobel_pixels_c[1280 * 4]);
  SIMD_ALIGNED(uint8 sobel_pixels_opt[1280 * 4]);

  for (int i = 0; i < 1280; ++i) {
    orig_sobelx[i] = i;
    orig_sobely[i] = i * 2;
  }

  SobelRow_C(orig_sobelx, orig_sobely, sobel_pixels_c, 1280);

  EXPECT_EQ(0u, sobel_pixels_c[0]);
  EXPECT_EQ(3u, sobel_pixels_c[4]);
  EXPECT_EQ(3u, sobel_pixels_c[5]);
  EXPECT_EQ(3u, sobel_pixels_c[6]);
  EXPECT_EQ(255u, sobel_pixels_c[7]);
  EXPECT_EQ(6u, sobel_pixels_c[8]);
  EXPECT_EQ(6u, sobel_pixels_c[9]);
  EXPECT_EQ(6u, sobel_pixels_c[10]);
  EXPECT_EQ(255u, sobel_pixels_c[7]);
  EXPECT_EQ(255u, sobel_pixels_c[100 * 4 + 1]);
  EXPECT_EQ(255u, sobel_pixels_c[255 * 4 + 1]);
  void (*SobelRow)(const uint8* src_sobelx, const uint8* src_sobely,
                   uint8* dst_argb, int width) = SobelRow_C;
#if defined(HAS_SOBELROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SobelRow = SobelRow_SSE2;
  }
#endif
#if defined(HAS_SOBELROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SobelRow = SobelRow_NEON;
  }
#endif
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    SobelRow(orig_sobelx, orig_sobely, sobel_pixels_opt, 1280);
  }
  for (int i = 0; i < 1280 * 4; ++i) {
    EXPECT_EQ(sobel_pixels_c[i], sobel_pixels_opt[i]);
  }
}

TEST_F(libyuvTest, TestSobelToPlane) {
  SIMD_ALIGNED(uint8 orig_sobelx[1280]);
  SIMD_ALIGNED(uint8 orig_sobely[1280]);
  SIMD_ALIGNED(uint8 sobel_pixels_c[1280]);
  SIMD_ALIGNED(uint8 sobel_pixels_opt[1280]);

  for (int i = 0; i < 1280; ++i) {
    orig_sobelx[i] = i;
    orig_sobely[i] = i * 2;
  }

  SobelToPlaneRow_C(orig_sobelx, orig_sobely, sobel_pixels_c, 1280);

  EXPECT_EQ(0u, sobel_pixels_c[0]);
  EXPECT_EQ(3u, sobel_pixels_c[1]);
  EXPECT_EQ(6u, sobel_pixels_c[2]);
  EXPECT_EQ(99u, sobel_pixels_c[33]);
  EXPECT_EQ(255u, sobel_pixels_c[100]);
  void (*SobelToPlaneRow)(const uint8* src_sobelx, const uint8* src_sobely,
                          uint8* dst_y, int width) = SobelToPlaneRow_C;
#if defined(HAS_SOBELTOPLANEROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SobelToPlaneRow = SobelToPlaneRow_SSE2;
  }
#endif
#if defined(HAS_SOBELTOPLANEROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SobelToPlaneRow = SobelToPlaneRow_NEON;
  }
#endif
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    SobelToPlaneRow(orig_sobelx, orig_sobely, sobel_pixels_opt, 1280);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(sobel_pixels_c[i], sobel_pixels_opt[i]);
  }
}

TEST_F(libyuvTest, TestSobelXY) {
  SIMD_ALIGNED(uint8 orig_sobelx[1280]);
  SIMD_ALIGNED(uint8 orig_sobely[1280]);
  SIMD_ALIGNED(uint8 sobel_pixels_c[1280 * 4]);
  SIMD_ALIGNED(uint8 sobel_pixels_opt[1280 * 4]);

  for (int i = 0; i < 1280; ++i) {
    orig_sobelx[i] = i;
    orig_sobely[i] = i * 2;
  }

  SobelXYRow_C(orig_sobelx, orig_sobely, sobel_pixels_c, 1280);

  EXPECT_EQ(0u, sobel_pixels_c[0]);
  EXPECT_EQ(2u, sobel_pixels_c[4]);
  EXPECT_EQ(3u, sobel_pixels_c[5]);
  EXPECT_EQ(1u, sobel_pixels_c[6]);
  EXPECT_EQ(255u, sobel_pixels_c[7]);
  EXPECT_EQ(255u, sobel_pixels_c[100 * 4 + 1]);
  EXPECT_EQ(255u, sobel_pixels_c[255 * 4 + 1]);
  void (*SobelXYRow)(const uint8* src_sobelx, const uint8* src_sobely,
                       uint8* dst_argb, int width) = SobelXYRow_C;
#if defined(HAS_SOBELXYROW_SSE2)
  if (TestCpuFlag(kCpuHasSSE2)) {
    SobelXYRow = SobelXYRow_SSE2;
  }
#endif
#if defined(HAS_SOBELXYROW_NEON)
  if (TestCpuFlag(kCpuHasNEON)) {
    SobelXYRow = SobelXYRow_NEON;
  }
#endif
  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    SobelXYRow(orig_sobelx, orig_sobely, sobel_pixels_opt, 1280);
  }
  for (int i = 0; i < 1280 * 4; ++i) {
    EXPECT_EQ(sobel_pixels_c[i], sobel_pixels_opt[i]);
  }
}

TEST_F(libyuvTest, TestCopyPlane) {
  int err = 0;
  int yw = benchmark_width_;
  int yh = benchmark_height_;
  int b = 12;
  int i, j;

  int y_plane_size = (yw + b * 2) * (yh + b * 2);
  srandom(time(NULL));
  align_buffer_64(orig_y, y_plane_size);
  align_buffer_64(dst_c, y_plane_size);
  align_buffer_64(dst_opt, y_plane_size);

  memset(orig_y, 0, y_plane_size);
  memset(dst_c, 0, y_plane_size);
  memset(dst_opt, 0, y_plane_size);

  // Fill image buffers with random data.
  for (i = b; i < (yh + b); ++i) {
    for (j = b; j < (yw + b); ++j) {
      orig_y[i * (yw + b * 2) + j] = random() & 0xff;
    }
  }

  // Fill destination buffers with random data.
  for (i = 0; i < y_plane_size; ++i) {
    uint8 random_number = random() & 0x7f;
    dst_c[i] = random_number;
    dst_opt[i] = dst_c[i];
  }

  int y_off = b * (yw + b * 2) + b;

  int y_st = yw + b * 2;
  int stride = 8;

  // Disable all optimizations.
  MaskCpuFlags(0);
  double c_time = get_time();
  for (j = 0; j < benchmark_iterations_; j++) {
    CopyPlane(orig_y + y_off, y_st, dst_c + y_off, stride, yw, yh);
  }
  c_time = (get_time() - c_time) / benchmark_iterations_;

  // Enable optimizations.
  MaskCpuFlags(-1);
  double opt_time = get_time();
  for (j = 0; j < benchmark_iterations_; j++) {
    CopyPlane(orig_y + y_off, y_st, dst_opt + y_off, stride, yw, yh);
  }
  opt_time = (get_time() - opt_time) / benchmark_iterations_;

  for (i = 0; i < y_plane_size; ++i) {
    if (dst_c[i] != dst_opt[i])
      ++err;
  }

  free_aligned_buffer_64(orig_y);
  free_aligned_buffer_64(dst_c);
  free_aligned_buffer_64(dst_opt);

  EXPECT_EQ(0, err);
}

static int TestMultiply(int width, int height, int benchmark_iterations,
                        int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(src_argb_b, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
    src_argb_b[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBMultiply(src_argb_a + off, kStride,
               src_argb_b + off, kStride,
               dst_argb_c, kStride,
               width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBMultiply(src_argb_a + off, kStride,
                 src_argb_b + off, kStride,
                 dst_argb_opt, kStride,
                 width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(src_argb_b);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBMultiply_Any) {
  int max_diff = TestMultiply(benchmark_width_ - 1, benchmark_height_,
                              benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBMultiply_Unaligned) {
  int max_diff = TestMultiply(benchmark_width_, benchmark_height_,
                              benchmark_iterations_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBMultiply_Invert) {
  int max_diff = TestMultiply(benchmark_width_, benchmark_height_,
                              benchmark_iterations_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBMultiply_Opt) {
  int max_diff = TestMultiply(benchmark_width_, benchmark_height_,
                              benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static int TestAdd(int width, int height, int benchmark_iterations,
                   int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(src_argb_b, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
    src_argb_b[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBAdd(src_argb_a + off, kStride,
          src_argb_b + off, kStride,
          dst_argb_c, kStride,
          width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBAdd(src_argb_a + off, kStride,
            src_argb_b + off, kStride,
            dst_argb_opt, kStride,
            width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(src_argb_b);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBAdd_Any) {
  int max_diff = TestAdd(benchmark_width_ - 1, benchmark_height_,
                         benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBAdd_Unaligned) {
  int max_diff = TestAdd(benchmark_width_, benchmark_height_,
                         benchmark_iterations_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBAdd_Invert) {
  int max_diff = TestAdd(benchmark_width_, benchmark_height_,
                         benchmark_iterations_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBAdd_Opt) {
  int max_diff = TestAdd(benchmark_width_, benchmark_height_,
                         benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static int TestSubtract(int width, int height, int benchmark_iterations,
                        int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(src_argb_b, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
    src_argb_b[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBSubtract(src_argb_a + off, kStride,
               src_argb_b + off, kStride,
               dst_argb_c, kStride,
               width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSubtract(src_argb_a + off, kStride,
                 src_argb_b + off, kStride,
                 dst_argb_opt, kStride,
                 width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(src_argb_b);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBSubtract_Any) {
  int max_diff = TestSubtract(benchmark_width_ - 1, benchmark_height_,
                              benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBSubtract_Unaligned) {
  int max_diff = TestSubtract(benchmark_width_, benchmark_height_,
                              benchmark_iterations_, +1, 1);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBSubtract_Invert) {
  int max_diff = TestSubtract(benchmark_width_, benchmark_height_,
                              benchmark_iterations_, -1, 0);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBSubtract_Opt) {
  int max_diff = TestSubtract(benchmark_width_, benchmark_height_,
                              benchmark_iterations_, +1, 0);
  EXPECT_LE(max_diff, 1);
}

static int TestSobel(int width, int height, int benchmark_iterations,
                     int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  memset(src_argb_a, 0, kStride * height + off);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBSobel(src_argb_a + off, kStride,
            dst_argb_c, kStride,
            width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobel(src_argb_a + off, kStride,
              dst_argb_opt, kStride,
              width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBSobel_Any) {
  int max_diff = TestSobel(benchmark_width_ - 1, benchmark_height_,
                           benchmark_iterations_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobel_Unaligned) {
  int max_diff = TestSobel(benchmark_width_, benchmark_height_,
                           benchmark_iterations_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobel_Invert) {
  int max_diff = TestSobel(benchmark_width_, benchmark_height_,
                           benchmark_iterations_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobel_Opt) {
  int max_diff = TestSobel(benchmark_width_, benchmark_height_,
                           benchmark_iterations_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

static int TestSobelToPlane(int width, int height, int benchmark_iterations,
                            int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kSrcBpp = 4;
  const int kDstBpp = 1;
  const int kSrcStride = (width * kSrcBpp + 15) & ~15;
  const int kDstStride = (width * kDstBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kSrcStride * height + off);
  align_buffer_64(dst_argb_c, kDstStride * height);
  align_buffer_64(dst_argb_opt, kDstStride * height);
  memset(src_argb_a, 0, kSrcStride * height + off);
  srandom(time(NULL));
  for (int i = 0; i < kSrcStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kDstStride * height);
  memset(dst_argb_opt, 0, kDstStride * height);

  MaskCpuFlags(0);
  ARGBSobelToPlane(src_argb_a + off, kSrcStride,
                   dst_argb_c, kDstStride,
                   width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobelToPlane(src_argb_a + off, kSrcStride,
                     dst_argb_opt, kDstStride,
                     width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kDstStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBSobelToPlane_Any) {
  int max_diff = TestSobelToPlane(benchmark_width_ - 1, benchmark_height_,
                                  benchmark_iterations_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobelToPlane_Unaligned) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobelToPlane_Invert) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobelToPlane_Opt) {
  int max_diff = TestSobelToPlane(benchmark_width_, benchmark_height_,
                                  benchmark_iterations_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

static int TestSobelXY(int width, int height, int benchmark_iterations,
                     int invert, int off) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  memset(src_argb_a, 0, kStride * height + off);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
  }
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBSobelXY(src_argb_a + off, kStride,
            dst_argb_c, kStride,
            width, invert * height);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBSobelXY(src_argb_a + off, kStride,
              dst_argb_opt, kStride,
              width, invert * height);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

TEST_F(libyuvTest, ARGBSobelXY_Any) {
  int max_diff = TestSobelXY(benchmark_width_ - 1, benchmark_height_,
                             benchmark_iterations_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobelXY_Unaligned) {
  int max_diff = TestSobelXY(benchmark_width_, benchmark_height_,
                             benchmark_iterations_, +1, 1);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobelXY_Invert) {
  int max_diff = TestSobelXY(benchmark_width_, benchmark_height_,
                             benchmark_iterations_, -1, 0);
  EXPECT_EQ(0, max_diff);
}

TEST_F(libyuvTest, ARGBSobelXY_Opt) {
  int max_diff = TestSobelXY(benchmark_width_, benchmark_height_,
                             benchmark_iterations_, +1, 0);
  EXPECT_EQ(0, max_diff);
}

static int TestBlur(int width, int height, int benchmark_iterations,
                    int invert, int off, int radius) {
  if (width < 1) {
    width = 1;
  }
  const int kBpp = 4;
  const int kStride = (width * kBpp + 15) & ~15;
  align_buffer_64(src_argb_a, kStride * height + off);
  align_buffer_64(dst_cumsum, width * height * 16);
  align_buffer_64(dst_argb_c, kStride * height);
  align_buffer_64(dst_argb_opt, kStride * height);
  srandom(time(NULL));
  for (int i = 0; i < kStride * height; ++i) {
    src_argb_a[i + off] = (random() & 0xff);
  }
  memset(dst_cumsum, 0, width * height * 16);
  memset(dst_argb_c, 0, kStride * height);
  memset(dst_argb_opt, 0, kStride * height);

  MaskCpuFlags(0);
  ARGBBlur(src_argb_a + off, kStride,
           dst_argb_c, kStride,
           reinterpret_cast<int32*>(dst_cumsum), width * 4,
           width, invert * height, radius);
  MaskCpuFlags(-1);
  for (int i = 0; i < benchmark_iterations; ++i) {
    ARGBBlur(src_argb_a + off, kStride,
             dst_argb_opt, kStride,
             reinterpret_cast<int32*>(dst_cumsum), width * 4,
             width, invert * height, radius);
  }
  int max_diff = 0;
  for (int i = 0; i < kStride * height; ++i) {
    int abs_diff =
        abs(static_cast<int>(dst_argb_c[i]) -
            static_cast<int>(dst_argb_opt[i]));
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
    }
  }
  free_aligned_buffer_64(src_argb_a);
  free_aligned_buffer_64(dst_cumsum);
  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  return max_diff;
}

static const int kBlurSize = 55;
TEST_F(libyuvTest, ARGBBlur_Any) {
  int max_diff = TestBlur(benchmark_width_ - 1, benchmark_height_,
                          benchmark_iterations_, +1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlur_Unaligned) {
  int max_diff = TestBlur(benchmark_width_, benchmark_height_,
                          benchmark_iterations_, +1, 1, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlur_Invert) {
  int max_diff = TestBlur(benchmark_width_, benchmark_height_,
                          benchmark_iterations_, -1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlur_Opt) {
  int max_diff = TestBlur(benchmark_width_, benchmark_height_,
                          benchmark_iterations_, +1, 0, kBlurSize);
  EXPECT_LE(max_diff, 1);
}

static const int kBlurSmallSize = 5;
TEST_F(libyuvTest, ARGBBlurSmall_Any) {
  int max_diff = TestBlur(benchmark_width_ - 1, benchmark_height_,
                          benchmark_iterations_, +1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlurSmall_Unaligned) {
  int max_diff = TestBlur(benchmark_width_, benchmark_height_,
                          benchmark_iterations_, +1, 1, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlurSmall_Invert) {
  int max_diff = TestBlur(benchmark_width_, benchmark_height_,
                          benchmark_iterations_, -1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, ARGBBlurSmall_Opt) {
  int max_diff = TestBlur(benchmark_width_, benchmark_height_,
                          benchmark_iterations_, +1, 0, kBlurSmallSize);
  EXPECT_LE(max_diff, 1);
}

TEST_F(libyuvTest, TestARGBPolynomial) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_c[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  SIMD_ALIGNED(static const float kWarmifyPolynomial[16]) = {
    0.94230f,  -3.03300f,    -2.92500f,  0.f,  // C0
    0.584500f,  1.112000f,    1.535000f, 1.f,  // C1 x
    0.001313f, -0.002503f,   -0.004496f, 0.f,  // C2 x * x
    0.0f,       0.000006965f, 0.000008781f, 0.f,  // C3 x * x * x
  };

  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test white
  orig_pixels[3][0] = 255u;
  orig_pixels[3][1] = 255u;
  orig_pixels[3][2] = 255u;
  orig_pixels[3][3] = 255u;
  // Test color
  orig_pixels[4][0] = 16u;
  orig_pixels[4][1] = 64u;
  orig_pixels[4][2] = 192u;
  orig_pixels[4][3] = 224u;
  // Do 16 to test asm version.
  ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                 &kWarmifyPolynomial[0], 16, 1);
  EXPECT_EQ(235u, dst_pixels_opt[0][0]);
  EXPECT_EQ(0u, dst_pixels_opt[0][1]);
  EXPECT_EQ(0u, dst_pixels_opt[0][2]);
  EXPECT_EQ(128u, dst_pixels_opt[0][3]);
  EXPECT_EQ(0u, dst_pixels_opt[1][0]);
  EXPECT_EQ(233u, dst_pixels_opt[1][1]);
  EXPECT_EQ(0u, dst_pixels_opt[1][2]);
  EXPECT_EQ(0u, dst_pixels_opt[1][3]);
  EXPECT_EQ(0u, dst_pixels_opt[2][0]);
  EXPECT_EQ(0u, dst_pixels_opt[2][1]);
  EXPECT_EQ(241u, dst_pixels_opt[2][2]);
  EXPECT_EQ(255u, dst_pixels_opt[2][3]);
  EXPECT_EQ(235u, dst_pixels_opt[3][0]);
  EXPECT_EQ(233u, dst_pixels_opt[3][1]);
  EXPECT_EQ(241u, dst_pixels_opt[3][2]);
  EXPECT_EQ(255u, dst_pixels_opt[3][3]);
  EXPECT_EQ(10u, dst_pixels_opt[4][0]);
  EXPECT_EQ(59u, dst_pixels_opt[4][1]);
  EXPECT_EQ(188u, dst_pixels_opt[4][2]);
  EXPECT_EQ(224u, dst_pixels_opt[4][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  MaskCpuFlags(0);
  ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                 &kWarmifyPolynomial[0], 1280, 1);
  MaskCpuFlags(-1);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBPolynomial(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                   &kWarmifyPolynomial[0], 1280, 1);
  }

  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    EXPECT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    EXPECT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    EXPECT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }
}

TEST_F(libyuvTest, TestARGBLumaColorTable) {
  SIMD_ALIGNED(uint8 orig_pixels[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_opt[1280][4]);
  SIMD_ALIGNED(uint8 dst_pixels_c[1280][4]);
  memset(orig_pixels, 0, sizeof(orig_pixels));

  align_buffer_64(lumacolortable, 32768);
  int v = 0;
  for (int i = 0; i < 32768; ++i) {
    lumacolortable[i] = v;
    v += 3;
  }
  // Test blue
  orig_pixels[0][0] = 255u;
  orig_pixels[0][1] = 0u;
  orig_pixels[0][2] = 0u;
  orig_pixels[0][3] = 128u;
  // Test green
  orig_pixels[1][0] = 0u;
  orig_pixels[1][1] = 255u;
  orig_pixels[1][2] = 0u;
  orig_pixels[1][3] = 0u;
  // Test red
  orig_pixels[2][0] = 0u;
  orig_pixels[2][1] = 0u;
  orig_pixels[2][2] = 255u;
  orig_pixels[2][3] = 255u;
  // Test color
  orig_pixels[3][0] = 16u;
  orig_pixels[3][1] = 64u;
  orig_pixels[3][2] = 192u;
  orig_pixels[3][3] = 224u;
  // Do 16 to test asm version.
  ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                     &lumacolortable[0], 16, 1);
  EXPECT_EQ(253u, dst_pixels_opt[0][0]);
  EXPECT_EQ(0u, dst_pixels_opt[0][1]);
  EXPECT_EQ(0u, dst_pixels_opt[0][2]);
  EXPECT_EQ(128u, dst_pixels_opt[0][3]);
  EXPECT_EQ(0u, dst_pixels_opt[1][0]);
  EXPECT_EQ(253u, dst_pixels_opt[1][1]);
  EXPECT_EQ(0u, dst_pixels_opt[1][2]);
  EXPECT_EQ(0u, dst_pixels_opt[1][3]);
  EXPECT_EQ(0u, dst_pixels_opt[2][0]);
  EXPECT_EQ(0u, dst_pixels_opt[2][1]);
  EXPECT_EQ(253u, dst_pixels_opt[2][2]);
  EXPECT_EQ(255u, dst_pixels_opt[2][3]);
  EXPECT_EQ(48u, dst_pixels_opt[3][0]);
  EXPECT_EQ(192u, dst_pixels_opt[3][1]);
  EXPECT_EQ(64u, dst_pixels_opt[3][2]);
  EXPECT_EQ(224u, dst_pixels_opt[3][3]);

  for (int i = 0; i < 1280; ++i) {
    orig_pixels[i][0] = i;
    orig_pixels[i][1] = i / 2;
    orig_pixels[i][2] = i / 3;
    orig_pixels[i][3] = i;
  }

  MaskCpuFlags(0);
  ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_c[0][0], 0,
                     lumacolortable, 1280, 1);
  MaskCpuFlags(-1);

  for (int i = 0; i < benchmark_pixels_div1280_; ++i) {
    ARGBLumaColorTable(&orig_pixels[0][0], 0, &dst_pixels_opt[0][0], 0,
                       lumacolortable, 1280, 1);
  }
  for (int i = 0; i < 1280; ++i) {
    EXPECT_EQ(dst_pixels_c[i][0], dst_pixels_opt[i][0]);
    EXPECT_EQ(dst_pixels_c[i][1], dst_pixels_opt[i][1]);
    EXPECT_EQ(dst_pixels_c[i][2], dst_pixels_opt[i][2]);
    EXPECT_EQ(dst_pixels_c[i][3], dst_pixels_opt[i][3]);
  }

  free_aligned_buffer_64(lumacolortable);
}

TEST_F(libyuvTest, TestARGBCopyAlpha) {
  const int kSize = benchmark_width_ * benchmark_height_ * 4;
  align_buffer_64(orig_pixels, kSize);
  align_buffer_64(dst_pixels_opt, kSize);
  align_buffer_64(dst_pixels_c, kSize);

  MemRandomize(orig_pixels, kSize);
  MemRandomize(dst_pixels_opt, kSize);
  memcpy(dst_pixels_c, dst_pixels_opt, kSize);

  MaskCpuFlags(0);
  ARGBCopyAlpha(orig_pixels, benchmark_width_ * 4,
                dst_pixels_c, benchmark_width_ * 4,
                benchmark_width_, benchmark_height_);
  MaskCpuFlags(-1);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBCopyAlpha(orig_pixels, benchmark_width_ * 4,
                  dst_pixels_opt, benchmark_width_ * 4,
                  benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < kSize; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_64(dst_pixels_c);
  free_aligned_buffer_64(dst_pixels_opt);
  free_aligned_buffer_64(orig_pixels);
}

TEST_F(libyuvTest, TestARGBCopyYToAlpha) {
  const int kPixels = benchmark_width_ * benchmark_height_;
  align_buffer_64(orig_pixels, kPixels);
  align_buffer_64(dst_pixels_opt, kPixels * 4);
  align_buffer_64(dst_pixels_c, kPixels * 4);

  MemRandomize(orig_pixels, kPixels);
  MemRandomize(dst_pixels_opt, kPixels * 4);
  memcpy(dst_pixels_c, dst_pixels_opt, kPixels * 4);

  MaskCpuFlags(0);
  ARGBCopyYToAlpha(orig_pixels, benchmark_width_,
                   dst_pixels_c, benchmark_width_ * 4,
                   benchmark_width_, benchmark_height_);
  MaskCpuFlags(-1);

  for (int i = 0; i < benchmark_iterations_; ++i) {
    ARGBCopyYToAlpha(orig_pixels, benchmark_width_,
                     dst_pixels_opt, benchmark_width_ * 4,
                     benchmark_width_, benchmark_height_);
  }
  for (int i = 0; i < kPixels * 4; ++i) {
    EXPECT_EQ(dst_pixels_c[i], dst_pixels_opt[i]);
  }

  free_aligned_buffer_64(dst_pixels_c);
  free_aligned_buffer_64(dst_pixels_opt);
  free_aligned_buffer_64(orig_pixels);
}

}  // namespace libyuv
