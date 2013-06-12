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

// Test constants.
enum { kDefaultAudioFrequency = 8000 };
enum { kDefaultVideoFrequency = 90000 };
const double kNtpFracPerMs = 4.294967296E6;

class Time {
 public:
  explicit Time(int64_t offset)
      : kNtpJan1970(2208988800UL),
        time_now_ms_(offset) {}

    synchronization::RtcpMeasurement GenerateRtcp(int frequency,
                                                  uint32_t offset) const {
    synchronization::RtcpMeasurement rtcp;
    NowNtp(&rtcp.ntp_secs, &rtcp.ntp_frac);
    rtcp.rtp_timestamp = NowRtp(frequency, offset);
    return rtcp;
  }

  void NowNtp(uint32_t* ntp_secs, uint32_t* ntp_frac) const {
    *ntp_secs = time_now_ms_ / 1000 + kNtpJan1970;
    int64_t remainder_ms = time_now_ms_ % 1000;
    *ntp_frac = static_cast<uint32_t>(
        static_cast<double>(remainder_ms) * kNtpFracPerMs + 0.5);
  }

  uint32_t NowRtp(int frequency, uint32_t offset) const {
    return frequency * time_now_ms_ / 1000 + offset;
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
    audio_clock_drift_ = 1.0;
    video_clock_drift_ = 1.0;
  }

  virtual void TearDown() {
    delete sync_;
    delete send_time_;
    delete receive_time_;
  }

  // Generates the necessary RTCP measurements and RTP timestamps and computes
  // the audio and video delays needed to get the two streams in sync.
  // |audio_delay_ms| and |video_delay_ms| are the number of milliseconds after
  // capture which the frames are rendered.
  // |current_audio_delay_ms| is the number of milliseconds which audio is
  // currently being delayed by the receiver.
  bool DelayedStreams(int audio_delay_ms,
                      int video_delay_ms,
                      int current_audio_delay_ms,
                      int* extra_audio_delay_ms,
                      int* total_video_delay_ms) {
    int audio_frequency = static_cast<int>(kDefaultAudioFrequency *
                                           audio_clock_drift_ + 0.5);
    int audio_offset = 0;
    int video_frequency = static_cast<int>(kDefaultVideoFrequency *
                                           video_clock_drift_ + 0.5);
    int video_offset = 0;
    StreamSynchronization::Measurements audio;
    StreamSynchronization::Measurements video;
    // Generate NTP/RTP timestamp pair for both streams corresponding to RTCP.
    audio.rtcp.push_front(send_time_->GenerateRtcp(audio_frequency,
                                                   audio_offset));
    send_time_->IncreaseTimeMs(100);
    receive_time_->IncreaseTimeMs(100);
    video.rtcp.push_front(send_time_->GenerateRtcp(video_frequency,
                                                   video_offset));
    send_time_->IncreaseTimeMs(900);
    receive_time_->IncreaseTimeMs(900);
    audio.rtcp.push_front(send_time_->GenerateRtcp(audio_frequency,
                                                   audio_offset));
    send_time_->IncreaseTimeMs(100);
    receive_time_->IncreaseTimeMs(100);
    video.rtcp.push_front(send_time_->GenerateRtcp(video_frequency,
                                                   video_offset));
    send_time_->IncreaseTimeMs(900);
    receive_time_->IncreaseTimeMs(900);

    // Capture an audio and a video frame at the same time.
    audio.latest_timestamp = send_time_->NowRtp(audio_frequency,
                                                      audio_offset);
    video.latest_timestamp = send_time_->NowRtp(video_frequency,
                                                      video_offset);

    if (audio_delay_ms > video_delay_ms) {
      // Audio later than video.
      receive_time_->IncreaseTimeMs(video_delay_ms);
      video.latest_receive_time_ms = receive_time_->time_now_ms();
      receive_time_->IncreaseTimeMs(audio_delay_ms - video_delay_ms);
      audio.latest_receive_time_ms = receive_time_->time_now_ms();
    } else {
      // Video later than audio.
      receive_time_->IncreaseTimeMs(audio_delay_ms);
      audio.latest_receive_time_ms = receive_time_->time_now_ms();
      receive_time_->IncreaseTimeMs(video_delay_ms - audio_delay_ms);
      video.latest_receive_time_ms = receive_time_->time_now_ms();
    }
    int relative_delay_ms;
    StreamSynchronization::ComputeRelativeDelay(audio, video,
                                                &relative_delay_ms);
    EXPECT_EQ(video_delay_ms - audio_delay_ms, relative_delay_ms);
    return sync_->ComputeDelays(relative_delay_ms,
                                current_audio_delay_ms,
                                extra_audio_delay_ms,
                                total_video_delay_ms);
  }

