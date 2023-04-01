/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/input_volume_controller.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/metrics.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace webrtc {
namespace {

constexpr int kSampleRateHz = 32000;
constexpr int kNumChannels = 1;
constexpr int kInitialInputVolume = 128;
constexpr int kClippedMin = 165;  // Arbitrary, but different from the default.
constexpr float kAboveClippedThreshold = 0.2f;
constexpr int kMinMicLevel = 12;
constexpr int kClippedLevelStep = 15;
constexpr float kClippedRatioThreshold = 0.1f;
constexpr int kClippedWaitFrames = 300;
constexpr float kHighSpeechProbability = 0.7f;
constexpr float kLowSpeechProbability = 0.1f;
constexpr float kSpeechLevel = -25.0f;
constexpr float kSpeechProbabilityThreshold = 0.5f;
constexpr float kSpeechRatioThreshold = 0.8f;

constexpr float kMinSample = std::numeric_limits<int16_t>::min();
constexpr float kMaxSample = std::numeric_limits<int16_t>::max();

using ClippingPredictorConfig = AudioProcessing::Config::GainController1::
    AnalogGainController::ClippingPredictor;

using InputVolumeControllerConfig = InputVolumeController::Config;

constexpr InputVolumeControllerConfig kDefaultInputVolumeControllerConfig{};
constexpr ClippingPredictorConfig kDefaultClippingPredictorConfig{};

std::unique_ptr<InputVolumeController> CreateInputVolumeController(
    int clipped_level_step = kClippedLevelStep,
    float clipped_ratio_threshold = kClippedRatioThreshold,
    int clipped_wait_frames = kClippedWaitFrames,
    bool enable_clipping_predictor = false,
    int update_input_volume_wait_frames = 0) {
  InputVolumeControllerConfig config{
      .clipped_level_min = kClippedMin,
      .clipped_level_step = clipped_level_step,
      .clipped_ratio_threshold = clipped_ratio_threshold,
      .clipped_wait_frames = clipped_wait_frames,
      .enable_clipping_predictor = enable_clipping_predictor,
      .target_range_max_dbfs = -18,
      .target_range_min_dbfs = -30,
      .update_input_volume_wait_frames = update_input_volume_wait_frames,
      .speech_probability_threshold = kSpeechProbabilityThreshold,
      .speech_ratio_threshold = kSpeechRatioThreshold,
  };

  return std::make_unique<InputVolumeController>(/*num_capture_channels=*/1,
                                                 config);
}

constexpr char kMinInputVolumeFieldTrial[] = "WebRTC-Audio-Agc2-MinInputVolume";

std::string GetAgcMinInputVolumeFieldTrial(const std::string& value) {
  char field_trial_buffer[64];
  rtc::SimpleStringBuilder builder(field_trial_buffer);
  builder << kMinInputVolumeFieldTrial << "/" << value << "/";
  return builder.str();
}

std::string GetAgcMinInputVolumeFieldTrialEnabled(
    int enabled_value,
    const std::string& suffix = "") {
  RTC_DCHECK_GE(enabled_value, 0);
  RTC_DCHECK_LE(enabled_value, 255);
  char field_trial_buffer[64];
  rtc::SimpleStringBuilder builder(field_trial_buffer);
  builder << kMinInputVolumeFieldTrial << "/Enabled-" << enabled_value << suffix
          << "/";
  return builder.str();
}

std::string GetAgcMinInputVolumeFieldTrial(absl::optional<int> volume) {
  if (volume.has_value()) {
    return GetAgcMinInputVolumeFieldTrialEnabled(*volume);
  }
  return GetAgcMinInputVolumeFieldTrial("Disabled");
}

// (Over)writes `samples_value` for the samples in `audio_buffer`.
// When `clipped_ratio`, a value in [0, 1], is greater than 0, the corresponding
// fraction of the frame is set to a full scale value to simulate clipping.
void WriteAudioBufferSamples(float samples_value,
                             float clipped_ratio,
                             AudioBuffer& audio_buffer) {
  RTC_DCHECK_GE(samples_value, kMinSample);
  RTC_DCHECK_LE(samples_value, kMaxSample);
  RTC_DCHECK_GE(clipped_ratio, 0.0f);
  RTC_DCHECK_LE(clipped_ratio, 1.0f);
  int num_channels = audio_buffer.num_channels();
  int num_samples = audio_buffer.num_frames();
  int num_clipping_samples = clipped_ratio * num_samples;
  for (int ch = 0; ch < num_channels; ++ch) {
    int i = 0;
    for (; i < num_clipping_samples; ++i) {
      audio_buffer.channels()[ch][i] = 32767.0f;
    }
    for (; i < num_samples; ++i) {
      audio_buffer.channels()[ch][i] = samples_value;
    }
  }
}

// (Over)writes samples in `audio_buffer`. Alternates samples `samples_value`
// and zero.
void WriteAlternatingAudioBufferSamples(float samples_value,
                                        AudioBuffer& audio_buffer) {
  RTC_DCHECK_GE(samples_value, kMinSample);
  RTC_DCHECK_LE(samples_value, kMaxSample);
  const int num_channels = audio_buffer.num_channels();
  const int num_frames = audio_buffer.num_frames();
  for (int ch = 0; ch < num_channels; ++ch) {
    for (int i = 0; i < num_frames; i += 2) {
      audio_buffer.channels()[ch][i] = samples_value;
      audio_buffer.channels()[ch][i + 1] = 0.0f;
    }
  }
}

// Deprecated.
// TODO(bugs.webrtc.org/7494): Delete this helper, use
// `InputVolumeControllerTestHelper::CallAgcSequence()` instead.
int CallAnalyzeAndRecommend(int num_calls,
                            int initial_volume,
                            const AudioBuffer& audio_buffer,
                            float speech_probability,
                            absl::optional<float> speech_level_dbfs,
                            InputVolumeController& controller) {
  RTC_DCHECK(controller.capture_output_used());
  int volume = initial_volume;
  for (int n = 0; n < num_calls; ++n) {
    controller.AnalyzeInputAudio(volume, audio_buffer);
    const auto recommended_input_volume =
        controller.RecommendInputVolume(speech_probability, speech_level_dbfs);

    // Expect no errors: Applied volume set for every frame;
    // `RecommendInputVolume()` returns a non-empty value.
    EXPECT_TRUE(recommended_input_volume.has_value());

    volume = *recommended_input_volume;
  }
  return volume;
}

// Reads a given number of 10 ms chunks from a PCM file and feeds them to
// `InputVolumeController`.
class SpeechSamplesReader {
 private:
  // Recording properties.
  static constexpr int kPcmSampleRateHz = 16000;
  static constexpr int kPcmNumChannels = 1;
  static constexpr int kPcmBytesPerSamples = sizeof(int16_t);

 public:
  SpeechSamplesReader()
      : is_(test::ResourcePath("audio_processing/agc/agc_audio", "pcm"),
            std::ios::binary | std::ios::ate),
        audio_buffer_(kPcmSampleRateHz,
                      kPcmNumChannels,
                      kPcmSampleRateHz,
                      kPcmNumChannels,
                      kPcmSampleRateHz,
                      kPcmNumChannels),
        buffer_(audio_buffer_.num_frames()),
        buffer_num_bytes_(buffer_.size() * kPcmBytesPerSamples) {
    RTC_CHECK(is_);
  }

  // Reads `num_frames` 10 ms frames from the beginning of the PCM file, applies
  // `gain_db` and feeds the frames into `controller` by calling
  // `AnalyzeInputAudio()` and `RecommendInputVolume()` for each frame. Reads
  // the number of 10 ms frames available in the PCM file if `num_frames` is too
  // large - i.e., does not loop. `speech_probability` and `speech_level_dbfs`
  // are passed to `RecommendInputVolume()`.
  int Feed(int num_frames,
           int applied_input_volume,
           int gain_db,
           float speech_probability,
           absl::optional<float> speech_level_dbfs,
           InputVolumeController& controller) {
    RTC_DCHECK(controller.capture_output_used());

    float gain = std::pow(10.0f, gain_db / 20.0f);  // From dB to linear gain.
    is_.seekg(0, is_.beg);  // Start from the beginning of the PCM file.

    // Read and feed frames.
    for (int i = 0; i < num_frames; ++i) {
      is_.read(reinterpret_cast<char*>(buffer_.data()), buffer_num_bytes_);
      if (is_.gcount() < buffer_num_bytes_) {
        // EOF reached. Stop.
        break;
      }
      // Apply gain and copy samples into `audio_buffer_`.
      std::transform(buffer_.begin(), buffer_.end(),
                     audio_buffer_.channels()[0], [gain](int16_t v) -> float {
                       return rtc::SafeClamp(static_cast<float>(v) * gain,
                                             kMinSample, kMaxSample);
                     });
      controller.AnalyzeInputAudio(applied_input_volume, audio_buffer_);
      const auto recommended_input_volume = controller.RecommendInputVolume(
          speech_probability, speech_level_dbfs);

      // Expect no errors: Applied volume set for every frame;
      // `RecommendInputVolume()` returns a non-empty value.
      EXPECT_TRUE(recommended_input_volume.has_value());

      applied_input_volume = *recommended_input_volume;
    }
    return applied_input_volume;
  }

 private:
  std::ifstream is_;
  AudioBuffer audio_buffer_;
  std::vector<int16_t> buffer_;
  const std::streamsize buffer_num_bytes_;
};

// Runs the MonoInputVolumeControl processing sequence following the API
// contract. Returns the updated recommended input volume.
float UpdateRecommendedInputVolume(MonoInputVolumeController& mono_controller,
                                   int applied_input_volume,
                                   float speech_probability,
                                   absl::optional<float> rms_error_dbfs) {
  mono_controller.set_stream_analog_level(applied_input_volume);
  EXPECT_EQ(mono_controller.recommended_analog_level(), applied_input_volume);
  mono_controller.Process(rms_error_dbfs, speech_probability);
  return mono_controller.recommended_analog_level();
}

}  // namespace

// TODO(bugs.webrtc.org/12874): Use constexpr struct with designated
// initializers once fixed.
constexpr InputVolumeControllerConfig GetInputVolumeControllerTestConfig() {
  InputVolumeControllerConfig config{
      .clipped_level_min = kClippedMin,
      .clipped_level_step = kClippedLevelStep,
      .clipped_ratio_threshold = kClippedRatioThreshold,
      .clipped_wait_frames = kClippedWaitFrames,
      .enable_clipping_predictor = kDefaultClippingPredictorConfig.enabled,
      .target_range_max_dbfs = -18,
      .target_range_min_dbfs = -30,
      .update_input_volume_wait_frames = 0,
      .speech_probability_threshold = 0.5f,
      .speech_ratio_threshold = 1.0f,
  };
  return config;
}

// Helper class that provides an `InputVolumeController` instance with an
// `AudioBuffer` instance and `CallAgcSequence()`, a helper method that runs the
// `InputVolumeController` instance on the `AudioBuffer` one by sticking to the
// API contract.
class InputVolumeControllerTestHelper {
 public:
  // Ctor. Initializes `audio_buffer` with zeros.
  // TODO(bugs.webrtc.org/7494): Remove the default argument.
  InputVolumeControllerTestHelper(const InputVolumeController::Config& config =
                                      GetInputVolumeControllerTestConfig())
      : audio_buffer(kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels,
                     kSampleRateHz,
                     kNumChannels),
        controller(/*num_capture_channels=*/1, config) {
    controller.Initialize();
    WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.0f,
                            audio_buffer);
  }

  // Calls the sequence of `InputVolumeController` methods according to the API
  // contract, namely:
  // - Sets the applied input volume;
  // - Uses `audio_buffer` to call `AnalyzeInputAudio()` and
  // `RecommendInputVolume()`;
  //  Returns the recommended input volume.
  absl::optional<int> CallAgcSequence(int applied_input_volume,
                                      float speech_probability,
                                      absl::optional<float> speech_level_dbfs,
                                      int num_calls = 1) {
    RTC_DCHECK_GE(num_calls, 1);
    absl::optional<int> volume = applied_input_volume;
    for (int i = 0; i < num_calls; ++i) {
      // Repeat the initial volume if `RecommendInputVolume()` doesn't return a
      // value.
      controller.AnalyzeInputAudio(volume.value_or(applied_input_volume),
                                   audio_buffer);
      volume = controller.RecommendInputVolume(speech_probability,
                                               speech_level_dbfs);

      // Allow deviation from the API contract: `RecommendInputVolume()` doesn't
      // return a recommended input volume.
      if (volume.has_value()) {
        EXPECT_EQ(*volume, controller.recommended_input_volume());
      }
    }
    return volume;
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  int CallRecommendInputVolume(int num_calls,
                               int initial_volume,
                               float speech_probability,
                               absl::optional<float> speech_level_dbfs) {
    RTC_DCHECK(controller.capture_output_used());

    // Create non-clipping audio for `AnalyzeInputAudio()`.
    WriteAlternatingAudioBufferSamples(0.1f * kMaxSample, audio_buffer);
    int volume = initial_volume;
    for (int i = 0; i < num_calls; ++i) {
      controller.AnalyzeInputAudio(volume, audio_buffer);
      const auto recommended_input_volume = controller.RecommendInputVolume(
          speech_probability, speech_level_dbfs);

      // Expect no errors: Applied volume set for every frame;
      // `RecommendInputVolume()` returns a non-empty value.
      EXPECT_TRUE(recommended_input_volume.has_value());

      volume = *recommended_input_volume;
    }
    return volume;
  }

  // Deprecated.
  // TODO(bugs.webrtc.org/7494): Let the caller write `audio_buffer` and use
  // `CallAgcSequence()`.
  void CallAnalyzeInputAudio(int num_calls, float clipped_ratio) {
    RTC_DCHECK(controller.capture_output_used());

    RTC_DCHECK_GE(clipped_ratio, 0.0f);
    RTC_DCHECK_LE(clipped_ratio, 1.0f);
    WriteAudioBufferSamples(/*samples_value=*/0.0f, clipped_ratio,
                            audio_buffer);
    for (int i = 0; i < num_calls; ++i) {
      controller.AnalyzeInputAudio(controller.recommended_input_volume(),
                                   audio_buffer);
    }
  }

  AudioBuffer audio_buffer;
  InputVolumeController controller;
};

class InputVolumeControllerParametrizedTest
    : public ::testing::TestWithParam<absl::optional<int>> {
 protected:
  InputVolumeControllerParametrizedTest()
      : field_trials_(GetAgcMinInputVolumeFieldTrial(GetParam())) {}

  int GetMinInputVolume() const { return GetParam().value_or(kMinMicLevel); }

 private:
  test::ScopedFieldTrials field_trials_;
};

INSTANTIATE_TEST_SUITE_P(,
                         InputVolumeControllerParametrizedTest,
                         ::testing::Values(absl::nullopt, 12, 20));

TEST_P(InputVolumeControllerParametrizedTest,
       StartupMinVolumeConfigurationIsRespected) {
  InputVolumeControllerTestHelper helper;

  EXPECT_EQ(*helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                                    kSpeechLevel),
            kInitialInputVolume);
}

