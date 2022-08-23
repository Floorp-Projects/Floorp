// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_external_image.h"

#include <array>
#include <new>

#include "gtest/gtest.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/data_parallel.h"
#include "lib/jxl/base/thread_pool_internal.h"
#include "lib/jxl/color_encoding_internal.h"
#include "lib/jxl/image_ops.h"
#include "lib/jxl/image_test_utils.h"

namespace jxl {
namespace {

#if !defined(JXL_CRASH_ON_ERROR)
TEST(ExternalImageTest, InvalidSize) {
  ImageMetadata im;
  im.SetAlphaBits(8);
  ImageBundle ib(&im);

  const uint8_t buf[10 * 100 * 8] = {};
  EXPECT_FALSE(ConvertFromExternal(
      Span<const uint8_t>(buf, 10), /*xsize=*/10, /*ysize=*/100,
      /*c_current=*/ColorEncoding::SRGB(), /*channels=*/4,
      /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16, JXL_BIG_ENDIAN,
      nullptr, &ib, /*float_in=*/false, /*align=*/0));
  EXPECT_FALSE(ConvertFromExternal(
      Span<const uint8_t>(buf, sizeof(buf) - 1), /*xsize=*/10, /*ysize=*/100,
      /*c_current=*/ColorEncoding::SRGB(), /*channels=*/4,
      /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16, JXL_BIG_ENDIAN,
      nullptr, &ib, /*float_in=*/false, /*align=*/0));
  EXPECT_TRUE(
      ConvertFromExternal(Span<const uint8_t>(buf, sizeof(buf)), /*xsize=*/10,
                          /*ysize=*/100, /*c_current=*/ColorEncoding::SRGB(),
                          /*channels=*/4, /*alpha_is_premultiplied=*/false,
                          /*bits_per_sample=*/16, JXL_BIG_ENDIAN, nullptr, &ib,
                          /*float_in=*/false, /*align=*/0));
}
#endif

TEST(ExternalImageTest, AlphaMissing) {
  ImageMetadata im;
  im.SetAlphaBits(0);  // No alpha
  ImageBundle ib(&im);

  const size_t xsize = 10;
  const size_t ysize = 20;
  const uint8_t buf[xsize * ysize * 4] = {};

  // has_alpha is true but the ImageBundle has no alpha. Alpha channel should
  // be ignored.
  EXPECT_TRUE(
      ConvertFromExternal(Span<const uint8_t>(buf, sizeof(buf)), xsize, ysize,
                          /*c_current=*/ColorEncoding::SRGB(),
                          /*channels=*/4, /*alpha_is_premultiplied=*/false,
                          /*bits_per_sample=*/8, JXL_BIG_ENDIAN, nullptr, &ib,
                          /*float_in=*/false, /*align=*/0));
  EXPECT_FALSE(ib.HasAlpha());
}

}  // namespace
}  // namespace jxl
