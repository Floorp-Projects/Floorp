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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

// TODO(jesup) better adjust per platform ability
// Note: if these are changed (higher), you may need to change the
// KernelDelay values in the unit tests here and in output_mixer.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_GONK)
#define RESAMPLER_QUALITY 2
#else
#define RESAMPLER_QUALITY 3
#endif

namespace webrtc {

Resampler::Resampler() : state_(NULL), channels_(0)
{
  // Note: Push will fail until Reset() is called
}

Resampler::Resampler(int inFreq, int outFreq, size_t num_channels)
    : Resampler() {
  Reset(inFreq, outFreq, num_channels);
}

Resampler::~Resampler()
{
  if (state_)
  {
    speex_resampler_destroy(state_);
  }
}

int Resampler::ResetIfNeeded(int inFreq, int outFreq, size_t num_channels)
{
  if (!state_ || channels_ != num_channels ||
      inFreq != in_freq_ || outFreq != out_freq_)
  {
    // Note that fixed-rate resamplers where input == output rate will
    // have state_ == NULL, and will call Reset() here - but reset won't
    // do anything beyond overwrite the member vars unless it needs a
    // real resampler.
    return Reset(inFreq, outFreq, num_channels);
  } else {
    return 0;
  }
}

int Resampler::Reset(int inFreq, int outFreq, size_t num_channels)
{
  if (num_channels != 1 && num_channels != 2) {
    return -1;
  }

  if (state_)
  {
    speex_resampler_destroy(state_);
    state_ = NULL;
  }
  channels_ = num_channels;
  in_freq_ = inFreq;
  out_freq_ = outFreq;

  // For fixed-rate, same-rate resamples we just memcpy and so don't spin up a resampler
  if (inFreq != outFreq)
  {
    state_ = speex_resampler_init(num_channels, inFreq, outFreq, RESAMPLER_QUALITY, NULL);
    if (!state_)
    {
      return -1;
    }
  }
  return 0;
}

// Synchronous resampling, all output samples are written to samplesOut
// TODO(jesup) Change to take samples-per-channel in and out
int Resampler::Push(const int16_t* samplesIn, size_t lengthIn, int16_t* samplesOut,
                    size_t maxLen, size_t &outLen)
{
  if (maxLen < lengthIn)
  {
    return -1;
  }
  if (!state_)
  {
    if (in_freq_ != out_freq_ || channels_ == 0)
    {
      // Push() will fail until Reset() is called
      return -1;
    }
    // Same-freq "resample" - use memcpy, which avoids
    // filtering and delay.  For non-fixed rates, where we might tweak
    // from 48000->48000 to 48000->48001 for drift, we need to resample
    // (and filter) all the time to avoid glitches on rate changes.
    memcpy(samplesOut, samplesIn, lengthIn*sizeof(*samplesIn));
    outLen = lengthIn;
    return 0;
  }
  assert(channels_ == 1 || channels_ == 2);
  spx_uint32_t len = lengthIn = (lengthIn >> (channels_ - 1));
  spx_uint32_t out = (spx_uint32_t) (maxLen >> (channels_ - 1));
  if ((speex_resampler_process_interleaved_int(state_, samplesIn, &len,
                             samplesOut, &out) != RESAMPLER_ERR_SUCCESS) ||
      len != (spx_uint32_t) lengthIn)
  {
    return -1;
  }
  outLen = (int) (channels_ * out);
  return 0;
}
}// namespace webrtc