TEST_P(InputVolumeControllerParametrizedTest, MicVolumeResponseToRmsError) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(kInitialInputVolume,
                                       kHighSpeechProbability, kSpeechLevel);

  // Inside the digital gain's window; no change of volume.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -23.0f);

  // Inside the digital gain's window; no change of volume.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -28.0f);

  // Above the digital gain's  window; volume should be increased.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -29.0f);
  EXPECT_EQ(volume, 128);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -38.0f);
  EXPECT_EQ(volume, 156);

  // Inside the digital gain's window; no change of volume.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -23.0f);
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -18.0f);

  // Below the digial gain's window; volume should be decreased.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -17.0f);
  EXPECT_EQ(volume, 155);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -17.0f);
  EXPECT_EQ(volume, 151);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -9.0f);
  EXPECT_EQ(volume, 119);
}

TEST_P(InputVolumeControllerParametrizedTest, MicVolumeIsLimited) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(kInitialInputVolume,
                                       kHighSpeechProbability, kSpeechLevel);

  // Maximum upwards change is limited.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 183);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 243);

  // Won't go higher than the maximum.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 255);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -17.0f);
  EXPECT_EQ(volume, 254);

  // Maximum downwards change is limited.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, 194);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, 137);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, 88);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, 54);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, 33);

  // Won't go lower than the minimum.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, std::max(18, GetMinInputVolume()));

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, 22.0f);
  EXPECT_EQ(volume, std::max(12, GetMinInputVolume()));
}

