/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/video_coding/main/source/media_optimization.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
namespace media_optimization {

class TestMediaOptimization : public ::testing::Test {
 protected:
  enum {
    kSampleRate = 90000  // RTP timestamps per second.
  };

  // Note: simulated clock starts at 1 seconds, since parts of webrtc use 0 as
  // a special case (e.g. frame rate in media optimization).
  TestMediaOptimization()
      : clock_(1000),
        media_opt_(&clock_),
        frame_time_ms_(33),
        next_timestamp_(0) {}

  // This method mimics what happens in VideoSender::AddVideoFrame.
  void AddFrameAndAdvanceTime(int bitrate_bps, bool expect_frame_drop) {
    ASSERT_GE(bitrate_bps, 0);
    bool frame_dropped = media_opt_.DropFrame();
    EXPECT_EQ(expect_frame_drop, frame_dropped);
    if (!frame_dropped) {
      int bytes_per_frame = bitrate_bps * frame_time_ms_ / (8 * 1000);
      ASSERT_EQ(VCM_OK, media_opt_.UpdateWithEncodedData(
          bytes_per_frame, next_timestamp_, kVideoFrameDelta));
    }
    next_timestamp_ += frame_time_ms_ * kSampleRate / 1000;
    clock_.AdvanceTimeMilliseconds(frame_time_ms_);
  }

  SimulatedClock clock_;
  MediaOptimization media_opt_;
  int frame_time_ms_;
  uint32_t next_timestamp_;
};


TEST_F(TestMediaOptimization, VerifyMuting) {
  // Enable video suspension with these limits.
  // Suspend the video when the rate is below 50 kbps and resume when it gets
  // above 50 + 10 kbps again.
  const int kThresholdBps = 50000;
  const int kWindowBps = 10000;
  media_opt_.SuspendBelowMinBitrate(kThresholdBps, kWindowBps);

  // The video should not be suspended from the start.
  EXPECT_FALSE(media_opt_.IsVideoSuspended());

  int target_bitrate_kbps = 100;
  media_opt_.SetTargetRates(target_bitrate_kbps * 1000,
                            0,  // Lossrate.
                            100,
                            NULL,
                            NULL);  // RTT in ms.
  media_opt_.EnableFrameDropper(true);
  for (int time = 0; time < 2000; time += frame_time_ms_) {
    ASSERT_NO_FATAL_FAILURE(AddFrameAndAdvanceTime(target_bitrate_kbps, false));
  }

  // Set the target rate below the limit for muting.
  media_opt_.SetTargetRates(kThresholdBps - 1000,
                            0,  // Lossrate.
                            100,
                            NULL,
                            NULL);  // RTT in ms.
  // Expect the muter to engage immediately and stay muted.
  // Test during 2 seconds.
  for (int time = 0; time < 2000; time += frame_time_ms_) {
    EXPECT_TRUE(media_opt_.IsVideoSuspended());
    ASSERT_NO_FATAL_FAILURE(AddFrameAndAdvanceTime(target_bitrate_kbps, true));
  }

  // Set the target above the limit for muting, but not above the
  // limit + window.
  media_opt_.SetTargetRates(kThresholdBps + 1000,
                            0,  // Lossrate.
                            100,
                            NULL,
                            NULL);  // RTT in ms.
                                    // Expect the muter to stay muted.
  // Test during 2 seconds.
  for (int time = 0; time < 2000; time += frame_time_ms_) {
    EXPECT_TRUE(media_opt_.IsVideoSuspended());
    ASSERT_NO_FATAL_FAILURE(AddFrameAndAdvanceTime(target_bitrate_kbps, true));
  }

  // Set the target above limit + window.
  media_opt_.SetTargetRates(kThresholdBps + kWindowBps + 1000,
                            0,  // Lossrate.
                            100,
                            NULL,
                            NULL);  // RTT in ms.
  // Expect the muter to disengage immediately.
  // Test during 2 seconds.
  for (int time = 0; time < 2000; time += frame_time_ms_) {
    EXPECT_FALSE(media_opt_.IsVideoSuspended());
    ASSERT_NO_FATAL_FAILURE(
        AddFrameAndAdvanceTime((kThresholdBps + kWindowBps) / 1000, false));
  }
}

}  // namespace media_optimization
}  // namespace webrtc
