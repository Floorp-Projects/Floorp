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

#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/util.h"
#include "test/video_source.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

const int kVideoSourceWidth = 320;
const int kVideoSourceHeight = 240;
const int kFramesToEncode = 2;

class RealtimeTest
    : public ::libaom_test::CodecTestWithParam<libaom_test::TestMode>,
      public ::libaom_test::EncoderTest {
 protected:
  RealtimeTest() : EncoderTest(GET_PARAM(0)), frame_packets_(0) {}
  virtual ~RealtimeTest() {}

  virtual void SetUp() {
    InitializeConfig();
    cfg_.g_lag_in_frames = 0;
    SetMode(::libaom_test::kRealTime);
  }

  virtual void BeginPassHook(unsigned int /*pass*/) {
    // TODO(tomfinegan): We're changing the pass value here to make sure
    // we get frames when real time mode is combined with |g_pass| set to
    // AOM_RC_FIRST_PASS. This is necessary because EncoderTest::RunLoop() sets
    // the pass value based on the mode passed into EncoderTest::SetMode(),
    // which overrides the one specified in SetUp() above.
    cfg_.g_pass = AOM_RC_FIRST_PASS;
  }
  virtual void FramePktHook(const aom_codec_cx_pkt_t * /*pkt*/) {
    frame_packets_++;
  }

  int frame_packets_;
};

TEST_P(RealtimeTest, RealtimeFirstPassProducesFrames) {
  ::libaom_test::RandomVideoSource video;
  video.SetSize(kVideoSourceWidth, kVideoSourceHeight);
  video.set_limit(kFramesToEncode);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  EXPECT_EQ(kFramesToEncode, frame_packets_);
}

AV1_INSTANTIATE_TEST_CASE(RealtimeTest,
                          ::testing::Values(::libaom_test::kRealTime));

}  // namespace