TEST_P(InputVolumeControllerParametrizedTest, NoActionWhileMuted) {
  InputVolumeControllerTestHelper helper_1;
  InputVolumeControllerTestHelper helper_2;

  int volume_1 = *helper_1.CallAgcSequence(/*applied_input_volume=*/255,
                                           kHighSpeechProbability, kSpeechLevel,
                                           /*num_calls=*/1);
  int volume_2 = *helper_2.CallAgcSequence(/*applied_input_volume=*/255,
                                           kHighSpeechProbability, kSpeechLevel,
                                           /*num_calls=*/1);

  EXPECT_EQ(volume_1, 255);
  EXPECT_EQ(volume_2, 255);

  helper_2.controller.HandleCaptureOutputUsedChange(false);

  WriteAlternatingAudioBufferSamples(kMaxSample, helper_1.audio_buffer);
  WriteAlternatingAudioBufferSamples(kMaxSample, helper_2.audio_buffer);

  volume_1 =
      *helper_1.CallAgcSequence(volume_1, kHighSpeechProbability, kSpeechLevel,
                                /*num_calls=*/1);
  volume_2 =
      *helper_2.CallAgcSequence(volume_2, kHighSpeechProbability, kSpeechLevel,
                                /*num_calls=*/1);

  EXPECT_LT(volume_1, 255);
  EXPECT_EQ(volume_2, 255);
}

TEST_P(InputVolumeControllerParametrizedTest,
       UnmutingChecksVolumeWithoutRaising) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.controller.HandleCaptureOutputUsedChange(false);
  helper.controller.HandleCaptureOutputUsedChange(true);

  constexpr int kInputVolume = 127;

  // SetMicVolume should not be called.
  EXPECT_EQ(
      helper.CallRecommendInputVolume(/*num_calls=*/1, kInputVolume,
                                      kHighSpeechProbability, kSpeechLevel),
      kInputVolume);
}

TEST_P(InputVolumeControllerParametrizedTest, UnmutingRaisesTooLowVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.controller.HandleCaptureOutputUsedChange(false);
  helper.controller.HandleCaptureOutputUsedChange(true);

  constexpr int kInputVolume = 11;

  EXPECT_EQ(
      helper.CallRecommendInputVolume(/*num_calls=*/1, kInputVolume,
                                      kHighSpeechProbability, kSpeechLevel),
      GetMinInputVolume());
}

TEST_P(InputVolumeControllerParametrizedTest,
       ManualLevelChangeResultsInNoSetMicCall) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(kInitialInputVolume,
                                       kHighSpeechProbability, kSpeechLevel);

  // GetMicVolume returns a value outside of the quantization slack, indicating
  // a manual volume change.
  ASSERT_NE(volume, 154);
  volume = helper.CallRecommendInputVolume(
      /*num_calls=*/1, /*initial_volume=*/154, kHighSpeechProbability, -29.0f);
  EXPECT_EQ(volume, 154);

  // Do the same thing, except downwards now.
  volume = helper.CallRecommendInputVolume(
      /*num_calls=*/1, /*initial_volume=*/100, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(volume, 100);

  // And finally verify the AGC continues working without a manual change.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -17.0f);
  EXPECT_EQ(volume, 99);
}

TEST_P(InputVolumeControllerParametrizedTest,
       RecoveryAfterManualLevelChangeFromMax) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(kInitialInputVolume,
                                       kHighSpeechProbability, kSpeechLevel);

  // Force the mic up to max volume. Takes a few steps due to the residual
  // gain limitation.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 183);
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 243);
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 255);

  // Manual change does not result in SetMicVolume call.
  volume = helper.CallRecommendInputVolume(
      /*num_calls=*/1, /*initial_volume=*/50, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 50);

  // Continues working as usual afterwards.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -38.0f);

  EXPECT_EQ(volume, 65);
}

// Checks that the minimum input volume is enforced during the upward adjustment
// of the input volume.
TEST_P(InputVolumeControllerParametrizedTest,
       EnforceMinInputVolumeDuringUpwardsAdjustment) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(kInitialInputVolume,
                                       kHighSpeechProbability, kSpeechLevel);

  // Manual change below min, but strictly positive, otherwise no action will be
  // taken.
  volume = helper.CallRecommendInputVolume(
      /*num_calls=*/1, /*initial_volume=*/1, kHighSpeechProbability, -17.0f);

  // Trigger an upward adjustment of the input volume.
  EXPECT_EQ(volume, GetMinInputVolume());
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -29.0f);
  EXPECT_EQ(volume, GetMinInputVolume());
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -30.0f);
  EXPECT_EQ(volume, GetMinInputVolume());

  // After a number of consistently low speech level observations, the input
  // volume is eventually raised above the minimum.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/10, volume,
                                           kHighSpeechProbability, -38.0f);
  EXPECT_GT(volume, GetMinInputVolume());
}

// Checks that, when the min mic level override is specified, AGC immediately
// applies the minimum mic level after the mic level is manually set below the
// minimum gain to enforce.
TEST_P(InputVolumeControllerParametrizedTest,
       RecoveryAfterManualLevelChangeBelowMin) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(kInitialInputVolume,
                                       kHighSpeechProbability, kSpeechLevel);

  // Manual change below min, but strictly positive, otherwise
  // AGC won't take any action.
  volume = helper.CallRecommendInputVolume(
      /*num_calls=*/1, /*initial_volume=*/1, kHighSpeechProbability, -17.0f);
  EXPECT_EQ(volume, GetMinInputVolume());
}

TEST_P(InputVolumeControllerParametrizedTest, NoClippingHasNoImpact) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/100, /*clipped_ratio=*/0);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 128);
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingUnderThresholdHasNoImpact) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1, /*clipped_ratio=*/0.099);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 128);
}

TEST_P(InputVolumeControllerParametrizedTest, ClippingLowersVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1, /*clipped_ratio=*/0.2);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 240);
}

TEST_P(InputVolumeControllerParametrizedTest,
       WaitingPeriodBetweenClippingChecks) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 240);

  helper.CallAnalyzeInputAudio(/*num_calls=*/300,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 240);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 225);
}

TEST_P(InputVolumeControllerParametrizedTest, ClippingLoweringIsLimited) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/180, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), kClippedMin);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1000,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), kClippedMin);
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingMaxIsRespectedWhenEqualToLevel) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/255, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 240);

  helper.CallRecommendInputVolume(/*num_calls=*/10, /*initial_volume=*/240,
                                  kHighSpeechProbability, -48.0f);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 240);
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingMaxIsRespectedWhenHigherThanLevel) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/200, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  int volume = helper.controller.recommended_input_volume();
  EXPECT_EQ(volume, 185);

  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -58.0f);
  EXPECT_EQ(volume, 240);
  volume = helper.CallRecommendInputVolume(/*num_calls=*/10, volume,
                                           kHighSpeechProbability, -58.0f);
  EXPECT_EQ(volume, 240);
}

