/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
*/

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/video_source.h"

namespace {

class AV1FrameSizeTests : public ::testing::Test,
                          public ::libaom_test::EncoderTest {
 protected:
  AV1FrameSizeTests()
      : EncoderTest(&::libaom_test::kAV1), expected_res_(AOM_CODEC_OK) {}
  virtual ~AV1FrameSizeTests() {}

  virtual void SetUp() {
    InitializeConfig();
    SetMode(::libaom_test::kRealTime);
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(expected_res_, res_dec) << decoder->DecodeError();
    return !::testing::Test::HasFailure();
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 1) {
      encoder->Control(AOME_SET_CPUUSED, 7);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
      encoder->Control(AOME_SET_ARNR_MAXFRAMES, 7);
      encoder->Control(AOME_SET_ARNR_STRENGTH, 5);
    }
  }

  int expected_res_;
};

#if CONFIG_SIZE_LIMIT
TEST_F(AV1FrameSizeTests, TestInvalidSizes) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(DECODE_WIDTH_LIMIT + 16, DECODE_HEIGHT_LIMIT + 16);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_CORRUPT_FRAME;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}

TEST_F(AV1FrameSizeTests, LargeValidSizes) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(DECODE_WIDTH_LIMIT, DECODE_HEIGHT_LIMIT);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#endif

TEST_F(AV1FrameSizeTests, OneByOneVideo) {
  ::libaom_test::RandomVideoSource video;

  video.SetSize(1, 1);
  video.set_limit(2);
  expected_res_ = AOM_CODEC_OK;
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
}
#undef ONE_BY_ONE_VIDEO_NAME
}  // namespace
