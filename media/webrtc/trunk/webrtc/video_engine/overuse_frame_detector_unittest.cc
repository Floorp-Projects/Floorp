/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/overuse_frame_detector.h"

namespace webrtc {
namespace {
  const int kWidth = 640;
  const int kHeight = 480;
  const int kFrameInterval33ms = 33;
  const int kProcessIntervalMs = 5000;
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

class OveruseFrameDetectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockCpuOveruseObserver());
    overuse_detector_.reset(new OveruseFrameDetector(clock_.get()));

    options_.low_capture_jitter_threshold_ms = 10.0f;
    options_.high_capture_jitter_threshold_ms = 15.0f;
    options_.min_process_count = 0;
    overuse_detector_->SetOptions(options_);
    overuse_detector_->SetObserver(observer_.get());
  }

  int InitialJitter() {
    return ((options_.low_capture_jitter_threshold_ms +
             options_.high_capture_jitter_threshold_ms) / 2.0f) + 0.5;
  }

  int InitialUsage() {
    return ((options_.low_encode_usage_threshold_percent +
             options_.high_encode_usage_threshold_percent) / 2.0f) + 0.5;
  }

  void InsertFramesWithInterval(
      size_t num_frames, int interval_ms, int width, int height) {
    while (num_frames-- > 0) {
      clock_->AdvanceTimeMilliseconds(interval_ms);
      overuse_detector_->FrameCaptured(width, height,
                                       clock_->TimeInMilliseconds());
    }
  }

  void InsertAndSendFramesWithInterval(
      int num_frames, int interval_ms, int width, int height, int delay_ms) {
    while (num_frames-- > 0) {
      int64_t capture_time_ms = clock_->TimeInMilliseconds();
      overuse_detector_->FrameCaptured(width, height, capture_time_ms);
      clock_->AdvanceTimeMilliseconds(delay_ms);
      overuse_detector_->FrameEncoded(delay_ms);
      overuse_detector_->FrameSent(capture_time_ms);
      clock_->AdvanceTimeMilliseconds(interval_ms - delay_ms);
    }
  }

  void TriggerOveruse(int num_times) {
    for (int i = 0; i < num_times; ++i) {
      InsertFramesWithInterval(200, kFrameInterval33ms, kWidth, kHeight);
      InsertFramesWithInterval(50, 110, kWidth, kHeight);
      overuse_detector_->Process();
    }
  }

  void TriggerUnderuse() {
    InsertFramesWithInterval(900, kFrameInterval33ms, kWidth, kHeight);
    overuse_detector_->Process();
  }

  void TriggerOveruseWithProcessingUsage(int num_times) {
    const int kDelayMs = 32;
    for (int i = 0; i < num_times; ++i) {
      InsertAndSendFramesWithInterval(
          1000, kFrameInterval33ms, kWidth, kHeight, kDelayMs);
      overuse_detector_->Process();
    }
  }

  void TriggerUnderuseWithProcessingUsage() {
    const int kDelayMs1 = 5;
    const int kDelayMs2 = 6;
    InsertAndSendFramesWithInterval(
        1300, kFrameInterval33ms, kWidth, kHeight, kDelayMs1);
    InsertAndSendFramesWithInterval(
        1, kFrameInterval33ms, kWidth, kHeight, kDelayMs2);
    overuse_detector_->Process();
  }

  int CaptureJitterMs() {
    CpuOveruseMetrics metrics;
    overuse_detector_->GetCpuOveruseMetrics(&metrics);
    return metrics.capture_jitter_ms;
  }

  int AvgEncodeTimeMs() {
    CpuOveruseMetrics metrics;
    overuse_detector_->GetCpuOveruseMetrics(&metrics);
    return metrics.avg_encode_time_ms;
  }

  int UsagePercent() {
    CpuOveruseMetrics metrics;
    overuse_detector_->GetCpuOveruseMetrics(&metrics);
    return metrics.encode_usage_percent;
  }

  CpuOveruseOptions options_;
  scoped_ptr<SimulatedClock> clock_;
  scoped_ptr<MockCpuOveruseObserver> observer_;
  scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

