/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/audio_state.h"

#include <memory>
#include <vector>

#include "call/test/mock_audio_send_stream.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "rtc_base/ref_counted_object.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

using ::testing::_;
using ::testing::Matcher;

constexpr int kSampleRate = 16000;
constexpr int kNumberOfChannels = 1;

struct ConfigHelper {
  explicit ConfigHelper(bool use_null_audio_processing)
      : audio_mixer(AudioMixerImpl::Create()) {
    audio_state_config.audio_mixer = audio_mixer;
    audio_state_config.audio_processing =
        use_null_audio_processing
            ? nullptr
            : new rtc::RefCountedObject<
                  testing::NiceMock<MockAudioProcessing>>();
    audio_state_config.audio_device_module =
        new rtc::RefCountedObject<MockAudioDeviceModule>();
  }
  AudioState::Config& config() { return audio_state_config; }
  rtc::scoped_refptr<AudioMixer> mixer() { return audio_mixer; }

 private:
  AudioState::Config audio_state_config;
  rtc::scoped_refptr<AudioMixer> audio_mixer;
};

class FakeAudioSource : public AudioMixer::Source {
 public:
  // TODO(aleloi): Valid overrides commented out, because the gmock
  // methods don't use any override declarations, and we want to avoid
  // warnings from -Winconsistent-missing-override. See
  // http://crbug.com/428099.
  int Ssrc() const /*override*/ { return 0; }

  int PreferredSampleRate() const /*override*/ { return kSampleRate; }

  MOCK_METHOD(AudioFrameInfo,
              GetAudioFrameWithInfo,
              (int sample_rate_hz, AudioFrame*),
              (override));
};

std::vector<int16_t> Create10msTestData(int sample_rate_hz,
                                        size_t num_channels) {
  const int samples_per_channel = sample_rate_hz / 100;
  std::vector<int16_t> audio_data(samples_per_channel * num_channels, 0);
  // Fill the first channel with a 1kHz sine wave.
  const float inc = (2 * 3.14159265f * 1000) / sample_rate_hz;
  float w = 0.f;
  for (int i = 0; i < samples_per_channel; ++i) {
    audio_data[i * num_channels] = static_cast<int16_t>(32767.f * std::sin(w));
    w += inc;
  }
  return audio_data;
}

std::vector<uint32_t> ComputeChannelLevels(AudioFrame* audio_frame) {
  const size_t num_channels = audio_frame->num_channels_;
  const size_t samples_per_channel = audio_frame->samples_per_channel_;
  std::vector<uint32_t> levels(num_channels, 0);
  for (size_t i = 0; i < samples_per_channel; ++i) {
    for (size_t j = 0; j < num_channels; ++j) {
      levels[j] += std::abs(audio_frame->data()[i * num_channels + j]);
    }
  }
  return levels;
}
}  // namespace

TEST(AudioStateTest, Create) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    auto audio_state = AudioState::Create(helper.config());
    EXPECT_TRUE(audio_state.get());
  }
}

TEST(AudioStateTest, ConstructDestruct) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    rtc::scoped_refptr<internal::AudioState> audio_state(
        new rtc::RefCountedObject<internal::AudioState>(helper.config()));
  }
}

TEST(AudioStateTest, RecordedAudioArrivesAtSingleStream) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    rtc::scoped_refptr<internal::AudioState> audio_state(
        new rtc::RefCountedObject<internal::AudioState>(helper.config()));

    MockAudioSendStream stream;
    audio_state->AddSendingStream(&stream, 8000, 2);

    EXPECT_CALL(
        stream,
        SendAudioDataForMock(::testing::AllOf(
            ::testing::Field(&AudioFrame::sample_rate_hz_, ::testing::Eq(8000)),
            ::testing::Field(&AudioFrame::num_channels_, ::testing::Eq(2u)))))
        .WillOnce(
            // Verify that channels are not swapped by default.
            ::testing::Invoke([](AudioFrame* audio_frame) {
              auto levels = ComputeChannelLevels(audio_frame);
              EXPECT_LT(0u, levels[0]);
              EXPECT_EQ(0u, levels[1]);
            }));
    MockAudioProcessing* ap = use_null_audio_processing
                                  ? nullptr
                                  : static_cast<MockAudioProcessing*>(
                                        audio_state->audio_processing());
    if (ap) {
      EXPECT_CALL(*ap, set_stream_delay_ms(0));
      EXPECT_CALL(*ap, set_stream_key_pressed(false));
      EXPECT_CALL(*ap, ProcessStream(_, _, _, Matcher<int16_t*>(_)));
    }

    constexpr int kSampleRate = 16000;
    constexpr size_t kNumChannels = 2;
    auto audio_data = Create10msTestData(kSampleRate, kNumChannels);
    uint32_t new_mic_level = 667;
    audio_state->audio_transport()->RecordedDataIsAvailable(
        &audio_data[0], kSampleRate / 100, kNumChannels * 2, kNumChannels,
        kSampleRate, 0, 0, 0, false, new_mic_level);
    EXPECT_EQ(667u, new_mic_level);

    audio_state->RemoveSendingStream(&stream);
  }
}

