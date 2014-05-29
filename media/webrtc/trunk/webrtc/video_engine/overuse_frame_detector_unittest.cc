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

class MockCpuOveruseObserver : public CpuOveruseObserver {
 public:
  MockCpuOveruseObserver() {}
  virtual ~MockCpuOveruseObserver() {}

  MOCK_METHOD0(OveruseDetected, void());
  MOCK_METHOD0(NormalUsage, void());
};

class OveruseFrameDetectorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clock_.reset(new SimulatedClock(1234));
    observer_.reset(new MockCpuOveruseObserver());
    overuse_detector_.reset(new OveruseFrameDetector(clock_.get(),
                                                     10.0f,
                                                     15.0f));
    overuse_detector_->SetObserver(observer_.get());
  }

  void InsertFramesWithInterval(size_t num_frames, int interval_ms) {
    while (num_frames-- > 0) {
      clock_->AdvanceTimeMilliseconds(interval_ms);
      overuse_detector_->FrameCaptured(640, 480);
    }
  }

  void TriggerOveruse() {
    int regular_frame_interval_ms = 33;

    EXPECT_CALL(*(observer_.get()), OveruseDetected()).Times(1);

    InsertFramesWithInterval(200, regular_frame_interval_ms);
    InsertFramesWithInterval(50, 110);
    overuse_detector_->Process();

    InsertFramesWithInterval(200, regular_frame_interval_ms);
    InsertFramesWithInterval(50, 110);
    overuse_detector_->Process();
  }

  void TriggerNormalUsage() {
    int regular_frame_interval_ms = 33;

    EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(testing::AtLeast(1));

    InsertFramesWithInterval(900, regular_frame_interval_ms);
    overuse_detector_->Process();
  }

  scoped_ptr<SimulatedClock> clock_;
  scoped_ptr<MockCpuOveruseObserver> observer_;
  scoped_ptr<OveruseFrameDetector> overuse_detector_;
};

TEST_F(OveruseFrameDetectorTest, TriggerOveruse) {
  overuse_detector_->set_min_process_count_before_reporting(0);
  TriggerOveruse();
}

TEST_F(OveruseFrameDetectorTest, OveruseAndRecover) {
  overuse_detector_->set_min_process_count_before_reporting(0);
  TriggerOveruse();
  TriggerNormalUsage();
}

TEST_F(OveruseFrameDetectorTest, DoubleOveruseAndRecover) {
  overuse_detector_->set_min_process_count_before_reporting(0);
  TriggerOveruse();
  TriggerOveruse();
  TriggerNormalUsage();
}

TEST_F(OveruseFrameDetectorTest, TriggerNormalUsageWithMinProcessCount) {
  overuse_detector_->set_min_process_count_before_reporting(1);
  InsertFramesWithInterval(900, 33);
  overuse_detector_->Process();
  EXPECT_EQ(-1, overuse_detector_->last_capture_jitter_ms());
  clock_->AdvanceTimeMilliseconds(5000);
  overuse_detector_->Process();
  EXPECT_GT(overuse_detector_->last_capture_jitter_ms(), 0);
}

TEST_F(OveruseFrameDetectorTest, ConstantOveruseGivesNoNormalUsage) {
  overuse_detector_->set_min_process_count_before_reporting(0);
  EXPECT_CALL(*(observer_.get()), NormalUsage()).Times(0);

  for(size_t i = 0; i < 64; ++i)
    TriggerOveruse();
}

TEST_F(OveruseFrameDetectorTest, LastCaptureJitter) {
  overuse_detector_->set_min_process_count_before_reporting(0);
  EXPECT_EQ(-1, overuse_detector_->last_capture_jitter_ms());
  TriggerOveruse();
  EXPECT_GT(overuse_detector_->last_capture_jitter_ms(), 0);
}

TEST_F(OveruseFrameDetectorTest, NoCaptureQueueDelay) {
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 0);
  overuse_detector_->FrameCaptured(320, 180);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 0);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelay) {
  overuse_detector_->FrameCaptured(320, 180);
  clock_->AdvanceTimeMilliseconds(100);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayMultipleFrames) {
  overuse_detector_->FrameCaptured(320, 180);
  clock_->AdvanceTimeMilliseconds(10);
  overuse_detector_->FrameCaptured(320, 180);
  clock_->AdvanceTimeMilliseconds(20);

  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 30);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 20);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayResetAtResolutionSwitch) {
  overuse_detector_->FrameCaptured(320, 180);
  clock_->AdvanceTimeMilliseconds(10);
  overuse_detector_->FrameCaptured(321, 180);
  clock_->AdvanceTimeMilliseconds(20);

  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 20);
}

TEST_F(OveruseFrameDetectorTest, CaptureQueueDelayNoMatchingCapturedFrame) {
  overuse_detector_->FrameCaptured(320, 180);
  clock_->AdvanceTimeMilliseconds(100);
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
  // No new captured frame. The last delay should be reported.
  overuse_detector_->FrameProcessingStarted();
  EXPECT_EQ(overuse_detector_->CaptureQueueDelayMsPerS(), 100);
}

TEST_F(OveruseFrameDetectorTest, EncodedFrame) {
  const int kInitialAvgEncodeTimeInMs = 5;
  EXPECT_EQ(kInitialAvgEncodeTimeInMs, overuse_detector_->AvgEncodeTimeMs());
  for (int i = 0; i < 30; i++) {
    clock_->AdvanceTimeMilliseconds(33);
    overuse_detector_->FrameEncoded(2);
  }
  EXPECT_EQ(2, overuse_detector_->AvgEncodeTimeMs());
}

TEST_F(OveruseFrameDetectorTest, EncodedUsage) {
  for (int i = 0; i < 30; i++) {
    overuse_detector_->FrameCaptured(320, 180);
    clock_->AdvanceTimeMilliseconds(5);
    overuse_detector_->FrameEncoded(5);
    clock_->AdvanceTimeMilliseconds(33-5);
  }
  EXPECT_EQ(15, overuse_detector_->EncodeUsagePercent());
}

}  // namespace webrtc