TEST_P(InputVolumeControllerParametrizedTest, UserCanRaiseVolumeAfterClipping) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/225, kHighSpeechProbability,
                         kSpeechLevel);

  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), 210);

  // User changed the volume.
  int volume = helper.CallRecommendInputVolume(
      /*num_calls=*/1, /*initial_volume-*/ 250, kHighSpeechProbability, -32.0f);
  EXPECT_EQ(volume, 250);

  // Move down...
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -8.0f);
  EXPECT_EQ(volume, 210);
  // And back up to the new max established by the user.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -58.0f);
  EXPECT_EQ(volume, 250);
  // Will not move above new maximum.
  volume = helper.CallRecommendInputVolume(/*num_calls=*/1, volume,
                                           kHighSpeechProbability, -48.0f);
  EXPECT_EQ(volume, 250);
}

TEST_P(InputVolumeControllerParametrizedTest,
       ClippingDoesNotPullLowVolumeBackUp) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(/*applied_input_volume=*/80, kHighSpeechProbability,
                         kSpeechLevel);

  int initial_volume = helper.controller.recommended_input_volume();
  helper.CallAnalyzeInputAudio(/*num_calls=*/1,
                               /*clipped_ratio=*/kAboveClippedThreshold);
  EXPECT_EQ(helper.controller.recommended_input_volume(), initial_volume);
}

TEST_P(InputVolumeControllerParametrizedTest, TakesNoActionOnZeroMicVolume) {
  InputVolumeControllerTestHelper helper;
  helper.CallAgcSequence(kInitialInputVolume, kHighSpeechProbability,
                         kSpeechLevel);

  EXPECT_EQ(
      helper.CallRecommendInputVolume(/*num_calls=*/10, /*initial_volume=*/0,
                                      kHighSpeechProbability, -48.0f),
      0);
}

TEST_P(InputVolumeControllerParametrizedTest, ClippingDetectionLowersVolume) {
  InputVolumeControllerTestHelper helper;
  int volume = *helper.CallAgcSequence(/*applied_input_volume=*/255,
                                       kHighSpeechProbability, kSpeechLevel,
                                       /*num_calls=*/1);

  EXPECT_EQ(volume, 255);

  WriteAlternatingAudioBufferSamples(0.99f * kMaxSample, helper.audio_buffer);
  volume = *helper.CallAgcSequence(volume, kHighSpeechProbability, kSpeechLevel,
                                   /*num_calls=*/100);

  EXPECT_EQ(volume, 255);

  WriteAlternatingAudioBufferSamples(kMaxSample, helper.audio_buffer);
  volume = *helper.CallAgcSequence(volume, kHighSpeechProbability, kSpeechLevel,
                                   /*num_calls=*/100);

  EXPECT_EQ(volume, 240);
}

TEST(InputVolumeControllerTest, MinInputVolumeDefault) {
  std::unique_ptr<InputVolumeController> controller =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames);
  EXPECT_EQ(controller->channel_controllers_[0]->min_input_volume(),
            kMinMicLevel);
}

TEST(InputVolumeControllerTest, MinInputVolumeDisabled) {
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrial("Disabled" + field_trial_suffix));
    std::unique_ptr<InputVolumeController> controller =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);

    EXPECT_EQ(controller->channel_controllers_[0]->min_input_volume(),
              kMinMicLevel);
  }
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(InputVolumeControllerTest, MinInputVolumeOutOfRangeAbove) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinInputVolumeFieldTrial("Enabled-256"));
  std::unique_ptr<InputVolumeController> controller =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames);
  EXPECT_EQ(controller->channel_controllers_[0]->min_input_volume(),
            kMinMicLevel);
}

// Checks that a field-trial parameter outside of the valid range [0,255] is
// ignored.
TEST(InputVolumeControllerTest, MinInputVolumeOutOfRangeBelow) {
  test::ScopedFieldTrials field_trial(
      GetAgcMinInputVolumeFieldTrial("Enabled--1"));
  std::unique_ptr<InputVolumeController> controller =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames);
  EXPECT_EQ(controller->channel_controllers_[0]->min_input_volume(),
            kMinMicLevel);
}

// Verifies that a valid experiment changes the minimum microphone level. The
// start volume is larger than the min level and should therefore not be
// changed.
TEST(InputVolumeControllerTest, MinInputVolumeEnabled50) {
  constexpr int kMinInputVolume = 50;
  for (const std::string& field_trial_suffix : {"", "_20220210"}) {
    SCOPED_TRACE(field_trial_suffix);
    test::ScopedFieldTrials field_trial(GetAgcMinInputVolumeFieldTrialEnabled(
        kMinInputVolume, field_trial_suffix));
    std::unique_ptr<InputVolumeController> controller =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);

    EXPECT_EQ(controller->channel_controllers_[0]->min_input_volume(),
              kMinInputVolume);
  }
}

// Checks that, when the "WebRTC-Audio-Agc2-MinInputVolume" field trial is
// specified with a valid value, the mic level never gets lowered beyond the
// override value in the presence of clipping.
TEST(InputVolumeControllerTest, MinInputVolumeCheckMinLevelWithClipping) {
  constexpr int kMinInputVolume = 250;

  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    std::unique_ptr<InputVolumeController> controller =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);
    controller->Initialize();
    return controller;
  };
  std::unique_ptr<InputVolumeController> controller = factory();
  std::unique_ptr<InputVolumeController> controller_with_override;
  {
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinInputVolume));
    controller_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain. Use low speech probability to limit the
  // volume changes to clipping handling.
  const int volume = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      kLowSpeechProbability, /*speech_level_dbfs=*/-42.0f, *controller);
  const int volume_with_override = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      kLowSpeechProbability, /*speech_level_dbfs=*/-42.0f,
      *controller_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(volume, 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `controller`, which is allowed to reach a lower gain.
  EXPECT_GT(volume_with_override, volume);
  // Check that the gain selected by `controller_with_override` equals the
  // minimum value overridden via field trial.
  EXPECT_EQ(volume_with_override, kMinInputVolume);
}

// Checks that, when the "WebRTC-Audio-Agc2-MinInputVolume" field trial is
// specified with a valid value, the mic level never gets lowered beyond the
// override value in the presence of clipping when RMS error is not empty.
// TODO(webrtc:7494): Revisit the test after moving the number of update wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST(InputVolumeControllerTest,
     MinInputVolumeCheckMinLevelWithClippingWithRmsError) {
  constexpr int kMinInputVolume = 250;

  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    std::unique_ptr<InputVolumeController> controller =
        CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                    kClippedWaitFrames);
    controller->Initialize();
    return controller;
  };
  std::unique_ptr<InputVolumeController> controller = factory();
  std::unique_ptr<InputVolumeController> controller_with_override;
  {
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinInputVolume));
    controller_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain.
  const int volume = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      kHighSpeechProbability,
      /*speech_level_dbfs=*/-18.0f, *controller);
  const int volume_with_override = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      kHighSpeechProbability,
      /*speech_level_dbfs=*/-18.0f, *controller_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(volume, 0);

  // Check that the test signal triggers a larger downward adaptation for
  // `controller`, which is allowed to reach a lower gain.
  EXPECT_GT(volume_with_override, volume);

  // Check that the gain selected by `controller_with_override` equals the
  // minimum value overridden via field trial.
  EXPECT_EQ(volume_with_override, kMinInputVolume);
}

