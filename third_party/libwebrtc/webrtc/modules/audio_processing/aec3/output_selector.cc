/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/output_selector.h"

#include <algorithm>
#include <numeric>

#include "rtc_base/checks.h"

namespace webrtc {
namespace {

// Performs the transition between the signals in a smooth manner.
void SmoothFrameTransition(bool from_y_to_e,
                           rtc::ArrayView<const float> e,
                           rtc::ArrayView<float> y) {
  RTC_DCHECK_LT(0u, e.size());
  RTC_DCHECK_EQ(y.size(), e.size());

  const float change_factor = (from_y_to_e ? 1.f : -1.f) / e.size();
  float averaging = from_y_to_e ? 0.f : 1.f;
  for (size_t k = 0; k < e.size(); ++k) {
    y[k] += averaging * (e[k] - y[k]);
    averaging += change_factor;
  }
  RTC_DCHECK_EQ(from_y_to_e ? 1.f : 0.f, averaging);
}

}  // namespace

OutputSelector::OutputSelector() = default;

OutputSelector::~OutputSelector() = default;

void OutputSelector::FormLinearOutput(
    bool use_subtractor_output,
    rtc::ArrayView<const float> subtractor_output,
    rtc::ArrayView<float> capture) {
  RTC_DCHECK_EQ(subtractor_output.size(), capture.size());
  rtc::ArrayView<const float>& e_main = subtractor_output;
  rtc::ArrayView<float> y = capture;

  if (use_subtractor_output != use_subtractor_output_) {
    use_subtractor_output_ = use_subtractor_output;
    SmoothFrameTransition(use_subtractor_output_, e_main, y);
  } else if (use_subtractor_output_) {
    std::copy(e_main.begin(), e_main.end(), y.begin());
  }
}

}  // namespace webrtc
