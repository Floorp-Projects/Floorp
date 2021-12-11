/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_STATISTICS_H_
#define MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_STATISTICS_H_

#include "api/optional.h"

namespace webrtc {
// This version of the stats uses Optionals, it will replace the regular
// AudioProcessingStatistics struct.
struct AudioProcessingStats {
  AudioProcessingStats();
  AudioProcessingStats(const AudioProcessingStats& other);
  ~AudioProcessingStats();

  // AEC Statistics.
  // ERL = 10log_10(P_far / P_echo)
  rtc::Optional<double> echo_return_loss;
  // ERLE = 10log_10(P_echo / P_out)
  rtc::Optional<double> echo_return_loss_enhancement;
  // Fraction of time that the AEC linear filter is divergent, in a 1-second
  // non-overlapped aggregation window.
  rtc::Optional<double> divergent_filter_fraction;

  // The delay metrics consists of the delay median and standard deviation. It
  // also consists of the fraction of delay estimates that can make the echo
  // cancellation perform poorly. The values are aggregated until the first
  // call to |GetStatistics()| and afterwards aggregated and updated every
  // second. Note that if there are several clients pulling metrics from
  // |GetStatistics()| during a session the first call from any of them will
  // change to one second aggregation window for all.
  rtc::Optional<int32_t> delay_median_ms;
  rtc::Optional<int32_t> delay_standard_deviation_ms;

  // Residual echo detector likelihood.
  rtc::Optional<double> residual_echo_likelihood;
  // Maximum residual echo likelihood from the last time period.
  rtc::Optional<double> residual_echo_likelihood_recent_max;

  // The instantaneous delay estimate produced in the AEC. The unit is in
  // milliseconds and the value is the instantaneous value at the time of the
  // call to |GetStatistics()|.
  int delay_ms;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INCLUDE_AUDIO_PROCESSING_STATISTICS_H_