// Checks that, when the "WebRTC-Audio-Agc2-MinInputVolume" field trial is
// specified with a value lower than the `clipped_level_min`, the behavior of
// the analog gain controller is the same as that obtained when the field trial
// is not specified.
TEST(InputVolumeControllerTest, MinInputVolumeCompareMicLevelWithClipping) {
  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    // Use a large clipped level step to more quickly decrease the analog gain
    // with clipping.
    InputVolumeControllerConfig config = kDefaultInputVolumeControllerConfig;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller = std::make_unique<InputVolumeController>(
        /*num_capture_channels=*/1, config);
    controller->Initialize();
    return controller;
  };
  std::unique_ptr<InputVolumeController> controller = factory();
  std::unique_ptr<InputVolumeController> controller_with_override;
  {
    constexpr int kMinInputVolume = 20;
    static_assert(kDefaultInputVolumeControllerConfig.clipped_level_min >=
                      kMinInputVolume,
                  "Use a lower override value.");
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinInputVolume));
    controller_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  // Simulate 4 seconds of clipping; it is expected to trigger a downward
  // adjustment of the analog gain. Use low speech probability to limit the
  // volume changes to clipping handling.
  const int volume = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      kLowSpeechProbability, /*speech_level_dbfs=*/-18, *controller);
  const int volume_with_override = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      kLowSpeechProbability, /*speech_level_dbfs=*/-18,
      *controller_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(volume, 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(volume, volume_with_override);
  EXPECT_EQ(volume_with_override,
            kDefaultInputVolumeControllerConfig.clipped_level_min);
}

// Checks that, when the "WebRTC-Audio-Agc2-MinInputVolume" field trial is
// specified with a value lower than the `clipped_level_min`, the behavior of
// the analog gain controller is the same as that obtained when the field trial
// is not specified.
// TODO(webrtc:7494): Revisit the test after moving the number of update wait
// frames to APM config. The test passes but internally the gain update timing
// differs.
TEST(InputVolumeControllerTest,
     MinInputVolumeCompareMicLevelWithClippingWithRmsError) {
  // Create and initialize two AGCs by specifying and leaving unspecified the
  // relevant field trial.
  const auto factory = []() {
    // Use a large clipped level step to more quickly decrease the analog gain
    // with clipping.
    InputVolumeControllerConfig config = kDefaultInputVolumeControllerConfig;
    config.clipped_level_step = 64;
    config.clipped_ratio_threshold = kClippedRatioThreshold;
    config.clipped_wait_frames = kClippedWaitFrames;
    auto controller = std::make_unique<InputVolumeController>(
        /*num_capture_channels=*/1, config);
    controller->Initialize();
    return controller;
  };
  std::unique_ptr<InputVolumeController> controller = factory();
  std::unique_ptr<InputVolumeController> controller_with_override;
  {
    constexpr int kMinInputVolume = 20;
    static_assert(kDefaultInputVolumeControllerConfig.clipped_level_min >=
                      kMinInputVolume,
                  "Use a lower override value.");
    test::ScopedFieldTrials field_trial(
        GetAgcMinInputVolumeFieldTrialEnabled(kMinInputVolume));
    controller_with_override = factory();
  }

  // Create a test input signal which containts 80% of clipped samples.
  AudioBuffer audio_buffer(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz,
                           1);
  WriteAudioBufferSamples(/*samples_value=*/4000.0f, /*clipped_ratio=*/0.8f,
                          audio_buffer);

  const int volume = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      /*speech_probability=*/0.7f,
      /*speech_level_dbfs=*/-18.0f, *controller);
  const int volume_with_override = CallAnalyzeAndRecommend(
      /*num_calls=*/400, kInitialInputVolume, audio_buffer,
      /*speech_probability=*/0.7f,
      /*speech_level_dbfs=*/-18.0f, *controller_with_override);

  // Make sure that an adaptation occurred.
  ASSERT_GT(volume, 0);

  // Check that the selected analog gain is the same for both controllers and
  // that it equals the minimum level reached when clipping is handled. That is
  // expected because the minimum microphone level override is less than the
  // minimum level used when clipping is detected.
  EXPECT_EQ(volume, volume_with_override);
  EXPECT_EQ(volume_with_override,
            kDefaultInputVolumeControllerConfig.clipped_level_min);
}

// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_level_step`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_ratio_threshold`.
// TODO(bugs.webrtc.org/12774): Test the bahavior of `clipped_wait_frames`.
// Verifies that configurable clipping parameters are initialized as intended.
TEST_P(InputVolumeControllerParametrizedTest, ClippingParametersVerified) {
  std::unique_ptr<InputVolumeController> controller =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames);
  controller->Initialize();
  EXPECT_EQ(controller->clipped_level_step_, kClippedLevelStep);
  EXPECT_EQ(controller->clipped_ratio_threshold_, kClippedRatioThreshold);
  EXPECT_EQ(controller->clipped_wait_frames_, kClippedWaitFrames);
  std::unique_ptr<InputVolumeController> controller_custom =
      CreateInputVolumeController(/*clipped_level_step=*/10,
                                  /*clipped_ratio_threshold=*/0.2f,
                                  /*clipped_wait_frames=*/50);
  controller_custom->Initialize();
  EXPECT_EQ(controller_custom->clipped_level_step_, 10);
  EXPECT_EQ(controller_custom->clipped_ratio_threshold_, 0.2f);
  EXPECT_EQ(controller_custom->clipped_wait_frames_, 50);
}

TEST_P(InputVolumeControllerParametrizedTest,
       DisableClippingPredictorDisablesClippingPredictor) {
  std::unique_ptr<InputVolumeController> controller =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false);
  controller->Initialize();

  EXPECT_FALSE(controller->clipping_predictor_enabled());
  EXPECT_FALSE(controller->use_clipping_predictor_step());
}

TEST_P(InputVolumeControllerParametrizedTest,
       EnableClippingPredictorEnablesClippingPredictor) {
  std::unique_ptr<InputVolumeController> controller =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/true);
  controller->Initialize();

  EXPECT_TRUE(controller->clipping_predictor_enabled());
  EXPECT_TRUE(controller->use_clipping_predictor_step());
}

TEST_P(InputVolumeControllerParametrizedTest,
       DisableClippingPredictorDoesNotLowerVolume) {
  int volume = 255;
  InputVolumeControllerConfig config = GetInputVolumeControllerTestConfig();
  config.enable_clipping_predictor = false;
  auto helper = InputVolumeControllerTestHelper(config);
  helper.controller.Initialize();

  EXPECT_FALSE(helper.controller.clipping_predictor_enabled());
  EXPECT_FALSE(helper.controller.use_clipping_predictor_step());

  // Expect no change if clipping prediction is enabled.
  for (int j = 0; j < 31; ++j) {
    WriteAlternatingAudioBufferSamples(0.99f * kMaxSample, helper.audio_buffer);
    volume =
        *helper.CallAgcSequence(volume, kLowSpeechProbability, kSpeechLevel,
                                /*num_calls=*/5);

    WriteAudioBufferSamples(0.99f * kMaxSample, /*clipped_ratio=*/0.0f,
                            helper.audio_buffer);
    volume =
        *helper.CallAgcSequence(volume, kLowSpeechProbability, kSpeechLevel,
                                /*num_calls=*/5);

    EXPECT_EQ(volume, 255);
  }
}

