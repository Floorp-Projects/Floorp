/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <algorithm>

#include "gtest/gtest.h"
#include "video_engine/stream_synchronization.h"

namespace webrtc {

// These correspond to the same constants defined in vie_sync_module.cc.
enum { kMaxVideoDiffMs = 80 };
enum { kMaxAudioDiffMs = 80 };
enum { kMaxDelay = 1500 };

class Time {
 public:
  explicit Time(int64_t offset)
      : kNtpJan1970(2208988800UL),
        time_now_ms_(offset) {}

  void NowNtp(uint32_t* ntp_secs, uint32_t* ntp_frac) const {
    *ntp_secs = time_now_ms_ / 1000 + kNtpJan1970;
    int64_t remainder = time_now_ms_ % 1000;
    *ntp_frac = static_cast<uint32_t>(
        static_cast<double>(remainder) / 1000.0 * pow(2.0, 32.0) + 0.5);
  }

  void IncreaseTimeMs(int64_t inc) {
    time_now_ms_ += inc;
  }

  int64_t time_now_ms() const {
    return time_now_ms_;
  }
 private:
  // January 1970, in NTP seconds.
  const uint32_t kNtpJan1970;
  int64_t time_now_ms_;
};

class StreamSynchronizationTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    sync_ = new StreamSynchronization(0, 0);
    send_time_ = new Time(kSendTimeOffsetMs);
    receive_time_ = new Time(kReceiveTimeOffsetMs);
  }

  virtual void TearDown() {
    delete sync_;
    delete send_time_;
    delete receive_time_;
  }

  int DelayedAudio(int delay_ms,
                   int current_audio_delay_ms,
                   int* extra_audio_delay_ms,
                   int* total_video_delay_ms) {
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    send_time_->NowNtp(&audio.received_ntp_secs, &audio.received_ntp_frac);
    send_time_->NowNtp(&video.received_ntp_secs, &video.received_ntp_frac);
    receive_time_->NowNtp(&video.rtcp_arrivaltime_secs,
                          &video.rtcp_arrivaltime_frac);
    // Audio later than video.
    receive_time_->IncreaseTimeMs(delay_ms);
    receive_time_->NowNtp(&audio.rtcp_arrivaltime_secs,
                          &audio.rtcp_arrivaltime_frac);
    return sync_->ComputeDelays(audio,
                                current_audio_delay_ms,
                                extra_audio_delay_ms,
                                video,
                                total_video_delay_ms);
  }

  int DelayedVideo(int delay_ms,
                   int current_audio_delay_ms,
                   int* extra_audio_delay_ms,
                   int* total_video_delay_ms) {
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    send_time_->NowNtp(&audio.received_ntp_secs, &audio.received_ntp_frac);
    send_time_->NowNtp(&video.received_ntp_secs, &video.received_ntp_frac);
    receive_time_->NowNtp(&audio.rtcp_arrivaltime_secs,
                          &audio.rtcp_arrivaltime_frac);
    // Video later than audio.
    receive_time_->IncreaseTimeMs(delay_ms);
    receive_time_->NowNtp(&video.rtcp_arrivaltime_secs,
                          &video.rtcp_arrivaltime_frac);
    return sync_->ComputeDelays(audio,
                                current_audio_delay_ms,
                                extra_audio_delay_ms,
                                video,
                                total_video_delay_ms);
  }

  int DelayedAudioAndVideo(int audio_delay_ms,
                           int video_delay_ms,
                           int current_audio_delay_ms,
                           int* extra_audio_delay_ms,
                           int* total_video_delay_ms) {
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    send_time_->NowNtp(&audio.received_ntp_secs, &audio.received_ntp_frac);
    send_time_->NowNtp(&video.received_ntp_secs, &video.received_ntp_frac);

    if (audio_delay_ms > video_delay_ms) {
      // Audio later than video.
      receive_time_->IncreaseTimeMs(video_delay_ms);
      receive_time_->NowNtp(&video.rtcp_arrivaltime_secs,
                            &video.rtcp_arrivaltime_frac);
      receive_time_->IncreaseTimeMs(audio_delay_ms - video_delay_ms);
      receive_time_->NowNtp(&audio.rtcp_arrivaltime_secs,
                            &audio.rtcp_arrivaltime_frac);
    } else {
      // Video later than audio.
      receive_time_->IncreaseTimeMs(audio_delay_ms);
      receive_time_->NowNtp(&audio.rtcp_arrivaltime_secs,
                            &audio.rtcp_arrivaltime_frac);
      receive_time_->IncreaseTimeMs(video_delay_ms - audio_delay_ms);
      receive_time_->NowNtp(&video.rtcp_arrivaltime_secs,
                            &video.rtcp_arrivaltime_frac);
    }
    return sync_->ComputeDelays(audio,
                                current_audio_delay_ms,
                                extra_audio_delay_ms,
                                video,
                                total_video_delay_ms);
  }

  int MaxAudioDelayIncrease(int current_audio_delay_ms, int delay_ms) {
    return std::min((delay_ms - current_audio_delay_ms) / 2,
                    static_cast<int>(kMaxAudioDiffMs));
  }

  int MaxAudioDelayDecrease(int current_audio_delay_ms, int delay_ms) {
    return std::max((delay_ms - current_audio_delay_ms) / 2, -kMaxAudioDiffMs);
  }

  enum { kSendTimeOffsetMs = 0 };
  enum { kReceiveTimeOffsetMs = 123456 };

  StreamSynchronization* sync_;
  Time* send_time_;
  Time* receive_time_;
};

