/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_OUTPUT_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_OUTPUT_H_

#include <array>

#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/fft_data.h"

namespace webrtc {

// Stores the values being returned from the echo subtractor.
struct SubtractorOutput {
  std::array<float, kBlockSize> s_main;
  std::array<float, kBlockSize> e_main;
  std::array<float, kBlockSize> e_shadow;
  FftData E_main;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;

  void Reset() {
    s_main.fill(0.f);
    e_main.fill(0.f);
    e_shadow.fill(0.f);
    E_main.re.fill(0.f);
    E_main.im.fill(0.f);
    E2_main.fill(0.f);
    E2_shadow.fill(0.f);
  }
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUBTRACTOR_OUTPUT_H_