// TODO(bugs.webrtc.org/7494): Split into several smaller tests.
TEST_P(InputVolumeControllerParametrizedTest,
       UsedClippingPredictionsProduceLowerAnalogLevels) {
  constexpr int kInitialLevel = 255;
  constexpr float kCloseToClippingPeakRatio = 0.99f;
  int volume_1 = kInitialLevel;
  int volume_2 = kInitialLevel;

  // Create two helpers, one with clipping prediction and one without.
  auto config_1 = GetInputVolumeControllerTestConfig();
  auto config_2 = GetInputVolumeControllerTestConfig();
  config_1.enable_clipping_predictor = true;
  config_2.enable_clipping_predictor = false;
  auto helper_1 = InputVolumeControllerTestHelper(config_1);
  auto helper_2 = InputVolumeControllerTestHelper(config_2);
  helper_1.controller.Initialize();
  helper_2.controller.Initialize();

  EXPECT_TRUE(helper_1.controller.clipping_predictor_enabled());
  EXPECT_FALSE(helper_2.controller.clipping_predictor_enabled());
  EXPECT_TRUE(helper_1.controller.use_clipping_predictor_step());

  // Expect a change if clipping prediction is enabled.
  WriteAlternatingAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                                     helper_1.audio_buffer);
  WriteAlternatingAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                                     helper_2.audio_buffer);
  volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                       kSpeechLevel, 5);
  volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                       kSpeechLevel, 5);

  WriteAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                          /*clipped_ratio=*/0.0f, helper_1.audio_buffer);
  WriteAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                          /*clipped_ratio=*/0.0f, helper_2.audio_buffer);
  volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                       kSpeechLevel, 5);
  volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                       kSpeechLevel, 5);

  EXPECT_EQ(volume_1, kInitialLevel - kClippedLevelStep);
  EXPECT_EQ(volume_2, kInitialLevel);

  // Expect no change during waiting.
  for (int i = 0; i < kClippedWaitFrames / 10; ++i) {
    WriteAlternatingAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                                       helper_1.audio_buffer);
    WriteAlternatingAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                                       helper_2.audio_buffer);
    volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                         kSpeechLevel, 5);
    volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                         kSpeechLevel, 5);

    WriteAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                            /*clipped_ratio=*/0.0f, helper_1.audio_buffer);
    WriteAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                            /*clipped_ratio=*/0.0f, helper_2.audio_buffer);
    volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                         kSpeechLevel, 5);
    volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                         kSpeechLevel, 5);

    EXPECT_EQ(volume_1, kInitialLevel - kClippedLevelStep);
    EXPECT_EQ(volume_2, kInitialLevel);
  }

  // Expect a change when the prediction step is used.
  WriteAlternatingAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                                     helper_1.audio_buffer);
  WriteAlternatingAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                                     helper_2.audio_buffer);
  volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                       kSpeechLevel, 5);
  volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                       kSpeechLevel, 5);

  WriteAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                          /*clipped_ratio=*/0.0f, helper_1.audio_buffer);
  WriteAudioBufferSamples(kCloseToClippingPeakRatio * kMaxSample,
                          /*clipped_ratio=*/0.0f, helper_2.audio_buffer);
  volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                       kSpeechLevel, 5);
  volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                       kSpeechLevel, 5);

  EXPECT_EQ(volume_1, kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(volume_2, kInitialLevel);

  // Expect no change when clipping is not detected or predicted.
  for (int i = 0; i < 2 * kClippedWaitFrames / 10; ++i) {
    WriteAlternatingAudioBufferSamples(/*samples_value=*/0.0f,
                                       helper_1.audio_buffer);
    WriteAlternatingAudioBufferSamples(/*samples_value=*/0.0f,
                                       helper_2.audio_buffer);
    volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                         kSpeechLevel, 5);
    volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                         kSpeechLevel, 5);

    WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.0f,
                            helper_1.audio_buffer);
    WriteAudioBufferSamples(/*samples_value=*/0.0f, /*clipped_ratio=*/0.0f,
                            helper_2.audio_buffer);
    volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                         kSpeechLevel, 5);
    volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                         kSpeechLevel, 5);
  }

  EXPECT_EQ(volume_1, kInitialLevel - 2 * kClippedLevelStep);
  EXPECT_EQ(volume_2, kInitialLevel);

  // Expect a change for clipping frames.
  WriteAlternatingAudioBufferSamples(kMaxSample, helper_1.audio_buffer);
  WriteAlternatingAudioBufferSamples(kMaxSample, helper_2.audio_buffer);
  volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                       kSpeechLevel, 1);
  volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                       kSpeechLevel, 1);

  EXPECT_EQ(volume_1, kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(volume_2, kInitialLevel - kClippedLevelStep);

  // Expect no change during waiting.
  for (int i = 0; i < kClippedWaitFrames / 10; ++i) {
    WriteAlternatingAudioBufferSamples(kMaxSample, helper_1.audio_buffer);
    WriteAlternatingAudioBufferSamples(kMaxSample, helper_2.audio_buffer);
    volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                         kSpeechLevel, 5);
    volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                         kSpeechLevel, 5);

    WriteAudioBufferSamples(kMaxSample, /*clipped_ratio=*/1.0f,
                            helper_1.audio_buffer);
    WriteAudioBufferSamples(kMaxSample, /*clipped_ratio=*/1.0f,
                            helper_2.audio_buffer);
    volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                         kSpeechLevel, 5);
    volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                         kSpeechLevel, 5);
  }

  EXPECT_EQ(volume_1, kInitialLevel - 3 * kClippedLevelStep);
  EXPECT_EQ(volume_2, kInitialLevel - kClippedLevelStep);

  // Expect a change for clipping frames.
  WriteAlternatingAudioBufferSamples(kMaxSample, helper_1.audio_buffer);
  WriteAlternatingAudioBufferSamples(kMaxSample, helper_2.audio_buffer);
  volume_1 = *helper_1.CallAgcSequence(volume_1, kLowSpeechProbability,
                                       kSpeechLevel, 1);
  volume_2 = *helper_2.CallAgcSequence(volume_2, kLowSpeechProbability,
                                       kSpeechLevel, 1);

  EXPECT_EQ(volume_1, kInitialLevel - 4 * kClippedLevelStep);
  EXPECT_EQ(volume_2, kInitialLevel - 2 * kClippedLevelStep);
}

// Checks that passing an empty speech level has no effect on the input volume.
TEST_P(InputVolumeControllerParametrizedTest, EmptyRmsErrorHasNoEffect) {
  InputVolumeController controller(kNumChannels,
                                   GetInputVolumeControllerTestConfig());
  controller.Initialize();

  // Feed speech with low energy that would trigger an upward adapation of
  // the analog level if an speech level was not low and the RMS level empty.
  constexpr int kNumFrames = 125;
  constexpr int kGainDb = -20;
  SpeechSamplesReader reader;
  int volume = reader.Feed(kNumFrames, kInitialInputVolume, kGainDb,
                           kLowSpeechProbability, absl::nullopt, controller);

  // Check that no adaptation occurs.
  ASSERT_EQ(volume, kInitialInputVolume);
}

// Checks that the recommended input volume is not updated unless enough
// frames have been processed after the previous update.
TEST(InputVolumeControllerTest, UpdateInputVolumeWaitFramesIsEffective) {
  constexpr int kInputVolume = kInitialInputVolume;
  std::unique_ptr<InputVolumeController> controller_wait_0 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/0);
  std::unique_ptr<InputVolumeController> controller_wait_100 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/100);
  controller_wait_0->Initialize();
  controller_wait_100->Initialize();

  SpeechSamplesReader reader_1;
  SpeechSamplesReader reader_2;
  int volume_wait_0 = reader_1.Feed(
      /*num_frames=*/99, kInputVolume, /*gain_db=*/0, kHighSpeechProbability,
      /*speech_level_dbfs=*/-42.0f, *controller_wait_0);
  int volume_wait_100 = reader_2.Feed(
      /*num_frames=*/99, kInputVolume, /*gain_db=*/0, kHighSpeechProbability,
      /*speech_level_dbfs=*/-42.0f, *controller_wait_100);

  // Check that adaptation only occurs if enough frames have been processed.
  ASSERT_GT(volume_wait_0, kInputVolume);
  ASSERT_EQ(volume_wait_100, kInputVolume);

  volume_wait_0 =
      reader_1.Feed(/*num_frames=*/1, volume_wait_0,
                    /*gain_db=*/0, kHighSpeechProbability,
                    /*speech_level_dbfs=*/-42.0f, *controller_wait_0);
  volume_wait_100 =
      reader_2.Feed(/*num_frames=*/1, volume_wait_100,
                    /*gain_db=*/0, kHighSpeechProbability,
                    /*speech_level_dbfs=*/-42.0f, *controller_wait_100);

  // Check that adaptation only occurs when enough frames have been processed.
  ASSERT_GT(volume_wait_0, kInputVolume);
  ASSERT_GT(volume_wait_100, kInputVolume);
}