// enable_capture_jitter_method = true;
// CaptureJitterMs() > high_capture_jitter_threshold_ms => overuse.
// CaptureJitterMs() < low_capture_jitter_threshold_ms => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  // capture_jitter > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  // capture_jitter > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  // capture_jitter < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithNoObserver) {
  overuse_detector_->SetObserver(NULL);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruse(options_.high_threshold_consecutive_count);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerUnderuse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithMethodDisabled) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = false;
  overuse_detector_->SetOptions(options_);
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
  CpuOveruseObserverImpl overuse_observer_;
  overuse_detector_->SetObserver(&overuse_observer_);
  options_.min_process_count = 1;
  overuse_detector_->SetOptions(options_);
  InsertFramesWithInterval(1200, kFrameInterval33ms, kWidth, kHeight);
  overuse_detector_->Process();
  EXPECT_EQ(0, overuse_observer_.normaluse_);
  clock_->AdvanceTimeMilliseconds(kProcessIntervalMs);
  overuse_detector_->Process();
  EXPECT_EQ(1, overuse_observer_.normaluse_);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(64);
  for(size_t i = 0; i < 64; ++i) {
    TriggerOveruse(options_.high_threshold_consecutive_count);
  }
}

TEST_F(OveruseFrameDetectorTest, ConsecutiveCountTriggersOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(2);
}

TEST_F(OveruseFrameDetectorTest, IncorrectConsecutiveCountTriggersNoOveruse) {
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  options_.high_threshold_consecutive_count = 2;
  overuse_detector_->SetOptions(options_);
  TriggerOveruse(1);
}

TEST_F(OveruseFrameDetectorTest, GetCpuOveruseMetrics) {
  CpuOveruseMetrics metrics;
  overuse_detector_->GetCpuOveruseMetrics(&metrics);
  EXPECT_GT(metrics.capture_jitter_ms, 0);
  EXPECT_GT(metrics.avg_encode_time_ms, 0);
  EXPECT_GT(metrics.encode_usage_percent, 0);
  EXPECT_GE(metrics.capture_queue_delay_ms_per_s, 0);
  EXPECT_GE(metrics.encode_rsd, 0);
}

TEST_F(OveruseFrameDetectorTest, CaptureJitter) {
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
  InsertFramesWithInterval(1000, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, CaptureJitterResetAfterResolutionChange) {
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
  InsertFramesWithInterval(1000, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), CaptureJitterMs());
  // Verify reset.
  InsertFramesWithInterval(1, kFrameInterval33ms, kWidth, kHeight + 1);
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, CaptureJitterResetAfterFrameTimeout) {
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
  InsertFramesWithInterval(1000, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), CaptureJitterMs());
  InsertFramesWithInterval(
      1, options_.frame_timeout_interval_ms, kWidth, kHeight);
  EXPECT_NE(InitialJitter(), CaptureJitterMs());
  // Verify reset.
  InsertFramesWithInterval(
      1, options_.frame_timeout_interval_ms + 1, kWidth, kHeight);
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, CaptureJitterResetAfterChangingThreshold) {
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
  options_.high_capture_jitter_threshold_ms = 90.0f;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
  options_.low_capture_jitter_threshold_ms = 30.0f;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, MinFrameSamplesBeforeUpdatingCaptureJitter) {
  options_.min_frame_samples = 40;
  overuse_detector_->SetOptions(options_);
  InsertFramesWithInterval(40, kFrameInterval33ms, kWidth, kHeight);
  EXPECT_EQ(InitialJitter(), CaptureJitterMs());
}

