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

#include "lib/jxl/blending.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "lib/extras/codec.h"
#include "lib/jxl/dec_file.h"
#include "lib/jxl/image_test_utils.h"
#include "lib/jxl/testdata.h"

namespace jxl {
namespace {

using ::testing::SizeIs;

TEST(BlendingTest, Crops) {
  ThreadPool* pool = nullptr;

  const PaddedBytes compressed =
      ReadTestData("jxl/blending/cropped_traffic_light.jxl");
  DecompressParams dparams;
  CodecInOut decoded;
  ASSERT_TRUE(DecodeFile(dparams, compressed, &decoded, pool));
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
