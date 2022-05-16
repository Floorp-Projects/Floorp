/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/vad_with_level.h"

#include <limits>
#include <memory>
#include <vector>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "rtc_base/gunit.h"
#include "rtc_base/numerics/safe_compare.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

using ::testing::AnyNumber;
using ::testing::ReturnRoundRobin;

constexpr int kNoVadPeriodicReset =
    kFrameDurationMs * (std::numeric_limits<int>::max() / kFrameDurationMs);

constexpr float kInstantAttack = 1.0f;
constexpr float kSlowAttack = 0.1f;

constexpr int kSampleRateHz = 8000;

class MockVad : public VadLevelAnalyzer::VoiceActivityDetector {
 public:
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(float,
              ComputeProbability,
              (AudioFrameView<const float> frame),
              (override));
};

// Creates a `VadLevelAnalyzer` injecting a mock VAD which repeatedly returns
// the next value from `speech_probabilities` until it reaches the end and will
// restart from the beginning.
std::unique_ptr<VadLevelAnalyzer> CreateVadLevelAnalyzerWithMockVad(
    int vad_reset_period_ms,
    float vad_probability_attack,
    const std::vector<float>& speech_probabilities,
    int expected_vad_reset_calls = 0) {
  auto vad = std::make_unique<MockVad>();
  EXPECT_CALL(*vad, ComputeProbability)
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRoundRobin(speech_probabilities));
  if (expected_vad_reset_calls >= 0) {
    EXPECT_CALL(*vad, Reset).Times(expected_vad_reset_calls);
  }
  return std::make_unique<VadLevelAnalyzer>(
      vad_reset_period_ms, vad_probability_attack, std::move(vad));
}

// 10 ms mono frame.
struct FrameWithView {
  // Ctor. Initializes the frame samples with `value`.
  FrameWithView(float value = 0.0f)
      : channel0(samples.data()),
        view(&channel0, /*num_channels=*/1, samples.size()) {
    samples.fill(value);
  }
  std::array<float, kSampleRateHz / 100> samples;
  const float* const channel0;
  const AudioFrameView<const float> view;
};

TEST(AutomaticGainController2VadLevelAnalyzer, PeakLevelGreaterThanRmsLevel) {
  // Handcrafted frame so that the average is lower than the peak value.
  FrameWithView frame(1000.0f);  // Constant frame.
  frame.samples[10] = 2000.0f;   // Except for one peak value.

  // Compute audio frame levels (the VAD result is ignored).
  VadLevelAnalyzer analyzer;
  auto levels_and_vad_prob = analyzer.AnalyzeFrame(frame.view);

  // Compare peak and RMS levels.
  EXPECT_LT(levels_and_vad_prob.rms_dbfs, levels_and_vad_prob.peak_dbfs);
}

// Checks that the unprocessed and the smoothed speech probabilities match when
// instant attack is used.
TEST(AutomaticGainController2VadLevelAnalyzer, NoSpeechProbabilitySmoothing) {
  const std::vector<float> speech_probabilities{0.709f, 0.484f, 0.882f, 0.167f,
                                                0.44f,  0.525f, 0.858f, 0.314f,
                                                0.653f, 0.965f, 0.413f, 0.0f};
  auto analyzer = CreateVadLevelAnalyzerWithMockVad(
      kNoVadPeriodicReset, kInstantAttack, speech_probabilities);
  FrameWithView frame;
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size()); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(speech_probabilities[i],
              analyzer->AnalyzeFrame(frame.view).speech_probability);
  }
}

// Checks that the smoothed speech probability does not instantly converge to
// the unprocessed one when slow attack is used.
TEST(AutomaticGainController2VadLevelAnalyzer,
     SlowAttackSpeechProbabilitySmoothing) {
  const std::vector<float> speech_probabilities{0.0f, 0.0f, 1.0f,
                                                1.0f, 1.0f, 1.0f};
  auto analyzer = CreateVadLevelAnalyzerWithMockVad(
      kNoVadPeriodicReset, kSlowAttack, speech_probabilities);
  FrameWithView frame;
  float prev_probability = 0.0f;
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size()); ++i) {
    SCOPED_TRACE(i);
    const float smoothed_probability =
        analyzer->AnalyzeFrame(frame.view).speech_probability;
    EXPECT_LT(smoothed_probability, 1.0f);  // Not enough time to reach 1.
    EXPECT_LE(prev_probability, smoothed_probability);  // Converge towards 1.
    prev_probability = smoothed_probability;
  }
}

// Checks that the smoothed speech probability instantly decays to the
// unprocessed one when slow attack is used.
TEST(AutomaticGainController2VadLevelAnalyzer, SpeechProbabilityInstantDecay) {
  const std::vector<float> speech_probabilities{1.0f, 1.0f, 1.0f,
                                                1.0f, 1.0f, 0.0f};
  auto analyzer = CreateVadLevelAnalyzerWithMockVad(
      kNoVadPeriodicReset, kSlowAttack, speech_probabilities);
  FrameWithView frame;
  for (int i = 0; rtc::SafeLt(i, speech_probabilities.size() - 1); ++i) {
    analyzer->AnalyzeFrame(frame.view);
  }
  EXPECT_EQ(0.0f, analyzer->AnalyzeFrame(frame.view).speech_probability);
}

// Checks that the VAD is not periodically reset.
TEST(AutomaticGainController2VadLevelAnalyzer, VadNoPeriodicReset) {
  constexpr int kNumFrames = 19;
  auto analyzer = CreateVadLevelAnalyzerWithMockVad(
      kNoVadPeriodicReset, kSlowAttack, /*speech_probabilities=*/{1.0f},
      /*expected_vad_reset_calls=*/0);
  FrameWithView frame;
  for (int i = 0; i < kNumFrames; ++i) {
    analyzer->AnalyzeFrame(frame.view);
  }
}

class VadPeriodResetParametrization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  int num_frames() const { return std::get<0>(GetParam()); }
  int vad_reset_period_frames() const { return std::get<1>(GetParam()); }
};

// Checks that the VAD is periodically reset with the expected period.
TEST_P(VadPeriodResetParametrization, VadPeriodicReset) {
  auto analyzer = CreateVadLevelAnalyzerWithMockVad(
      /*vad_reset_period_ms=*/vad_reset_period_frames() * kFrameDurationMs,
      kSlowAttack, /*speech_probabilities=*/{1.0f},
      /*expected_vad_reset_calls=*/num_frames() / vad_reset_period_frames());
  FrameWithView frame;
  for (int i = 0; i < num_frames(); ++i) {
    analyzer->AnalyzeFrame(frame.view);
  }
}

INSTANTIATE_TEST_SUITE_P(AutomaticGainController2VadLevelAnalyzer,
                         VadPeriodResetParametrization,
                         ::testing::Combine(::testing::Values(1, 19, 123),
                                            ::testing::Values(2, 5, 20, 53)));

}  // namespace
}  // namespace webrtc
