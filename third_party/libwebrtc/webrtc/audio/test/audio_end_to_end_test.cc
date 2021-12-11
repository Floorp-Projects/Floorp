/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "audio/test/audio_end_to_end_test.h"
#include "system_wrappers/include/sleep.h"
#include "test/fake_audio_device.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {
// Wait half a second between stopping sending and stopping receiving audio.
constexpr int kExtraRecordTimeMs = 500;

constexpr int kSampleRate = 48000;
}  // namespace

AudioEndToEndTest::AudioEndToEndTest()
    : EndToEndTest(CallTest::kDefaultTimeoutMs) {}

FakeNetworkPipe::Config AudioEndToEndTest::GetNetworkPipeConfig() const {
  return FakeNetworkPipe::Config();
}

size_t AudioEndToEndTest::GetNumVideoStreams() const {
  return 0;
}

size_t AudioEndToEndTest::GetNumAudioStreams() const {
  return 1;
}

size_t AudioEndToEndTest::GetNumFlexfecStreams() const {
  return 0;
}

std::unique_ptr<test::FakeAudioDevice::Capturer>
    AudioEndToEndTest::CreateCapturer() {
  return test::FakeAudioDevice::CreatePulsedNoiseCapturer(32000, kSampleRate);
}

std::unique_ptr<test::FakeAudioDevice::Renderer>
    AudioEndToEndTest::CreateRenderer() {
  return test::FakeAudioDevice::CreateDiscardRenderer(kSampleRate);
}

void AudioEndToEndTest::OnFakeAudioDevicesCreated(
    test::FakeAudioDevice* send_audio_device,
    test::FakeAudioDevice* recv_audio_device) {
  send_audio_device_ = send_audio_device;
}

test::PacketTransport* AudioEndToEndTest::CreateSendTransport(
    SingleThreadedTaskQueueForTesting* task_queue,
    Call* sender_call) {
  return new test::PacketTransport(
      task_queue, sender_call, this, test::PacketTransport::kSender,
      test::CallTest::payload_type_map_, GetNetworkPipeConfig());
}

test::PacketTransport* AudioEndToEndTest::CreateReceiveTransport(
      SingleThreadedTaskQueueForTesting* task_queue) {
  return new test::PacketTransport(
      task_queue, nullptr, this, test::PacketTransport::kReceiver,
      test::CallTest::payload_type_map_, GetNetworkPipeConfig());
}

void AudioEndToEndTest::ModifyAudioConfigs(
  AudioSendStream::Config* send_config,
  std::vector<AudioReceiveStream::Config>* receive_configs) {
  // Large bitrate by default.
  const webrtc::SdpAudioFormat kDefaultFormat("opus", 48000, 2,
                                              {{"stereo", "1"}});
  send_config->send_codec_spec = AudioSendStream::Config::SendCodecSpec(
      test::CallTest::kAudioSendPayloadType, kDefaultFormat);
}

void AudioEndToEndTest::OnAudioStreamsCreated(
    AudioSendStream* send_stream,
    const std::vector<AudioReceiveStream*>& receive_streams) {
  ASSERT_NE(nullptr, send_stream);
  ASSERT_EQ(1u, receive_streams.size());
  ASSERT_NE(nullptr, receive_streams[0]);
  send_stream_ = send_stream;
  receive_stream_ = receive_streams[0];
}

void AudioEndToEndTest::PerformTest() {
  // Wait until the input audio file is done...
  send_audio_device_->WaitForRecordingEnd();
  // and some extra time to account for network delay.
  SleepMs(GetNetworkPipeConfig().queue_delay_ms + kExtraRecordTimeMs);
}
}  // namespace test
}  // namespace webrtc
