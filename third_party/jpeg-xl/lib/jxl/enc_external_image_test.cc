// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
      /*c_current=*/ColorEncoding::SRGB(), /*has_alpha=*/true,
      /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16, JXL_BIG_ENDIAN,
      /*flipped_y=*/false, nullptr, &ib));
  EXPECT_FALSE(ConvertFromExternal(
      Span<const uint8_t>(buf, sizeof(buf) - 1), /*xsize=*/10, /*ysize=*/100,
      /*c_current=*/ColorEncoding::SRGB(), /*has_alpha=*/true,
      /*alpha_is_premultiplied=*/false, /*bits_per_sample=*/16, JXL_BIG_ENDIAN,
      /*flipped_y=*/false, nullptr, &ib));
  EXPECT_TRUE(
      ConvertFromExternal(Span<const uint8_t>(buf, sizeof(buf)), /*xsize=*/10,
                          /*ysize=*/100, /*c_current=*/ColorEncoding::SRGB(),
                          /*has_alpha=*/true, /*alpha_is_premultiplied=*/false,
                          /*bits_per_sample=*/16, JXL_BIG_ENDIAN,
                          /*flipped_y=*/false, nullptr, &ib));
}
#endif

}  // namespace
}  // namespace jxl
