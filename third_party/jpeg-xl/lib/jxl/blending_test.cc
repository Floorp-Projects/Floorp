// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/extras/codec.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {

using ::testing::SizeIs;

TEST(BlendingTest, Crops) {
  ThreadPool* pool = nullptr;

  const PaddedBytes compressed =
      ReadTestData("jxl/blending/cropped_traffic_light.jxl");
  CodecInOut decoded;
  ASSERT_TRUE(test::DecodeFile({}, compressed, &decoded, pool));
  ASSERT_THAT(decoded.frames, SizeIs(4));

  int i = 0;
  for (const ImageBundle& ib : decoded.frames) {
    std::ostringstream filename;
    filename << "jxl/blending/cropped_traffic_light_frame-" << i << ".png";
    const PaddedBytes compressed_frame = ReadTestData(filename.str());
    CodecInOut frame;
    ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(compressed_frame), &frame));
    EXPECT_TRUE(SamePixels(ib.color(), *frame.Main().color()));
    ++i;
  }
}

}  // namespace
}  // namespace jxl
