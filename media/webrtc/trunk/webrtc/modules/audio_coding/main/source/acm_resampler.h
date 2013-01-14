/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_RESAMPLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_RESAMPLER_H_

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;

class ACMResampler {
 public:
  ACMResampler();
  ~ACMResampler();

  WebRtc_Word16 Resample10Msec(const WebRtc_Word16* in_audio,
                               const WebRtc_Word32 in_freq_hz,
                               WebRtc_Word16* out_audio,
                               const WebRtc_Word32 out_freq_hz,
                               WebRtc_UWord8 num_audio_channels);

 private:
  // Use the Resampler class.
  Resampler resampler_;
  CriticalSectionWrapper* resampler_crit_sect_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_RESAMPLER_H_
