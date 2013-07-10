/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_DTMF_DETECTION_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_DTMF_DETECTION_H_

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/source/acm_resampler.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ACMDTMFDetection {
 public:
  ACMDTMFDetection();
  ~ACMDTMFDetection();
  int16_t Enable(ACMCountries cpt = ACMDisableCountryDetection);
  int16_t Disable();
  int16_t Detect(const int16_t* in_audio_buff,
                 const uint16_t in_buff_len_word16,
                 const int32_t in_freq_hz,
                 bool& tone_detected,
                 int16_t& tone);

 private:
  ACMResampler resampler_;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_DTMF_DETECTION_H_