TEST(InputVolumeControllerTest, SpeechRatioThresholdIsEffective) {
  constexpr int kInputVolume = kInitialInputVolume;
  // Create two input volume controllers with 10 frames between volume updates
  // and the minimum speech ratio of 0.8 and speech probability threshold 0.5.
  std::unique_ptr<InputVolumeController> controller_1 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  std::unique_ptr<InputVolumeController> controller_2 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  controller_1->Initialize();
  controller_2->Initialize();

  SpeechSamplesReader reader_1;
  SpeechSamplesReader reader_2;

  int volume_1 = reader_1.Feed(/*num_frames=*/1, kInputVolume, /*gain_db=*/0,
                               /*speech_probability=*/0.7f,
                               /*speech_level_dbfs=*/-42.0f, *controller_1);
  int volume_2 = reader_2.Feed(/*num_frames=*/1, kInputVolume, /*gain_db=*/0,
                               /*speech_probability=*/0.4f,
                               /*speech_level_dbfs=*/-42.0f, *controller_2);

  ASSERT_EQ(volume_1, kInputVolume);
  ASSERT_EQ(volume_2, kInputVolume);

  volume_1 = reader_1.Feed(/*num_frames=*/2, volume_1, /*gain_db=*/0,
                           /*speech_probability=*/0.4f,
                           /*speech_level_dbfs=*/-42.0f, *controller_1);
  volume_2 = reader_2.Feed(/*num_frames=*/2, volume_2, /*gain_db=*/0,
                           /*speech_probability=*/0.4f,
                           /*speech_level_dbfs=*/-42.0f, *controller_2);

  ASSERT_EQ(volume_1, kInputVolume);
  ASSERT_EQ(volume_2, kInputVolume);

  volume_1 = reader_1.Feed(
      /*num_frames=*/7, volume_1, /*gain_db=*/0,
      /*speech_probability=*/0.7f, /*speech_level_dbfs=*/-42.0f, *controller_1);
  volume_2 = reader_2.Feed(
      /*num_frames=*/7, volume_2, /*gain_db=*/0,
      /*speech_probability=*/0.7f, /*speech_level_dbfs=*/-42.0f, *controller_2);

  ASSERT_GT(volume_1, kInputVolume);
  ASSERT_EQ(volume_2, kInputVolume);
}

TEST(InputVolumeControllerTest, SpeechProbabilityThresholdIsEffective) {
  constexpr int kInputVolume = kInitialInputVolume;
  // Create two input volume controllers with the exact same settings and
  // 10 frames between volume updates.
  std::unique_ptr<InputVolumeController> controller_1 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  std::unique_ptr<InputVolumeController> controller_2 =
      CreateInputVolumeController(kClippedLevelStep, kClippedRatioThreshold,
                                  kClippedWaitFrames,
                                  /*enable_clipping_predictor=*/false,
                                  /*update_input_volume_wait_frames=*/10);
  controller_1->Initialize();
  controller_2->Initialize();

  SpeechSamplesReader reader_1;
  SpeechSamplesReader reader_2;

  // Process with two sets of inputs: Use `reader_1` to process inputs
  // that make the volume to be adjusted after enough frames have been
  // processsed and `reader_2` to process inputs that won't make the volume
  // to be adjusted.
  int volume_1 = reader_1.Feed(/*num_frames=*/1, kInputVolume, /*gain_db=*/0,
                               /*speech_probability=*/0.5f,
                               /*speech_level_dbfs=*/-42.0f, *controller_1);
  int volume_2 = reader_2.Feed(/*num_frames=*/1, kInputVolume, /*gain_db=*/0,
                               /*speech_probability=*/0.49f,
                               /*speech_level_dbfs=*/-42.0f, *controller_2);

  ASSERT_EQ(volume_1, kInputVolume);
  ASSERT_EQ(volume_2, kInputVolume);

  reader_1.Feed(/*num_frames=*/2, volume_1, /*gain_db=*/0,
                /*speech_probability=*/0.49f, /*speech_level_dbfs=*/-42.0f,
                *controller_1);
  reader_2.Feed(/*num_frames=*/2, volume_2, /*gain_db=*/0,
                /*speech_probability=*/0.49f, /*speech_level_dbfs=*/-42.0f,
                *controller_2);

  ASSERT_EQ(volume_1, kInputVolume);
  ASSERT_EQ(volume_2, kInputVolume);

  volume_1 = reader_1.Feed(
      /*num_frames=*/7, volume_1, /*gain_db=*/0,
      /*speech_probability=*/0.5f, /*speech_level_dbfs=*/-42.0f, *controller_1);
  volume_2 = reader_2.Feed(
      /*num_frames=*/7, volume_2, /*gain_db=*/0,
      /*speech_probability=*/0.5f, /*speech_level_dbfs=*/-42.0f, *controller_2);

  ASSERT_GT(volume_1, kInputVolume);
  ASSERT_EQ(volume_2, kInputVolume);
}

TEST(InputVolumeControllerTest,
     DoNotLogRecommendedInputVolumeOnChangeToMatchTarget) {
  metrics::Reset();

  SpeechSamplesReader reader;
  auto controller = CreateInputVolumeController();
  controller->Initialize();
  // Trigger a downward volume change by inputting audio that clips. Pass a
  // speech level that falls in the target range to make sure that the
  // adaptation is not made to match the target range.
  constexpr int kStartupVolume = 255;
  const int volume = reader.Feed(/*num_frames=*/14, kStartupVolume,
                                 /*gain_db=*/50, kHighSpeechProbability,
                                 /*speech_level_dbfs=*/-20.0f, *controller);
  ASSERT_LT(volume, kStartupVolume);
  EXPECT_METRIC_THAT(
      metrics::Samples(
          "WebRTC.Audio.Apm.RecommendedInputVolume.OnChangeToMatchTarget"),
      ::testing::IsEmpty());
}

TEST(InputVolumeControllerTest,
     LogRecommendedInputVolumeOnUpwardChangeToMatchTarget) {
  metrics::Reset();

  SpeechSamplesReader reader;
  auto controller = CreateInputVolumeController();
  controller->Initialize();
  constexpr int kStartupVolume = 100;
  // Trigger an upward volume change by inputting audio that does not clip and
  // by passing a speech level below the target range.
  const int volume = reader.Feed(/*num_frames=*/14, kStartupVolume,
                                 /*gain_db=*/-6, kHighSpeechProbability,
                                 /*speech_level_dbfs=*/-50.0f, *controller);
  ASSERT_GT(volume, kStartupVolume);
  EXPECT_METRIC_THAT(
      metrics::Samples(
          "WebRTC.Audio.Apm.RecommendedInputVolume.OnChangeToMatchTarget"),
      ::testing::Not(::testing::IsEmpty()));
}

TEST(InputVolumeControllerTest,
     LogRecommendedInputVolumeOnDownwardChangeToMatchTarget) {
  metrics::Reset();

  SpeechSamplesReader reader;
  auto controller = CreateInputVolumeController();
  controller->Initialize();
  constexpr int kStartupVolume = 100;
  // Trigger a downward volume change by inputting audio that does not clip and
  // by passing a speech level above the target range.
  const int volume = reader.Feed(/*num_frames=*/14, kStartupVolume,
                                 /*gain_db=*/-6, kHighSpeechProbability,
                                 /*speech_level_dbfs=*/-5.0f, *controller);
  ASSERT_LT(volume, kStartupVolume);
  EXPECT_METRIC_THAT(
      metrics::Samples(
          "WebRTC.Audio.Apm.RecommendedInputVolume.OnChangeToMatchTarget"),
      ::testing::Not(::testing::IsEmpty()));
}

