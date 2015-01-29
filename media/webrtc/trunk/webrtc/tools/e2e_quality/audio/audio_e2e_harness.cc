/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Sets up a simple VoiceEngine loopback call with the default audio devices
// and runs forever. Some parameters can be configured through command-line
// flags.

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_network.h"

DEFINE_string(render, "render", "render device name");
DEFINE_string(codec, "ISAC", "codec name");
DEFINE_int32(rate, 16000, "codec sample rate in Hz");

namespace webrtc {
namespace test {

void RunHarness() {
  VoiceEngine* voe = VoiceEngine::Create();
  ASSERT_TRUE(voe != NULL);
  VoEAudioProcessing* audio = VoEAudioProcessing::GetInterface(voe);
  ASSERT_TRUE(audio != NULL);
  VoEBase* base = VoEBase::GetInterface(voe);
  ASSERT_TRUE(base != NULL);
  VoECodec* codec = VoECodec::GetInterface(voe);
  ASSERT_TRUE(codec != NULL);
  VoEHardware* hardware = VoEHardware::GetInterface(voe);
  ASSERT_TRUE(hardware != NULL);
  VoENetwork* network = VoENetwork::GetInterface(voe);
  ASSERT_TRUE(network != NULL);

  ASSERT_EQ(0, base->Init());
  int channel = base->CreateChannel();
  ASSERT_NE(-1, channel);

  scoped_ptr<VoiceChannelTransport> voice_channel_transport(
      new VoiceChannelTransport(network, channel));

  ASSERT_EQ(0, voice_channel_transport->SetSendDestination("127.0.0.1", 1234));
  ASSERT_EQ(0, voice_channel_transport->SetLocalReceiver(1234));

  CodecInst codec_params = {0};
  bool codec_found = false;
  for (int i = 0; i < codec->NumOfCodecs(); i++) {
    ASSERT_EQ(0, codec->GetCodec(i, codec_params));
    if (FLAGS_codec.compare(codec_params.plname) == 0 &&
        FLAGS_rate == codec_params.plfreq) {
      codec_found = true;
      break;
    }
  }
  ASSERT_TRUE(codec_found);
  ASSERT_EQ(0, codec->SetSendCodec(channel, codec_params));

  int num_devices = 0;
  ASSERT_EQ(0, hardware->GetNumOfPlayoutDevices(num_devices));
  char device_name[128] = {0};
  char guid[128] = {0};
  bool device_found = false;
  int device_index;
  for (device_index = 0; device_index < num_devices; device_index++) {
    ASSERT_EQ(0, hardware->GetPlayoutDeviceName(device_index, device_name,
                                                guid));
    if (FLAGS_render.compare(device_name) == 0) {
      device_found = true;
      break;
    }
  }
  ASSERT_TRUE(device_found);
  ASSERT_EQ(0, hardware->SetPlayoutDevice(device_index));

  // Disable all audio processing.
  ASSERT_EQ(0, audio->SetAgcStatus(false));
  ASSERT_EQ(0, audio->SetEcStatus(false));
  ASSERT_EQ(0, audio->EnableHighPassFilter(false));
  ASSERT_EQ(0, audio->SetNsStatus(false));

  ASSERT_EQ(0, base->StartReceive(channel));
  ASSERT_EQ(0, base->StartPlayout(channel));
  ASSERT_EQ(0, base->StartSend(channel));

  // Run forever...
  while (1) {
  }
}

}  // namespace test
}  // namespace webrtc

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::RunHarness();
}
