/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_dtmf_detection.h"

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"

namespace webrtc {

ACMDTMFDetection::ACMDTMFDetection() {}

ACMDTMFDetection::~ACMDTMFDetection() {}

int16_t ACMDTMFDetection::Enable(ACMCountries /* cpt */) {
  return -1;
}

int16_t ACMDTMFDetection::Disable() {
  return -1;
}

int16_t ACMDTMFDetection::Detect(
    const int16_t* /* in_audio_buff */,
    const uint16_t /* in_buff_len_word16 */,
    const int32_t /* in_freq_hz */,
    bool& /* tone_detected */,
    int16_t& /* tone  */) {
  return -1;
}

}  // namespace webrtc
