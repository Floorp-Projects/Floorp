/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive.h"

#include "api/crypto/frame_decryptor_interface.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_transport.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace voe {

TEST(ChannelReceiveTest, CreateAndDestroy) {
  GlobalSimulatedTimeController time_controller(Timestamp::Seconds(5555));
  uint32_t local_ssrc = 1111;
  uint32_t remote_ssrc = 2222;
  webrtc::CryptoOptions crypto_options;
  rtc::scoped_refptr<test::MockAudioDeviceModule> audio_device_module =
      test::MockAudioDeviceModule::CreateNice();
  MockTransport transport;
  auto channel = CreateChannelReceive(
      time_controller.GetClock(),
      /* neteq_factory= */ nullptr, audio_device_module.get(), &transport,
      /* rtc_event_log= */ nullptr, local_ssrc, remote_ssrc,
      /* jitter_buffer_max_packets= */ 0,
      /* jitter_buffer_fast_playout= */ false,
      /* jitter_buffer_min_delay_ms= */ 0,
      /* enable_non_sender_rtt= */ false,
      /* decoder_factory= */ nullptr,
      /* codec_pair_id= */ absl::nullopt,
      /* frame_decryptor_interface= */ nullptr, crypto_options,
      /* frame_transformer= */ nullptr);
  EXPECT_TRUE(!!channel);
}

}  // namespace voe
}  // namespace webrtc
