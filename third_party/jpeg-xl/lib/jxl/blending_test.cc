// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

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

TEST(BlendingTest, Offset) {
  const PaddedBytes background_bytes = ReadTestData("jxl/splines.png");
  CodecInOut background;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(background_bytes), &background));
  const PaddedBytes foreground_bytes =
      ReadTestData("jxl/grayscale_patches.png");
  CodecInOut foreground;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(foreground_bytes), &foreground));

  ImageBlender blender;
  ImageBundle output;
  CodecMetadata nonserialized_metadata;
  ASSERT_TRUE(
      nonserialized_metadata.size.Set(background.xsize(), background.ysize()));
  PassesSharedState state;
  state.frame_header.blending_info.mode = BlendMode::kReplace;
  state.frame_header.blending_info.source = 0;
  state.frame_header.nonserialized_metadata = &nonserialized_metadata;
  state.metadata = &background.metadata;
  state.reference_frames[0].frame = &background.Main();
  PassesDecoderState dec_state;
  dec_state.shared = &state;
  const FrameOrigin foreground_origin = {-50, -50};
  ASSERT_TRUE(blender.PrepareBlending(&dec_state, foreground_origin,
                                      foreground.xsize(), foreground.ysize(),
                                      background.Main().c_current(), &output));

  static constexpr int kStep = 20;
  for (size_t x0 = 0; x0 < foreground.xsize(); x0 += kStep) {
    for (size_t y0 = 0; y0 < foreground.ysize(); y0 += kStep) {
      const Rect rect =
          Rect(x0, y0, kStep, kStep).Intersection(Rect(foreground.Main()));
      Image3F foreground_crop(rect.xsize(), rect.ysize());
      CopyImageTo(rect, *foreground.Main().color(), Rect(foreground_crop),
                  &foreground_crop);
      auto rect_blender =
          blender.PrepareRect(rect, foreground_crop, {}, Rect(foreground_crop));
      for (size_t y = 0; y < rect.ysize(); ++y) {
        ASSERT_TRUE(rect_blender.DoBlending(y));
      }
    }
  }

  const PaddedBytes expected_bytes =
      ReadTestData("jxl/blending/grayscale_patches_on_splines.png");
  CodecInOut expected;
  ASSERT_TRUE(SetFromBytes(Span<const uint8_t>(expected_bytes), &expected));
  VerifyRelativeError(*expected.Main().color(), *output.color(), 1. / (2 * 255),
                      0);
}

}  // namespace
}  // namespace jxl
