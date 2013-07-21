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

#include "typedefs.h"
#include "speex/speex_resampler.h"

namespace webrtc
{

#define FIXED_RATE_RESAMPLER 0x10
enum ResamplerType
{
    kResamplerSynchronous            = 0x00,
    kResamplerSynchronousStereo      = 0x01,
    kResamplerFixedSynchronous       = 0x00 | FIXED_RATE_RESAMPLER,
    kResamplerFixedSynchronousStereo = 0x01 | FIXED_RATE_RESAMPLER,
};

class Resampler
{
public:
    Resampler();
    // TODO(andrew): use an init function instead.
    Resampler(int in_freq, int out_freq, ResamplerType type);
    ~Resampler();

    // Reset all states
    int Reset(int in_freq, int out_freq, ResamplerType type);

    // Reset all states if any parameter has changed
    int ResetIfNeeded(int in_freq, int out_freq, ResamplerType type);

    int Push(const int16_t* samples_in, int length_in,
             int16_t* samples_out, int max_len, int &out_len);

private:
    bool IsFixedRate() { return !!(type_ & FIXED_RATE_RESAMPLER); }

    SpeexResamplerState* state_;

    // State
    int in_freq_;
    int out_freq_;
    int channels_;
    ResamplerType type_;
};

} // namespace webrtc

#endif // WEBRTC_RESAMPLER_RESAMPLER_H_
