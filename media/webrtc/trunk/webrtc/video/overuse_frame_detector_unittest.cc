/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/overuse_frame_detector.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace {
  const int kWidth = 640;
  const int kHeight = 480;
  const int kFrameInterval33ms = 33;
  const int kProcessIntervalMs = 5000;
  const int kProcessTime5ms = 5;
}  // namespace

class MockCpuOveruseObserver : public CpuOveruseObserver {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD0(OveruseDetected, void());
  MOCK_METHOD0(NormalUsage, void());
};

class CpuOveruseObserverImpl : public CpuOveruseObserver {
 public:
  CpuOveruseObserverImpl() :
    overuse_(0),
    normaluse_(0) {}
  virtual ~CpuOveruseObserverImpl() {}

  void OveruseDetected() { ++overuse_; }
  void NormalUsage() { ++normaluse_; }

  int overuse_;
  int normaluse_;
};

class OveruseFrameDetectorTest : public ::testing::Test,
                                 public CpuOveruseMetricsObserver {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockCpuOveruseObserver());
    options_.min_process_count = 0;
    ReinitializeOveruseDetector();
  }

  void ReinitializeOveruseDetector() {
    overuse_detector_.reset(new OveruseFrameDetector(clock_.get(), options_,
                                                     observer_.get(), this));
  }

  void CpuOveruseMetricsUpdated(const CpuOveruseMetrics& metrics) override {
    metrics_ = metrics;
  }

  int InitialUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) / 2.0f) + 0.5;
  }

  void InsertAndSendFramesWithInterval(
      int num_frames, int interval_ms, int width, int height, int delay_ms) {
    while (num_frames-- > 0) {
      int64_t capture_time_ms = clock_->TimeInMilliseconds();
      overuse_detector_->FrameCaptured(width, height, capture_time_ms);
      clock_->AdvanceTimeMilliseconds(delay_ms);
      overuse_detector_->FrameSent(capture_time_ms);
      clock_->AdvanceTimeMilliseconds(interval_ms - delay_ms);
    }
  }

  void TriggerOveruse(int num_times) {
    const int kDelayMs = 32;
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(
          1000, kFrameInterval33ms, kWidth, kHeight, kDelayMs);
      overuse_detector_->Process();
    }
  }

  void TriggerUnderuse() {
    const int kDelayMs1 = 5;
    const int kDelayMs2 = 6;
    InsertAndSendFramesWithInterval(
        1300, kFrameInterval33ms, kWidth, kHeight, kDelayMs1);
    InsertAndSendFramesWithInterval(
        1, kFrameInterval33ms, kWidth, kHeight, kDelayMs2);
    overuse_detector_->Process();
  }

  int UsagePercent() { return metrics_.encode_usage_percent; }

  CpuOveruseOptions options_;
  rtc::scoped_ptr<SimulatedClock> clock_;
  rtc::scoped_ptr<MockCpuOveruseObserver> observer_;
  rtc::scoped_ptr<OveruseFrameDetector> overuse_detector_;
  CpuOveruseMetrics metrics_;
};


// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithNoObserver) {
  overuse_detector_.reset(
      new OveruseFrameDetector(clock_.get(), options_, nullptr, this));
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(2);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, TriggerUnderuseWithMinProcessCount) {
  options_.min_process_count = 1;
  CpuOveruseObserverImpl overuse_observer;
  overuse_detector_.reset(new OveruseFrameDetector(clock_.get(), options_,
                                                   &overuse_observer, this));
  InsertAndSendFramesWithInterval(
      1200, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  overuse_detector_->Process();
  EXPECT_EQ(0, overuse_observer.normaluse_);
  clock_->AdvanceTimeMilliseconds(kProcessIntervalMs);
  overuse_detector_->Process();
  EXPECT_EQ(1, overuse_observer.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(64);
  for (size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  options_.high_threshold_consecutive_count = 2;
  ReinitializeOveruseDetector();
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_EQ(kProcessTime5ms * 100 / kFrameInterval33ms, UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterResolutionChange) {
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      1, kFrameInterval33ms, kWidth, kHeight + 1, kProcessTime5ms);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ResetAfterFrameTimeout) {
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
  // Verify reset.
  InsertAndSendFramesWithInterval(
      2, options_.frame_timeout_interval_ms + 1, kWidth, kHeight,
      kProcessTime5ms);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, MinFrameSamplesBeforeUpdating) {
  options_.min_frame_samples = 40;
  ReinitializeOveruseDetector();
  InsertAndSendFramesWithInterval(
      40, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  InsertAndSendFramesWithInterval(
      1, kFrameInterval33ms, kWidth, kHeight, kProcessTime5ms);
  EXPECT_NE(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_OneFrame) {
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  EXPECT_EQ(-1, overuse_detector_->LastProcessingTimeMs());
  overuse_detector_->FrameSent(33);
  EXPECT_EQ(kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
  EXPECT_EQ(0, overuse_detector_->FramesInQueue());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_TwoFrames) {
  const int kProcessingTimeMs1 = 100;
  const int kProcessingTimeMs2 = 50;
  const int kTimeBetweenFramesMs = 200;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs1);
  overuse_detector_->FrameSent(33);
  EXPECT_EQ(kProcessingTimeMs1, overuse_detector_->LastProcessingTimeMs());
  clock_->AdvanceTimeMilliseconds(kTimeBetweenFramesMs);
  overuse_detector_->FrameCaptured(kWidth, kHeight, 66);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs2);
  overuse_detector_->FrameSent(66);
  EXPECT_EQ(kProcessingTimeMs2, overuse_detector_->LastProcessingTimeMs());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_MaxQueueSize) {
  const int kMaxQueueSize = 91;
  for (int i = 0; i < kMaxQueueSize * 2; ++i) {
    overuse_detector_->FrameCaptured(kWidth, kHeight, i);
  }
  EXPECT_EQ(kMaxQueueSize, overuse_detector_->FramesInQueue());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_NonProcessedFramesRemoved) {
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameCaptured(kWidth, kHeight, 35);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameCaptured(kWidth, kHeight, 66);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameCaptured(kWidth, kHeight, 99);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  EXPECT_EQ(-1, overuse_detector_->LastProcessingTimeMs());
  EXPECT_EQ(4, overuse_detector_->FramesInQueue());
  overuse_detector_->FrameSent(66);
  // Frame 33, 35 removed, 66 processed, 99 not processed.
  EXPECT_EQ(2 * kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
  EXPECT_EQ(1, overuse_detector_->FramesInQueue());
  overuse_detector_->FrameSent(99);
  EXPECT_EQ(kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
  EXPECT_EQ(0, overuse_detector_->FramesInQueue());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_ResetClearsFrames) {
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  EXPECT_EQ(1, overuse_detector_->FramesInQueue());
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  // Verify reset (resolution changed).
  overuse_detector_->FrameCaptured(kWidth, kHeight + 1, 66);
  EXPECT_EQ(1, overuse_detector_->FramesInQueue());
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameSent(66);
  EXPECT_EQ(kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
  EXPECT_EQ(0, overuse_detector_->FramesInQueue());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_NonMatchingSendFrameIgnored) {
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameSent(34);
  EXPECT_EQ(-1, overuse_detector_->LastProcessingTimeMs());
  overuse_detector_->FrameSent(33);
  EXPECT_EQ(kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
}

}  // namespace webrtc
