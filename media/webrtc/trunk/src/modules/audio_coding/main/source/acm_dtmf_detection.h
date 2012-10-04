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

#include "acm_resampler.h"
#include "audio_coding_module_typedefs.h"
#include "typedefs.h"

namespace webrtc {

class ACMDTMFDetection {
 public:
  ACMDTMFDetection();
  ~ACMDTMFDetection();
  WebRtc_Word16 Enable(ACMCountries cpt = ACMDisableCountryDetection);
  WebRtc_Word16 Disable();
  WebRtc_Word16 Detect(const WebRtc_Word16* inAudioBuff,
                       const WebRtc_UWord16 inBuffLenWord16,
                       const WebRtc_Word32 inFreqHz,
                       bool& toneDetected,
                       WebRtc_Word16& tone);

 private:
  ACMResampler _resampler;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_DTMF_DETECTION_H_
