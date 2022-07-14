/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/test/create_videocodec_test_fixture.h"
#include "api/test/video/function_video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/media_constants.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/codecs/av1/libaom_av1_decoder.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {
// Test clips settings.
constexpr int kCifWidth = 352;
constexpr int kCifHeight = 288;
constexpr int kNumFramesLong = 300;

VideoCodecTestFixture::Config CreateConfig(std::string filename) {
  VideoCodecTestFixture::Config config;
  config.filename = filename;
  config.filepath = ResourcePath(config.filename, "yuv");
  config.num_frames = kNumFramesLong;
  config.use_single_core = true;
  return config;
}

class VideoCodecTestAv1 : public ::testing::TestWithParam<std::string> {
 public:
  VideoCodecTestAv1() : scoped_field_trial_(GetParam()) {}

 private:
  ScopedFieldTrials scoped_field_trial_;
};

TEST_P(VideoCodecTestAv1, HighBitrate) {
  auto config = CreateConfig("foreman_cif");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kCifWidth, kCifHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{500, 30, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {12, 1, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{37, 34, 0.94, 0.91}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

TEST_P(VideoCodecTestAv1, VeryLowBitrate) {
  auto config = CreateConfig("foreman_cif");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kCifWidth, kCifHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{50, 30, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {15, 8, 75, 2, 2, 2, 2, 1}};

  std::vector<QualityThresholds> quality_thresholds = {{28, 24.8, 0.70, 0.55}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}

#if !defined(WEBRTC_ANDROID)
constexpr int kHdWidth = 1280;
constexpr int kHdHeight = 720;
TEST_P(VideoCodecTestAv1, Hd) {
  auto config = CreateConfig("ConferenceMotion_1280_720_50");
  config.SetCodecSettings(cricket::kAv1CodecName, 1, 1, 1, false, true, true,
                          kHdWidth, kHdHeight);
  config.codec_settings.SetScalabilityMode(ScalabilityMode::kL1T1);
  config.num_frames = kNumFramesLong;
  auto fixture = CreateVideoCodecTestFixture(config);

  std::vector<RateProfile> rate_profiles = {{1000, 50, 0}};

  std::vector<RateControlThresholds> rc_thresholds = {
      {13, 3, 0, 1, 0.3, 0.1, 0, 1}};

  std::vector<QualityThresholds> quality_thresholds = {
      {35.9, 31.55, 0.925, 0.865}};

  fixture->RunTest(rate_profiles, &rc_thresholds, &quality_thresholds, nullptr);
}
#endif

std::vector<std::string> GetTestValues() {
  std::vector<std::string> field_trial_values;
#if defined(RTC_DAV1D_IN_INTERNAL_DECODER_FACTORY)
  field_trial_values.push_back("WebRTC-Dav1dDecoder/Enabled/");
#endif
  if (kIsLibaomAv1DecoderSupported) {
    // As long as the field trial doesn't enable dav1d the libaom decoder will
    // be used instead.
    field_trial_values.push_back("");
  }
  return field_trial_values;
}

INSTANTIATE_TEST_SUITE_P(Decoder,
                         VideoCodecTestAv1,
                         testing::ValuesIn(GetTestValues()));

}  // namespace
}  // namespace test
}  // namespace webrtc