TEST_F(StreamSynchronizationTest, NoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int delay_ms = 0;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_EQ(0, DelayedAudio(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  EXPECT_EQ(0, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, VideoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_EQ(0, DelayedAudio(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(kMaxVideoDiffMs, total_video_delay_ms);

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_EQ(0, DelayedAudio(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(2*kMaxVideoDiffMs, total_video_delay_ms);

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_EQ(0, DelayedAudio(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(delay_ms, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, AudioDelay) {
  int current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_EQ(0, DelayedVideo(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(kMaxAudioDiffMs, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  int current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedVideo(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
            MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedVideo(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
            MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason reduced the delay.
  current_audio_delay_ms = 170;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedVideo(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // Since we only can ask NetEQ for a certain amount of extra delay, and
  // we only measure the total NetEQ delay, we will ask for additional delay
  // here to try to
  EXPECT_EQ(current_extra_delay_ms +
            MaxAudioDelayIncrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason significantly increased the delay.
  current_audio_delay_ms = 250;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedVideo(delay_ms, current_audio_delay_ms,
                            &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms +
            MaxAudioDelayDecrease(current_audio_delay_ms, delay_ms),
            extra_audio_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoLater) {
  int current_audio_delay_ms = 0;
  int audio_delay_ms = 100;
  int video_delay_ms = 300;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(kMaxAudioDiffMs, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  int current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms + MaxAudioDelayIncrease(
      current_audio_delay_ms, video_delay_ms - audio_delay_ms),
      extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms + MaxAudioDelayIncrease(
      current_audio_delay_ms, video_delay_ms - audio_delay_ms),
      extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason reduced the delay.
  current_audio_delay_ms = 170;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // Since we only can ask NetEQ for a certain amount of extra delay, and
  // we only measure the total NetEQ delay, we will ask for additional delay
  // here to try to stay in sync.
  EXPECT_EQ(current_extra_delay_ms + MaxAudioDelayIncrease(
      current_audio_delay_ms, video_delay_ms - audio_delay_ms),
      extra_audio_delay_ms);
  current_extra_delay_ms = extra_audio_delay_ms;

  // Simulate that NetEQ for some reason significantly increased the delay.
  current_audio_delay_ms = 250;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than the half of the required
  // change in delay.
  EXPECT_EQ(current_extra_delay_ms + MaxAudioDelayIncrease(
      current_audio_delay_ms, video_delay_ms - audio_delay_ms),
      extra_audio_delay_ms);
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioLater) {
  int current_audio_delay_ms = 0;
  int audio_delay_ms = 300;
  int video_delay_ms = 100;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(kMaxVideoDiffMs, total_video_delay_ms);
  EXPECT_EQ(0, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(1000 - std::max(audio_delay_ms,
                                                video_delay_ms));
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(2 * kMaxVideoDiffMs, total_video_delay_ms);
  EXPECT_EQ(0, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(1000 - std::max(audio_delay_ms,
                                                video_delay_ms));
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(audio_delay_ms - video_delay_ms, total_video_delay_ms);
  EXPECT_EQ(0, extra_audio_delay_ms);

  // Simulate that NetEQ introduces some audio delay.
  current_audio_delay_ms = 50;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(1000 - std::max(audio_delay_ms,
                                                video_delay_ms));
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(audio_delay_ms - video_delay_ms + current_audio_delay_ms,
            total_video_delay_ms);
  EXPECT_EQ(0, extra_audio_delay_ms);

  // Simulate that NetEQ reduces its delay.
  current_audio_delay_ms = 10;
  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(1000 - std::max(audio_delay_ms,
                                                video_delay_ms));
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_EQ(0, DelayedAudioAndVideo(audio_delay_ms,
                                    video_delay_ms,
                                    current_audio_delay_ms,
                                    &extra_audio_delay_ms,
                                    &total_video_delay_ms));
  EXPECT_EQ(audio_delay_ms - video_delay_ms + current_audio_delay_ms,
            total_video_delay_ms);
  EXPECT_EQ(0, extra_audio_delay_ms);
}
}  // namespace webrtc
