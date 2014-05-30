/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_RESAMPLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_RESAMPLER_H_

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;

namespace acm2 {

class ACMResampler {
 public:
  ACMResampler();
  ~ACMResampler();

  int Resample10Msec(const int16_t* in_audio,
                     int in_freq_hz,
                     int out_freq_hz,
                     int num_audio_channels,
                     int16_t* out_audio);

 private:
  // Use the Resampler class.
  Resampler resampler_;
  CriticalSectionWrapper* resampler_crit_sect_;
};

}  // namespace acm2

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_RESAMPLER_H_
