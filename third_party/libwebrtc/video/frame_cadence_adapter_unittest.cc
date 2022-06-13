/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_cadence_adapter.h"

#include <utility>
#include <vector>

#include "api/task_queue/task_queue_base.h"
#include "api/video/nv12_buffer.h"
#include "api/video/video_frame.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/ref_counted_object.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::Pair;
using ::testing::Ref;
using ::testing::UnorderedElementsAre;

VideoFrame CreateFrame() {
  return VideoFrame::Builder()
      .set_video_frame_buffer(
          rtc::make_ref_counted<NV12Buffer>(/*width=*/16, /*height=*/16))
      .build();
}

std::unique_ptr<FrameCadenceAdapterInterface> CreateAdapter(Clock* clock) {
  return FrameCadenceAdapterInterface::Create(clock, TaskQueueBase::Current());
}

class MockCallback : public FrameCadenceAdapterInterface::Callback {
 public:
  MOCK_METHOD(void, OnFrame, (Timestamp, int, const VideoFrame&), (override));
  MOCK_METHOD(void, OnDiscardedFrame, (), (override));
};

class ZeroHertzFieldTrialDisabler : public test::ScopedFieldTrials {
 public:
  ZeroHertzFieldTrialDisabler()
      : test::ScopedFieldTrials("WebRTC-ZeroHertzScreenshare/Disabled/") {}
};

class ZeroHertzFieldTrialEnabler : public test::ScopedFieldTrials {
 public:
  ZeroHertzFieldTrialEnabler()
      : test::ScopedFieldTrials("WebRTC-ZeroHertzScreenshare/Enabled/") {}
};

TEST(FrameCadenceAdapterTest,
     ForwardsFramesOnConstructionAndUnderDisabledFieldTrial) {
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(1));
  auto disabler = std::make_unique<ZeroHertzFieldTrialDisabler>();
  for (int i = 0; i != 2; i++) {
    MockCallback callback;
    auto adapter = CreateAdapter(time_controller.GetClock());
    adapter->Initialize(&callback);
    VideoFrame frame = CreateFrame();
    EXPECT_CALL(callback, OnFrame).Times(1);
    adapter->OnFrame(frame);
    time_controller.AdvanceTime(TimeDelta::Zero());
    Mock::VerifyAndClearExpectations(&callback);
    EXPECT_CALL(callback, OnDiscardedFrame).Times(1);
    adapter->OnDiscardedFrame();
    Mock::VerifyAndClearExpectations(&callback);

    disabler = nullptr;
  }
}

TEST(FrameCadenceAdapterTest, CountsOutstandingFramesToProcess) {
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(1));
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller.GetClock());
  adapter->Initialize(&callback);
  EXPECT_CALL(callback, OnFrame(_, 2, _)).Times(1);
  EXPECT_CALL(callback, OnFrame(_, 1, _)).Times(1);
  auto frame = CreateFrame();
  adapter->OnFrame(frame);
  adapter->OnFrame(frame);
  time_controller.AdvanceTime(TimeDelta::Zero());
  EXPECT_CALL(callback, OnFrame(_, 1, _)).Times(1);
  adapter->OnFrame(frame);
  time_controller.AdvanceTime(TimeDelta::Zero());
}

TEST(FrameCadenceAdapterTest, FrameRateFollowsRateStatisticsByDefault) {
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(0));
  auto adapter = CreateAdapter(time_controller.GetClock());
  adapter->Initialize(nullptr);

  // Create an "oracle" rate statistics which should be followed on a sequence
  // of frames.
  RateStatistics rate(
      FrameCadenceAdapterInterface::kFrameRateAveragingWindowSizeMs, 1000);

  for (int frame = 0; frame != 10; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
    adapter->UpdateFrameRate();
    EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
              adapter->GetInputFrameRateFps())
        << " failed for frame " << frame;
  }
}

TEST(FrameCadenceAdapterTest,
     FrameRateFollowsRateStatisticsWhenFeatureDisabled) {
  ZeroHertzFieldTrialDisabler feature_disabler;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(0));
  auto adapter = CreateAdapter(time_controller.GetClock());
  adapter->Initialize(nullptr);

  // Create an "oracle" rate statistics which should be followed on a sequence
  // of frames.
  RateStatistics rate(
      FrameCadenceAdapterInterface::kFrameRateAveragingWindowSizeMs, 1000);

  for (int frame = 0; frame != 10; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
    adapter->UpdateFrameRate();
    EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
              adapter->GetInputFrameRateFps())
        << " failed for frame " << frame;
  }
}

