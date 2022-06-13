/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/audio_rtp_receiver.h"

#include "media/base/media_channel.h"
#include "pc/test/mock_voice_media_channel.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

static const int kTimeOut = 100;
static const double kDefaultVolume = 1;
static const double kVolume = 3.7;
static const uint32_t kSsrc = 3;

namespace webrtc {
class AudioRtpReceiverTest : public ::testing::Test {
 protected:
  AudioRtpReceiverTest()
      : worker_(rtc::Thread::Current()),
        receiver_(
            rtc::make_ref_counted<AudioRtpReceiver>(worker_,
                                                    std::string(),
                                                    std::vector<std::string>(),
                                                    false)),
        media_channel_(rtc::Thread::Current()) {
    EXPECT_CALL(media_channel_, SetRawAudioSink(kSsrc, _));
    EXPECT_CALL(media_channel_, SetBaseMinimumPlayoutDelayMs(kSsrc, _));
  }

  ~AudioRtpReceiverTest() {
    receiver_->SetMediaChannel(nullptr);
    receiver_->Stop();
  }

  rtc::Thread* worker_;
  rtc::scoped_refptr<AudioRtpReceiver> receiver_;
  cricket::MockVoiceMediaChannel media_channel_;
};

TEST_F(AudioRtpReceiverTest, SetOutputVolumeIsCalled) {
  std::atomic_int set_volume_calls(0);

  EXPECT_CALL(media_channel_, SetOutputVolume(kSsrc, kDefaultVolume))
      .WillOnce(InvokeWithoutArgs([&] {
        set_volume_calls++;
        return true;
      }));

  receiver_->track();
  receiver_->track()->set_enabled(true);
  receiver_->SetMediaChannel(&media_channel_);
  receiver_->SetupMediaChannel(kSsrc);

  EXPECT_CALL(media_channel_, SetOutputVolume(kSsrc, kVolume))
      .WillOnce(InvokeWithoutArgs([&] {
        set_volume_calls++;
        return true;
      }));

  receiver_->OnSetVolume(kVolume);
  EXPECT_TRUE_WAIT(set_volume_calls == 2, kTimeOut);
}

TEST_F(AudioRtpReceiverTest, VolumesSetBeforeStartingAreRespected) {
  // Set the volume before setting the media channel. It should still be used
  // as the initial volume.
  receiver_->OnSetVolume(kVolume);

  receiver_->track()->set_enabled(true);
  receiver_->SetMediaChannel(&media_channel_);

  // The previosly set initial volume should be propagated to the provided
  // media_channel_ as soon as SetupMediaChannel is called.
  EXPECT_CALL(media_channel_, SetOutputVolume(kSsrc, kVolume));

  receiver_->SetupMediaChannel(kSsrc);
}
}  // namespace webrtc