TEST(MonoInputVolumeControllerTest, CheckHandleClippingLowersVolume) {
  constexpr int kInitialInputVolume = 100;
  constexpr int kInputVolumeStep = 29;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/70,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  UpdateRecommendedInputVolume(mono_controller, kInitialInputVolume,
                               kLowSpeechProbability,
                               /*rms_error_dbfs*/ -10.0f);

  mono_controller.HandleClipping(kInputVolumeStep);

  EXPECT_EQ(mono_controller.recommended_analog_level(),
            kInitialInputVolume - kInputVolumeStep);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessNegativeRmsErrorDecreasesInputVolume) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, -10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume, kInitialInputVolume);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessPositiveRmsErrorIncreasesInputVolume) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, 10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, 10.0f);
  volume = UpdateRecommendedInputVolume(mono_controller, volume,
                                        kHighSpeechProbability, 10.0f);

  EXPECT_GT(volume, kInitialInputVolume);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessNegativeRmsErrorDecreasesInputVolumeWithLimit) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_3(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2,
      /*speech_probability_threshold=*/0.7,
      /*speech_ratio_threshold=*/0.8);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();
  mono_controller_3.Initialize();

  // Process RMS errors in the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -14.0f);
  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -14.0f);
  // Process RMS errors outside the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -15.0f);
  int volume_3 = UpdateRecommendedInputVolume(
      mono_controller_3, kInitialInputVolume, kHighSpeechProbability, -30.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -15.0f);
  volume_3 = UpdateRecommendedInputVolume(mono_controller_3, volume_3,
                                          kHighSpeechProbability, -30.0f);

  EXPECT_LT(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
  EXPECT_EQ(volume_2, volume_3);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessPositiveRmsErrorIncreasesInputVolumeWithLimit) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_3(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();
  mono_controller_3.Initialize();

  // Process RMS errors in the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, 14.0f);
  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, 14.0f);
  // Process RMS errors outside the range
  // [`-kMaxResidualGainChange`, `kMaxResidualGainChange`].
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, 15.0f);
  int volume_3 = UpdateRecommendedInputVolume(
      mono_controller_3, kInitialInputVolume, kHighSpeechProbability, 30.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, 15.0f);
  volume_3 = UpdateRecommendedInputVolume(mono_controller_3, volume_3,
                                          kHighSpeechProbability, 30.0f);

  EXPECT_GT(volume_1, kInitialInputVolume);
  EXPECT_GT(volume_2, volume_1);
  EXPECT_EQ(volume_2, volume_3);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessRmsErrorDecreasesInputVolumeRepeatedly) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume_before = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  volume_before = UpdateRecommendedInputVolume(mono_controller, volume_before,
                                               kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_before, kInitialInputVolume);

  int volume_after = UpdateRecommendedInputVolume(
      mono_controller, volume_before, kHighSpeechProbability, -10.0f);
  volume_after = UpdateRecommendedInputVolume(mono_controller, volume_after,
                                              kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_after, volume_before);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessPositiveRmsErrorIncreasesInputVolumeRepeatedly) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/32,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller.Initialize();

  int volume_before = UpdateRecommendedInputVolume(
      mono_controller, kInitialInputVolume, kHighSpeechProbability, 10.0f);
  volume_before = UpdateRecommendedInputVolume(mono_controller, volume_before,
                                               kHighSpeechProbability, 10.0f);

  EXPECT_GT(volume_before, kInitialInputVolume);

  int volume_after = UpdateRecommendedInputVolume(
      mono_controller, volume_before, kHighSpeechProbability, 10.0f);
  volume_after = UpdateRecommendedInputVolume(mono_controller, volume_after,
                                              kHighSpeechProbability, 10.0f);

  EXPECT_GT(volume_after, volume_before);
}

TEST(MonoInputVolumeControllerTest, CheckClippedLevelMinIsEffective) {
  constexpr int kInitialInputVolume = 100;
  constexpr int kClippedLevelMin = 70;
  MonoInputVolumeController mono_controller_1(
      kClippedLevelMin,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      kClippedLevelMin,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  // Process one frame to reset the state for `HandleClipping()`.
  EXPECT_EQ(UpdateRecommendedInputVolume(mono_controller_1, kInitialInputVolume,
                                         kLowSpeechProbability, -10.0f),
            kInitialInputVolume);
  EXPECT_EQ(UpdateRecommendedInputVolume(mono_controller_2, kInitialInputVolume,
                                         kLowSpeechProbability, -10.0f),
            kInitialInputVolume);

  mono_controller_1.HandleClipping(29);
  mono_controller_2.HandleClipping(31);

  EXPECT_EQ(mono_controller_2.recommended_analog_level(), kClippedLevelMin);
  EXPECT_LT(mono_controller_2.recommended_analog_level(),
            mono_controller_1.recommended_analog_level());
}

TEST(MonoInputVolumeControllerTest, CheckMinMicLevelIsEffective) {
  constexpr int kInitialInputVolume = 100;
  constexpr int kMinMicLevel = 64;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64, kMinMicLevel,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64, kMinMicLevel,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -30.0f);

  EXPECT_LT(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
  EXPECT_EQ(volume_2, kMinMicLevel);
}

TEST(MonoInputVolumeControllerTest,
     CheckUpdateInputVolumeWaitFramesIsEffective) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/1, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/3, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_LT(volume_2, kInitialInputVolume);
}

TEST(MonoInputVolumeControllerTest,
     CheckSpeechProbabilityThresholdIsEffective) {
  constexpr int kInitialInputVolume = 100;
  constexpr float kSpeechProbabilityThreshold = 0.8f;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kSpeechProbabilityThreshold,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kSpeechProbabilityThreshold,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 =
      UpdateRecommendedInputVolume(mono_controller_1, kInitialInputVolume,
                                   kSpeechProbabilityThreshold, -10.0f);
  int volume_2 =
      UpdateRecommendedInputVolume(mono_controller_2, kInitialInputVolume,
                                   kSpeechProbabilityThreshold, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, volume_1, kSpeechProbabilityThreshold - 0.1f, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kSpeechProbabilityThreshold, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
}

TEST(MonoInputVolumeControllerTest, CheckSpeechRatioThresholdIsEffective) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/4, kHighSpeechProbability,
      /*speech_ratio_threshold=*/0.75f);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/4, kHighSpeechProbability,
      /*speech_ratio_threshold=*/0.75f);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kHighSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kLowSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kLowSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(mono_controller_1, volume_1,
                                          kLowSpeechProbability, -10.0f);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
}

TEST(MonoInputVolumeControllerTest,
     CheckProcessEmptyRmsErrorDoesNotLowerVolume) {
  constexpr int kInitialInputVolume = 100;
  MonoInputVolumeController mono_controller_1(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  MonoInputVolumeController mono_controller_2(
      /*clipped_level_min=*/64,
      /*min_mic_level=*/84,
      /*update_input_volume_wait_frames=*/2, kHighSpeechProbability,
      kSpeechRatioThreshold);
  mono_controller_1.Initialize();
  mono_controller_2.Initialize();

  int volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, kInitialInputVolume, kHighSpeechProbability, -10.0f);
  int volume_2 = UpdateRecommendedInputVolume(
      mono_controller_2, kInitialInputVolume, kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_EQ(volume_2, kInitialInputVolume);

  volume_1 = UpdateRecommendedInputVolume(
      mono_controller_1, volume_1, kHighSpeechProbability, absl::nullopt);
  volume_2 = UpdateRecommendedInputVolume(mono_controller_2, volume_2,
                                          kHighSpeechProbability, -10.0f);

  EXPECT_EQ(volume_1, kInitialInputVolume);
  EXPECT_LT(volume_2, volume_1);
}

}  // namespace webrtc
