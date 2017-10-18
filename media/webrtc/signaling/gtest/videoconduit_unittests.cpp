/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "VideoConduit.h"

#include "webrtc/media/base/videoadapter.h"

using namespace mozilla;

namespace test {

class MockVideoAdapter : public cricket::VideoAdapter {
public:

  bool AdaptFrameResolution(int in_width,
                            int in_height,
                            int64_t in_timestamp_ns,
                            int* cropped_width,
                            int* cropped_height,
                            int* out_width,
                            int* out_height) override
  {
    mInWidth = in_width;
    mInHeight = in_height;
    mInTimestampNs = in_timestamp_ns;
    return true;
  }

  void OnResolutionRequest(rtc::Optional<int> max_pixel_count,
                           rtc::Optional<int> max_pixel_count_step_up) override
  {
    mMaxPixelCount = max_pixel_count.value_or(-1);
    mMaxPixelCountStepUp = max_pixel_count_step_up.value_or(-1);
  }

  void OnScaleResolutionBy(rtc::Optional<float> scale_resolution_by) override
  {
    mScaleResolutionBy = scale_resolution_by.value_or(-1.0);
  }

  int mInWidth;
  int mInHeight;
  int64_t mInTimestampNs;
  int mMaxPixelCount;
  int mMaxPixelCountStepUp;
  int mScaleResolutionBy;
};

class VideoConduitTest : public ::testing::Test {
};

TEST_F(VideoConduitTest, TestOnSinkWantsChanged)
{
  RefPtr<mozilla::WebrtcVideoConduit> videoConduit;
  MockVideoAdapter* adapter = new MockVideoAdapter;
  videoConduit = new WebrtcVideoConduit(WebRtcCallWrapper::Create(),
                                        UniquePtr<cricket::VideoAdapter>(adapter));

  rtc::VideoSinkWants wants;
  wants.max_pixel_count = rtc::Optional<int>(256000);
  EncodingConstraints constraints;
  VideoCodecConfig codecConfig(120, "VP8", constraints);

  codecConfig.mEncodingConstraints.maxFs = 0;
  videoConduit->ConfigureSendMediaCodec(&codecConfig);
  videoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(adapter->mMaxPixelCount, 256000);

  codecConfig.mEncodingConstraints.maxFs = 500;
  videoConduit->ConfigureSendMediaCodec(&codecConfig);
  videoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(adapter->mMaxPixelCount, 500*16*16); //convert macroblocks to pixels

  codecConfig.mEncodingConstraints.maxFs = 1000;
  videoConduit->ConfigureSendMediaCodec(&codecConfig);
  videoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(adapter->mMaxPixelCount, 256000);

  wants.max_pixel_count = rtc::Optional<int>(64000);
  codecConfig.mEncodingConstraints.maxFs = 500;
  videoConduit->ConfigureSendMediaCodec(&codecConfig);
  videoConduit->OnSinkWantsChanged(wants);
  ASSERT_EQ(adapter->mMaxPixelCount, 64000);
}

} // End namespace test.