TEST(AudioStateTest, RecordedAudioArrivesAtMultipleStreams) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    rtc::scoped_refptr<internal::AudioState> audio_state(
        new rtc::RefCountedObject<internal::AudioState>(helper.config()));

    MockAudioSendStream stream_1;
    MockAudioSendStream stream_2;
    audio_state->AddSendingStream(&stream_1, 8001, 2);
    audio_state->AddSendingStream(&stream_2, 32000, 1);

    EXPECT_CALL(
        stream_1,
        SendAudioDataForMock(::testing::AllOf(
            ::testing::Field(&AudioFrame::sample_rate_hz_,
                             ::testing::Eq(16000)),
            ::testing::Field(&AudioFrame::num_channels_, ::testing::Eq(1u)))))
        .WillOnce(
            // Verify that there is output signal.
            ::testing::Invoke([](AudioFrame* audio_frame) {
              auto levels = ComputeChannelLevels(audio_frame);
              EXPECT_LT(0u, levels[0]);
            }));
    EXPECT_CALL(
        stream_2,
        SendAudioDataForMock(::testing::AllOf(
            ::testing::Field(&AudioFrame::sample_rate_hz_,
                             ::testing::Eq(16000)),
            ::testing::Field(&AudioFrame::num_channels_, ::testing::Eq(1u)))))
        .WillOnce(
            // Verify that there is output signal.
            ::testing::Invoke([](AudioFrame* audio_frame) {
              auto levels = ComputeChannelLevels(audio_frame);
              EXPECT_LT(0u, levels[0]);
            }));
    MockAudioProcessing* ap =
        static_cast<MockAudioProcessing*>(audio_state->audio_processing());
    if (ap) {
      EXPECT_CALL(*ap, set_stream_delay_ms(5));
      EXPECT_CALL(*ap, set_stream_key_pressed(true));
      EXPECT_CALL(*ap, ProcessStream(_, _, _, Matcher<int16_t*>(_)));
    }

    constexpr int kSampleRate = 16000;
    constexpr size_t kNumChannels = 1;
    auto audio_data = Create10msTestData(kSampleRate, kNumChannels);
    uint32_t new_mic_level = 667;
    audio_state->audio_transport()->RecordedDataIsAvailable(
        &audio_data[0], kSampleRate / 100, kNumChannels * 2, kNumChannels,
        kSampleRate, 5, 0, 0, true, new_mic_level);
    EXPECT_EQ(667u, new_mic_level);

    audio_state->RemoveSendingStream(&stream_1);
    audio_state->RemoveSendingStream(&stream_2);
  }
}

TEST(AudioStateTest, EnableChannelSwap) {
  constexpr int kSampleRate = 16000;
  constexpr size_t kNumChannels = 2;

  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    rtc::scoped_refptr<internal::AudioState> audio_state(
        new rtc::RefCountedObject<internal::AudioState>(helper.config()));

    audio_state->SetStereoChannelSwapping(true);

    MockAudioSendStream stream;
    audio_state->AddSendingStream(&stream, kSampleRate, kNumChannels);

    EXPECT_CALL(stream, SendAudioDataForMock(_))
        .WillOnce(
            // Verify that channels are swapped.
            ::testing::Invoke([](AudioFrame* audio_frame) {
              auto levels = ComputeChannelLevels(audio_frame);
              EXPECT_EQ(0u, levels[0]);
              EXPECT_LT(0u, levels[1]);
            }));

    auto audio_data = Create10msTestData(kSampleRate, kNumChannels);
    uint32_t new_mic_level = 667;
    audio_state->audio_transport()->RecordedDataIsAvailable(
        &audio_data[0], kSampleRate / 100, kNumChannels * 2, kNumChannels,
        kSampleRate, 0, 0, 0, false, new_mic_level);
    EXPECT_EQ(667u, new_mic_level);

    audio_state->RemoveSendingStream(&stream);
  }
}

TEST(AudioStateTest,
     QueryingTransportForAudioShouldResultInGetAudioCallOnMixerSource) {
  for (bool use_null_audio_processing : {false, true}) {
    ConfigHelper helper(use_null_audio_processing);
    auto audio_state = AudioState::Create(helper.config());

    FakeAudioSource fake_source;
    helper.mixer()->AddSource(&fake_source);

    EXPECT_CALL(fake_source, GetAudioFrameWithInfo(_, _))
        .WillOnce(
            ::testing::Invoke([](int sample_rate_hz, AudioFrame* audio_frame) {
              audio_frame->sample_rate_hz_ = sample_rate_hz;
              audio_frame->samples_per_channel_ = sample_rate_hz / 100;
              audio_frame->num_channels_ = kNumberOfChannels;
              return AudioMixer::Source::AudioFrameInfo::kNormal;
            }));

    int16_t audio_buffer[kSampleRate / 100 * kNumberOfChannels];
    size_t n_samples_out;
    int64_t elapsed_time_ms;
    int64_t ntp_time_ms;
    audio_state->audio_transport()->NeedMorePlayData(
        kSampleRate / 100, kNumberOfChannels * 2, kNumberOfChannels,
        kSampleRate, audio_buffer, n_samples_out, &elapsed_time_ms,
        &ntp_time_ms);
  }
}
}  // namespace test
}  // namespace webrtc
