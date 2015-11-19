/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * A wrapper for resampling a numerous amount of sampling combinations.
 */

#ifndef WEBRTC_RESAMPLER_RESAMPLER_H_
#define WEBRTC_RESAMPLER_RESAMPLER_H_

#include "webrtc/typedefs.h"
#include <speex/speex_resampler.h>

namespace webrtc {

#define FIXED_RATE_RESAMPLER 0x10

// All methods return 0 on success and -1 on failure.
class Resampler
{

public:
    Resampler();
    Resampler(int inFreq, int outFreq, int num_channels);
    ~Resampler();

    // Reset all states
    int Reset(int inFreq, int outFreq, int num_channels);

    // Reset all states if any parameter has changed
    int ResetIfNeeded(int inFreq, int outFreq, int num_channels);

    // Resample samplesIn to samplesOut.
    int Push(const int16_t* samplesIn, int lengthIn, int16_t* samplesOut,
             int maxLen, int &outLen);

private:
    SpeexResamplerState* state_;

    int in_freq_;
    int out_freq_;
    int channels_;
};

}  // namespace webrtc

#endif // WEBRTC_RESAMPLER_RESAMPLER_H_
