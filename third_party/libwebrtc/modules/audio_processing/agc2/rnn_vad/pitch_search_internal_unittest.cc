/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"

#include <array>
#include <tuple>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr int kTestPitchPeriodsLow = 3 * kMinPitch48kHz / 2;
constexpr int kTestPitchPeriodsHigh = (3 * kMinPitch48kHz + kMaxPitch48kHz) / 2;

constexpr float kTestPitchGainsLow = 0.35f;
constexpr float kTestPitchGainsHigh = 0.75f;

}  // namespace

// Checks that the frame-wise sliding square energy function produces output
// within tolerance given test input data.
TEST(RnnVadTest, ComputeSlidingFrameSquareEnergies24kHzWithinTolerance) {
  PitchTestData test_data;
  std::array<float, kNumPitchBufSquareEnergies> computed_output;
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         computed_output);
  auto square_energies_view = test_data.GetPitchBufSquareEnergiesView();
  ExpectNearAbsolute({square_energies_view.data(), square_energies_view.size()},
                     computed_output, 1e-3f);
}

// Checks that the estimated pitch period is bit-exact given test input data.
TEST(RnnVadTest, ComputePitchPeriod12kHzBitExactness) {
  PitchTestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.GetPitchBufView(), pitch_buf_decimated);
  CandidatePitchPeriods pitch_candidates;
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  auto auto_corr_view = test_data.GetPitchBufAutoCorrCoeffsView();
  pitch_candidates =
      ComputePitchPeriod12kHz(pitch_buf_decimated, auto_corr_view);
  EXPECT_EQ(pitch_candidates.best, 140);
  EXPECT_EQ(pitch_candidates.second_best, 142);
}

// Checks that the refined pitch period is bit-exact given test input data.
TEST(RnnVadTest, ComputePitchPeriod48kHzBitExactness) {
  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         y_energy_view);
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  EXPECT_EQ(ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                                    /*pitch_candidates=*/{280, 284}),
            560);
  EXPECT_EQ(ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                                    /*pitch_candidates=*/{260, 284}),
            568);
}

class PitchCandidatesParametrization
    : public ::testing::TestWithParam<CandidatePitchPeriods> {
 protected:
  CandidatePitchPeriods GetPitchCandidates() const { return GetParam(); }
  CandidatePitchPeriods GetSwappedPitchCandidates() const {
    CandidatePitchPeriods candidate = GetParam();
    return {candidate.second_best, candidate.best};
  }
};

// Checks that the result of `ComputePitchPeriod48kHz()` does not depend on the
// order of the input pitch candidates.
TEST_P(PitchCandidatesParametrization,
       ComputePitchPeriod48kHzOrderDoesNotMatter) {
  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         y_energy_view);
  EXPECT_EQ(ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                                    GetPitchCandidates()),
            ComputePitchPeriod48kHz(test_data.GetPitchBufView(), y_energy_view,
                                    GetSwappedPitchCandidates()));
}

INSTANTIATE_TEST_SUITE_P(RnnVadTest,
                         PitchCandidatesParametrization,
                         ::testing::Values(CandidatePitchPeriods{0, 2},
                                           CandidatePitchPeriods{260, 284},
                                           CandidatePitchPeriods{280, 284},
                                           CandidatePitchPeriods{
                                               kInitialNumLags24kHz - 2,
                                               kInitialNumLags24kHz - 1}));

class ExtendedPitchPeriodSearchParametrizaion
    : public ::testing::TestWithParam<std::tuple<int, int, float, int, float>> {
 protected:
  int GetInitialPitchPeriod() const { return std::get<0>(GetParam()); }
  int GetLastPitchPeriod() const { return std::get<1>(GetParam()); }
  float GetLastPitchStrength() const { return std::get<2>(GetParam()); }
  int GetExpectedPitchPeriod() const { return std::get<3>(GetParam()); }
  float GetExpectedPitchStrength() const { return std::get<4>(GetParam()); }
};

// Checks that the computed pitch period is bit-exact and that the computed
// pitch strength is within tolerance given test input data.
TEST_P(ExtendedPitchPeriodSearchParametrizaion,
       PeriodBitExactnessGainWithinTolerance) {
  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.GetPitchBufView(),
                                         y_energy_view);
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  const auto computed_output = ComputeExtendedPitchPeriod48kHz(
      test_data.GetPitchBufView(), y_energy_view, GetInitialPitchPeriod(),
      {GetLastPitchPeriod(), GetLastPitchStrength()});
  EXPECT_EQ(GetExpectedPitchPeriod(), computed_output.period);
  EXPECT_NEAR(GetExpectedPitchStrength(), computed_output.strength, 1e-6f);
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    ExtendedPitchPeriodSearchParametrizaion,
    ::testing::Values(std::make_tuple(kTestPitchPeriodsLow,
                                      kTestPitchPeriodsLow,
                                      kTestPitchGainsLow,
                                      91,
                                      -0.0188608f),
                      std::make_tuple(kTestPitchPeriodsLow,
                                      kTestPitchPeriodsLow,
                                      kTestPitchGainsHigh,
                                      91,
                                      -0.0188608f),
                      std::make_tuple(kTestPitchPeriodsLow,
                                      kTestPitchPeriodsHigh,
                                      kTestPitchGainsLow,
                                      91,
                                      -0.0188608f),
                      std::make_tuple(kTestPitchPeriodsLow,
                                      kTestPitchPeriodsHigh,
                                      kTestPitchGainsHigh,
                                      91,
                                      -0.0188608f),
                      std::make_tuple(kTestPitchPeriodsHigh,
                                      kTestPitchPeriodsLow,
                                      kTestPitchGainsLow,
                                      475,
                                      -0.0904344f),
                      std::make_tuple(kTestPitchPeriodsHigh,
                                      kTestPitchPeriodsLow,
                                      kTestPitchGainsHigh,
                                      475,
                                      -0.0904344f),
                      std::make_tuple(kTestPitchPeriodsHigh,
                                      kTestPitchPeriodsHigh,
                                      kTestPitchGainsLow,
                                      475,
                                      -0.0904344f),
                      std::make_tuple(kTestPitchPeriodsHigh,
                                      kTestPitchPeriodsHigh,
                                      kTestPitchGainsHigh,
                                      475,
                                      -0.0904344f)));

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