  // Simulate audio playback 300 ms after capture and video rendering 100 ms
  // after capture. Verify that the correct extra delays are calculated for
  // audio and video, and that they change correctly when we simulate that
  // NetEQ or the VCM adds more delay to the streams.
  // TODO(holmer): This is currently wrong! We should simply change
  // audio_delay_ms or video_delay_ms since those now include VCM and NetEQ
  // delays.
  void BothDelayedAudioLaterTest() {
    int current_audio_delay_ms = 0;
    int audio_delay_ms = 300;
    int video_delay_ms = 100;
    int extra_audio_delay_ms = 0;
    int total_video_delay_ms = 0;

    EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
    EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
    EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
    EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
    EXPECT_TRUE(DelayedStreams(audio_delay_ms,
                               video_delay_ms,
                               current_audio_delay_ms,
                               &extra_audio_delay_ms,
                               &total_video_delay_ms));
    EXPECT_EQ(audio_delay_ms - video_delay_ms + current_audio_delay_ms,
              total_video_delay_ms);
    EXPECT_EQ(0, extra_audio_delay_ms);
  }

  int MaxAudioDelayIncrease(int current_audio_delay_ms, int delay_ms) {
    return std::min((delay_ms - current_audio_delay_ms) / 2,
                    static_cast<int>(kMaxAudioDiffMs));
  }

  int MaxAudioDelayDecrease(int current_audio_delay_ms, int delay_ms) {
    return std::max((delay_ms - current_audio_delay_ms) / 2, -kMaxAudioDiffMs);
  }

  enum { kSendTimeOffsetMs = 98765 };
  enum { kReceiveTimeOffsetMs = 43210 };

  StreamSynchronization* sync_;
  Time* send_time_;  // The simulated clock at the sender.
  Time* receive_time_;  // The simulated clock at the receiver.
  double audio_clock_drift_;
  double video_clock_drift_;
};

TEST_F(StreamSynchronizationTest, NoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(0, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  EXPECT_EQ(0, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, VideoDelay) {
  uint32_t current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(kMaxVideoDiffMs, total_video_delay_ms);

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // The video delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(2*kMaxVideoDiffMs, total_video_delay_ms);

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  // Simulate 0 minimum delay in the VCM.
  total_video_delay_ms = 0;
  EXPECT_TRUE(DelayedStreams(delay_ms, 0, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, extra_audio_delay_ms);
  // Enough time should have elapsed for the requested total video delay to be
  // equal to the relative delay between audio and video, i.e., we are in sync.
  EXPECT_EQ(delay_ms, total_video_delay_ms);
}

TEST_F(StreamSynchronizationTest, AudioDelay) {
  int current_audio_delay_ms = 0;
  int delay_ms = 200;
  int extra_audio_delay_ms = 0;
  int total_video_delay_ms = 0;

  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
                             &extra_audio_delay_ms, &total_video_delay_ms));
  EXPECT_EQ(0, total_video_delay_ms);
  // The audio delay is not allowed to change more than this in 1 second.
  EXPECT_EQ(kMaxAudioDiffMs, extra_audio_delay_ms);
  current_audio_delay_ms = extra_audio_delay_ms;
  int current_extra_delay_ms = extra_audio_delay_ms;

  send_time_->IncreaseTimeMs(1000);
  receive_time_->IncreaseTimeMs(800);
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(0, delay_ms, current_audio_delay_ms,
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

  EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
  EXPECT_TRUE(DelayedStreams(audio_delay_ms,
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
  BothDelayedAudioLaterTest();
}

TEST_F(StreamSynchronizationTest, BothDelayedAudioClockDrift) {
  audio_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest();
}

TEST_F(StreamSynchronizationTest, BothDelayedVideoClockDrift) {
  video_clock_drift_ = 1.05;
  BothDelayedAudioLaterTest();
}
}  // namespace webrtc
