/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/auto_correlation.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {
namespace rnn_vad {

// Pitch estimator.
class PitchEstimator {
 public:
  PitchEstimator();
  PitchEstimator(const PitchEstimator&) = delete;
  PitchEstimator& operator=(const PitchEstimator&) = delete;
  ~PitchEstimator();
  // Returns the estimated pitch period at 48 kHz.
  int Estimate(rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer);

 private:
  FRIEND_TEST_ALL_PREFIXES(RnnVadTest, PitchSearchWithinTolerance);
  float GetLastPitchStrengthForTesting() const {
    return last_pitch_48kHz_.strength;
  }

  PitchInfo last_pitch_48kHz_{};
  AutoCorrelationCalculator auto_corr_calculator_;
  std::vector<float> pitch_buf_decimated_;
  rtc::ArrayView<float, kBufSize12kHz> pitch_buf_decimated_view_;
  std::vector<float> auto_corr_;
  rtc::ArrayView<float, kNumLags12kHz> auto_corr_view_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_H_
