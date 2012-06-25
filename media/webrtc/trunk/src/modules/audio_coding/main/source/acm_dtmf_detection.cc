/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_dtmf_detection.h"
#include "audio_coding_module_typedefs.h"

namespace webrtc {

ACMDTMFDetection::ACMDTMFDetection()
    : _init(0) {}

ACMDTMFDetection::~ACMDTMFDetection() {}

WebRtc_Word16 ACMDTMFDetection::Enable(ACMCountries /* cpt */) {
  return -1;
}

WebRtc_Word16 ACMDTMFDetection::Disable() {
  return -1;
}

WebRtc_Word16 ACMDTMFDetection::Detect(
    const WebRtc_Word16* /* inAudioBuff */,
    const WebRtc_UWord16 /* inBuffLenWord16 */,
    const WebRtc_Word32 /* inFreqHz */,
    bool& /* toneDetected */,
    WebRtc_Word16& /* tone  */) {
  return -1;
}

WebRtc_Word16 ACMDTMFDetection::GetVersion(
    WebRtc_Word8* /* version */,
    WebRtc_UWord32& /* remainingBufferInBytes */,
    WebRtc_UWord32& /* position */) {
  return -1;
}

} // namespace webrtc
