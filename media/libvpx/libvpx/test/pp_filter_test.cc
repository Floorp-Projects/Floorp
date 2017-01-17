/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "third_party/googletest/src/include/gtest/gtest.h"
#include "./vpx_config.h"
#include "./vp8_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_mem/vpx_mem.h"

typedef void (*PostProcFunc)(unsigned char *src_ptr,
                             unsigned char *dst_ptr,
                             int src_pixels_per_line,
                             int dst_pixels_per_line,
                             int cols,
                             unsigned char *flimit,
                             int size);

namespace {

class VP8PostProcessingFilterTest
    : public ::testing::TestWithParam<PostProcFunc> {
 public:
  virtual void TearDown() {
    libvpx_test::ClearSystemState();
  }
};

// Test routine for the VP8 post-processing function
// vp8_post_proc_down_and_across_mb_row_c.

TEST_P(VP8PostProcessingFilterTest, FilterOutputCheck) {
  // Size of the underlying data block that will be filtered.
  const int block_width  = 16;
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

  uint8_t *const src_image =
      reinterpret_cast<uint8_t*>(vpx_calloc(input_size, 1));
  uint8_t *const dst_image =
      reinterpret_cast<uint8_t*>(vpx_calloc(output_size, 1));

  // Pointers to top-left pixel of block in the input and output images.
  uint8_t *const src_image_ptr = src_image + (input_stride << 1);
  uint8_t *const dst_image_ptr = dst_image + 8;
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

  ASM_REGISTER_STATE_CHECK(
      GetParam()(src_image_ptr, dst_image_ptr, input_stride,
                 output_stride, block_width, flimits, 16));

  static const uint8_t expected_data[block_height] = {
    4, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 4
  };

  pixel_ptr = dst_image_ptr;
  for (int i = 0; i < block_height; ++i) {
    for (int j = 0; j < block_width; ++j) {
      EXPECT_EQ(expected_data[i], pixel_ptr[j])
          << "VP8PostProcessingFilterTest failed with invalid filter output";
    }
    pixel_ptr += output_stride;
  }

  vpx_free(src_image);
  vpx_free(dst_image);
  vpx_free(flimits);
};

INSTANTIATE_TEST_CASE_P(C, VP8PostProcessingFilterTest,
    ::testing::Values(vp8_post_proc_down_and_across_mb_row_c));

#if HAVE_SSE2
INSTANTIATE_TEST_CASE_P(SSE2, VP8PostProcessingFilterTest,
    ::testing::Values(vp8_post_proc_down_and_across_mb_row_sse2));
#endif

#if HAVE_MSA
INSTANTIATE_TEST_CASE_P(MSA, VP8PostProcessingFilterTest,
    ::testing::Values(vp8_post_proc_down_and_across_mb_row_msa));
#endif

}  // namespace