TEST_F(OveruseFrameDetectorTest, NoCaptureQueueDelay) {
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 0);
  overuse_detector_->FrameCaptured(
      kWidth, kHeight, clock_->TimeInMilliseconds());
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 0);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelay) {
  overuse_detector_->FrameCaptured(
      kWidth, kHeight, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(100);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayMultipleFrames) {
  overuse_detector_->FrameCaptured(
      kWidth, kHeight, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(10);
  overuse_detector_->FrameCaptured(
      kWidth, kHeight, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(20);

  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 30);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 20);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayResetAtResolutionSwitch) {
  overuse_detector_->FrameCaptured(
      kWidth, kHeight, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(10);
  overuse_detector_->FrameCaptured(
      kWidth, kHeight + 1, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(20);

  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 20);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayNoMatchingCapturedFrame) {
  overuse_detector_->FrameCaptured(
      kWidth, kHeight, clock_->TimeInMilliseconds());
  clock_->AdvanceTimeMilliseconds(100);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
  // No new captured frame. The last delay should be reported.
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_OneFrameDisabled) {
  options_.enable_extended_processing_usage = false;
  overuse_detector_->SetOptions(options_);
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameSent(33);
  EXPECT_EQ(-1, overuse_detector_->LastProcessingTimeMs());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_OneFrame) {
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  EXPECT_EQ(-1, overuse_detector_->LastProcessingTimeMs());
  overuse_detector_->FrameSent(33);
  EXPECT_EQ(kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
  EXPECT_EQ(0, overuse_detector_->FramesInQueue());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_TwoFrames) {
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
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
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
  const int kMaxQueueSize = 91;
  for (int i = 0; i < kMaxQueueSize * 2; ++i) {
    overuse_detector_->FrameCaptured(kWidth, kHeight, i);
  }
  EXPECT_EQ(kMaxQueueSize, overuse_detector_->FramesInQueue());
}

TEST_F(OveruseFrameDetectorTest, FrameDelay_NonProcessedFramesRemoved) {
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
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
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
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
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
  const int kProcessingTimeMs = 100;
  overuse_detector_->FrameCaptured(kWidth, kHeight, 33);
  clock_->AdvanceTimeMilliseconds(kProcessingTimeMs);
  overuse_detector_->FrameSent(34);
  EXPECT_EQ(-1, overuse_detector_->LastProcessingTimeMs());
  overuse_detector_->FrameSent(33);
  EXPECT_EQ(kProcessingTimeMs, overuse_detector_->LastProcessingTimeMs());
}

TEST_F(OveruseFrameDetectorTest, EncodedFrame) {
  const int kInitialAvgEncodeTimeInMs = 5;
  EXPECT_EQ(kInitialAvgEncodeTimeInMs, AvgEncodeTimeMs());
  for (int i = 0; i < 30; i++) {
    clock_->AdvanceTimeMilliseconds(33);
    overuse_detector_->FrameEncoded(2);
  }
  EXPECT_EQ(2, AvgEncodeTimeMs());
}

TEST_F(OveruseFrameDetectorTest, InitialProcessingUsage) {
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsage) {
  const int kProcessingTimeMs = 5;
  InsertAndSendFramesWithInterval(
      1000, kFrameInterval33ms, kWidth, kHeight, kProcessingTimeMs);
  EXPECT_EQ(kProcessingTimeMs * 100 / kFrameInterval33ms, UsagePercent());
}

TEST_F(OveruseFrameDetectorTest, ProcessingUsageResetAfterChangingThreshold) {
  EXPECT_EQ(InitialUsage(), UsagePercent());
  options_.high_encode_usage_threshold_percent = 100;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialUsage(), UsagePercent());
  options_.low_encode_usage_threshold_percent = 20;
  overuse_detector_->SetOptions(options_);
  EXPECT_EQ(InitialUsage(), UsagePercent());
}

// enable_encode_usage_method = true;
// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruseWithProcessingUsage) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = true;
  options_.enable_extended_processing_usage = false;
  overuse_detector_->SetOptions(options_);
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruseWithProcessingUsage(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithProcessingUsage) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = true;
  options_.enable_extended_processing_usage = false;
  overuse_detector_->SetOptions(options_);
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruseWithProcessingUsage(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuseWithProcessingUsage();
}

TEST_F(OveruseFrameDetectorTest,
       OveruseAndRecoverWithProcessingUsageMethodDisabled) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = false;
  options_.enable_extended_processing_usage = false;
  overuse_detector_->SetOptions(options_);
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruseWithProcessingUsage(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerUnderuseWithProcessingUsage();
}

// enable_extended_processing_usage = true;
// enable_encode_usage_method = true;
// UsagePercent() > high_encode_usage_threshold_percent => overuse.
// UsagePercent() < low_encode_usage_threshold_percent => underuse.
TEST_F(OveruseFrameDetectorTest, TriggerOveruseWithExtendedProcessingUsage) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = true;
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruseWithProcessingUsage(options_.high_threshold_consecutive_count);
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecoverWithExtendedProcessingUsage) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = true;
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);
  TriggerOveruseWithProcessingUsage(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));
  TriggerUnderuseWithProcessingUsage();
}

TEST_F(OveruseFrameDetectorTest,
       OveruseAndRecoverWithExtendedProcessingUsageMethodDisabled) {
  options_.enable_capture_jitter_method = false;
  options_.enable_encode_usage_method = false;
  options_.enable_extended_processing_usage = true;
  overuse_detector_->SetOptions(options_);
  // usage > high => overuse
  EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(0);
  TriggerOveruseWithProcessingUsage(options_.high_threshold_consecutive_count);
  // usage < low => underuse
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);
  TriggerUnderuseWithProcessingUsage();
}

}  // namespace webrtc
