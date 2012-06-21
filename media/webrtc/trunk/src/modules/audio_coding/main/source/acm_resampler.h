/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_RESAMPLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_RESAMPLER_H_

#include "resampler.h"
#include "typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;

class ACMResampler
{
public:
    ACMResampler();
    ~ACMResampler();

    WebRtc_Word16 Resample10Msec(
        const WebRtc_Word16* inAudio,
        const WebRtc_Word32  inFreqHz,
        WebRtc_Word16*       outAudio,
        const WebRtc_Word32  outFreqHz,
        WebRtc_UWord8        numAudioChannels);

private:

    //Use the Resampler class
    Resampler               _resampler;
    CriticalSectionWrapper& _resamplerCritSect;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_RESAMPLER_H_
