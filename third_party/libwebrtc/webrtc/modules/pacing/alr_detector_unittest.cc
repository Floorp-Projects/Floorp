/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/alr_detector.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace {

constexpr int kEstimatedBitrateBps = 300000;

}  // namespace

namespace webrtc {

namespace {
class SimulateOutgoingTrafficIn {
 public:
  explicit SimulateOutgoingTrafficIn(AlrDetector* alr_detector)
      : alr_detector_(alr_detector) {
    RTC_CHECK(alr_detector_);
  }

  SimulateOutgoingTrafficIn& ForTimeMs(int time_ms) {
    interval_ms_ = rtc::Optional<int>(time_ms);
    interval_ms_.emplace(time_ms);
    ProduceTraffic();
    return *this;
  }

  SimulateOutgoingTrafficIn& AtPercentOfEstimatedBitrate(int usage_percentage) {
    usage_percentage_.emplace(usage_percentage);
    ProduceTraffic();
    return *this;
  }

 private:
  void ProduceTraffic() {
    if (!interval_ms_ || !usage_percentage_)
      return;
    const int kTimeStepMs = 10;
    for (int t = 0; t < *interval_ms_; t += kTimeStepMs) {
      alr_detector_->OnBytesSent(kEstimatedBitrateBps * *usage_percentage_ *
                                     kTimeStepMs / (8 * 100 * 1000),
                                 kTimeStepMs);
    }
    int remainder_ms = *interval_ms_ % kTimeStepMs;
    if (remainder_ms > 0) {
      alr_detector_->OnBytesSent(kEstimatedBitrateBps * *usage_percentage_ *
                                     remainder_ms / (8 * 100 * 1000),
                                 kTimeStepMs);
    }
  }
  AlrDetector* const alr_detector_;
  rtc::Optional<int> interval_ms_;
  rtc::Optional<int> usage_percentage_;
};
}  // namespace

class AlrDetectorTest : public testing::Test {
 public:
  void SetUp() override {
    alr_detector_.SetEstimatedBitrate(kEstimatedBitrateBps);
  }

 protected:
  AlrDetector alr_detector_;
};

TEST_F(AlrDetectorTest, AlrDetection) {
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Stay in non-ALR state when usage is close to 100%.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(1000)
      .AtPercentOfEstimatedBitrate(90);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we ALR starts when bitrate drops below 20%.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(1500)
      .AtPercentOfEstimatedBitrate(20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that ALR ends when usage is above 65%.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(4000)
      .AtPercentOfEstimatedBitrate(100);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());
}

TEST_F(AlrDetectorTest, ShortSpike) {
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we ALR starts when bitrate drops below 20%.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(1000)
      .AtPercentOfEstimatedBitrate(20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we stay in ALR region even after a short bitrate spike.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(100)
      .AtPercentOfEstimatedBitrate(150);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // ALR ends when usage is above 65%.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(3000)
      .AtPercentOfEstimatedBitrate(100);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());
}

TEST_F(AlrDetectorTest, BandwidthEstimateChanges) {
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // ALR starts when bitrate drops below 20%.
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(1000)
      .AtPercentOfEstimatedBitrate(20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // When bandwidth estimate drops the detector should stay in ALR mode and quit
  // it shortly afterwards as the sender continues sending the same amount of
  // traffic. This is necessary to ensure that ProbeController can still react
  // to the BWE drop by initiating a new probe.
  alr_detector_.SetEstimatedBitrate(kEstimatedBitrateBps / 5);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());
  SimulateOutgoingTrafficIn(&alr_detector_)
      .ForTimeMs(1000)
      .AtPercentOfEstimatedBitrate(50);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());
}

TEST_F(AlrDetectorTest, ParseControlFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trial(
      "WebRTC-ProbingScreenshareBwe/Control/");
  rtc::Optional<AlrDetector::AlrExperimentSettings> parsed_params =
      AlrDetector::ParseAlrSettingsFromFieldTrial(
          "WebRTC-ProbingScreenshareBwe");
  EXPECT_FALSE(static_cast<bool>(parsed_params));
}

TEST_F(AlrDetectorTest, ParseActiveFieldTrial) {
  webrtc::test::ScopedFieldTrials field_trial(
      "WebRTC-ProbingScreenshareBwe/1.1,2875,85,20,-20,1/");
  rtc::Optional<AlrDetector::AlrExperimentSettings> parsed_params =
      AlrDetector::ParseAlrSettingsFromFieldTrial(
          "WebRTC-ProbingScreenshareBwe");
  ASSERT_TRUE(static_cast<bool>(parsed_params));
  EXPECT_EQ(1.1f, parsed_params->pacing_factor);
  EXPECT_EQ(2875, parsed_params->max_paced_queue_time);
  EXPECT_EQ(85, parsed_params->alr_bandwidth_usage_percent);
  EXPECT_EQ(20, parsed_params->alr_start_budget_level_percent);
  EXPECT_EQ(-20, parsed_params->alr_stop_budget_level_percent);
  EXPECT_EQ(1, parsed_params->group_id);
}

}  // namespace webrtc