TEST(FrameCadenceAdapterTest, FrameRateFollowsMaxFpsWhenZeroHertzActivated) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(0));
  auto adapter = CreateAdapter(time_controller.GetClock());
  adapter->Initialize(nullptr);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  for (int frame = 0; frame != 10; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    adapter->UpdateFrameRate();
    EXPECT_EQ(adapter->GetInputFrameRateFps(), 1u);
  }
}

TEST(FrameCadenceAdapterTest,
     FrameRateFollowsRateStatisticsAfterZeroHertzDeactivated) {
  ZeroHertzFieldTrialEnabler enabler;
  MockCallback callback;
  GlobalSimulatedTimeController time_controller(Timestamp::Millis(0));
  auto adapter = CreateAdapter(time_controller.GetClock());
  adapter->Initialize(nullptr);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{0, 1});
  RateStatistics rate(
      FrameCadenceAdapterInterface::kFrameRateAveragingWindowSizeMs, 1000);
  constexpr int MAX = 10;
  for (int frame = 0; frame != MAX; ++frame) {
    time_controller.AdvanceTime(TimeDelta::Millis(10));
    rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
    adapter->UpdateFrameRate();
  }
  // Turn off zero hertz on the next-last frame; after the last frame we
  // should see a value that tracks the rate oracle.
  adapter->SetZeroHertzModeEnabled(false);
  // Last frame.
  time_controller.AdvanceTime(TimeDelta::Millis(10));
  rate.Update(1, time_controller.GetClock()->TimeInMilliseconds());
  adapter->UpdateFrameRate();

  EXPECT_EQ(rate.Rate(time_controller.GetClock()->TimeInMilliseconds()),
            adapter->GetInputFrameRateFps());
}

class FrameCadenceAdapterMetricsTest : public ::testing::Test {
 public:
  FrameCadenceAdapterMetricsTest() : time_controller_(Timestamp::Millis(1)) {
    metrics::Reset();
  }
  void DepleteTaskQueues() { time_controller_.AdvanceTime(TimeDelta::Zero()); }

 protected:
  GlobalSimulatedTimeController time_controller_;
};

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoUmasWithNoFrameTransfer) {
  MockCallback callback;
  auto adapter = CreateAdapter(nullptr);
  adapter->Initialize(&callback);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{absl::nullopt, 1});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{2, 3});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4, 4});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5, absl::nullopt});
  DepleteTaskQueues();
  EXPECT_TRUE(metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoUmasWithoutEnabledContentType) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->OnFrame(CreateFrame());
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{absl::nullopt, 1});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{2, 3});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4, 4});
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5, absl::nullopt});
  DepleteTaskQueues();
  EXPECT_TRUE(metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsNoConstraintsIfUnsetOnFrame) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists"),
      ElementsAre(Pair(false, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsEmptyConstraintsIfSetOnFrame) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, absl::nullopt});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{absl::nullopt, 2.0});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value")
          .empty());
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value"),
      ElementsAre(Pair(2.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max"),
      ElementsAre(Pair(2.0, 1)));
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(
      VideoTrackSourceConstraints{3.0, absl::nullopt});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value"),
      ElementsAre(Pair(3.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(false, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value")
          .empty());
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_TRUE(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne")
          .empty());
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinGtMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{5.0, 4.0});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Min.Value"),
      ElementsAre(Pair(5.0, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Exists"),
      ElementsAre(Pair(true, 1)));
  EXPECT_THAT(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.Max.Value"),
      ElementsAre(Pair(4.0, 1)));
  EXPECT_TRUE(
      metrics::Samples("WebRTC.Screenshare.FrameRateConstraints.MinUnset.Max")
          .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min")
                  .empty());
  EXPECT_TRUE(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max")
                  .empty());
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne"),
      ElementsAre(Pair(60 * 5.0 + 4.0 - 1, 1)));
}

TEST_F(FrameCadenceAdapterMetricsTest, RecordsMinLtMaxConstraintIfSetOnFrame) {
  MockCallback callback;
  auto adapter = CreateAdapter(time_controller_.GetClock());
  adapter->Initialize(&callback);
  adapter->SetZeroHertzModeEnabled(true);
  adapter->OnConstraintsChanged(VideoTrackSourceConstraints{4.0, 5.0});
  adapter->OnFrame(CreateFrame());
  DepleteTaskQueues();
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Min"),
              ElementsAre(Pair(4.0, 1)));
  EXPECT_THAT(metrics::Samples(
                  "WebRTC.Screenshare.FrameRateConstraints.MinLessThanMax.Max"),
              ElementsAre(Pair(5.0, 1)));
  EXPECT_THAT(
      metrics::Samples(
          "WebRTC.Screenshare.FrameRateConstraints.60MinPlusMaxMinusOne"),
      ElementsAre(Pair(60 * 4.0 + 5.0 - 1, 1)));
}

}  // namespace
}  // namespace webrtc
