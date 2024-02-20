// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include <jxl/types.h>

#include <cstdint>
#include <sstream>
#include <utility>
#include <vector>

#include "lib/extras/dec/decode.h"
#include "lib/extras/dec/jxl.h"
#include "lib/extras/packed_image.h"
#include "lib/jxl/base/span.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

using ::testing::SizeIs;

TEST(BlendingTest, Crops) {
  const std::vector<uint8_t> compressed =
      jxl::test::ReadTestData("jxl/blending/cropped_traffic_light.jxl");
  extras::JXLDecompressParams dparams;
  dparams.accepted_formats = {{3, JXL_TYPE_UINT16, JXL_LITTLE_ENDIAN, 0}};
  extras::PackedPixelFile decoded;
  ASSERT_TRUE(DecodeImageJXL(compressed.data(), compressed.size(), dparams,
                             /*decoded_bytes=*/nullptr, &decoded));
  ASSERT_THAT(decoded.frames, SizeIs(4));

  int i = 0;
  for (auto&& decoded_frame : decoded.frames) {
    std::ostringstream filename;
    filename << "jxl/blending/cropped_traffic_light_frame-" << i << ".png";
    const std::vector<uint8_t> compressed_frame =
        jxl::test::ReadTestData(filename.str());
    extras::PackedPixelFile decoded_frame_ppf;
    decoded_frame_ppf.info = decoded.info;
    decoded_frame_ppf.primary_color_representation =
        decoded.primary_color_representation;
    decoded_frame_ppf.color_encoding = decoded.color_encoding;
    decoded_frame_ppf.icc = decoded.icc;
    decoded_frame_ppf.extra_channels_info = decoded.extra_channels_info;
    decoded_frame_ppf.frames.emplace_back(std::move(decoded_frame));
    extras::PackedPixelFile expected_frame_ppf;
    ASSERT_TRUE(extras::DecodeBytes(Bytes(compressed_frame),
                                    extras::ColorHints(), &expected_frame_ppf));
    EXPECT_EQ(0.0f,
              test::ComputeDistance2(decoded_frame_ppf, expected_frame_ppf));
    ++i;
  }
}

}  // namespace
}  // namespace jxl
