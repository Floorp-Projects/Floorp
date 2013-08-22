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

#include "signal_processing_library.h"
#include "resampler.h"

// TODO(jesup) better adjust per platform ability
// Note: if these are changed (higher), you may need to change the
// KernelDelay values in the unit tests here and in output_mixer.
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_GONK)
#define RESAMPLER_QUALITY 2
#else
#define RESAMPLER_QUALITY 3
#endif

namespace webrtc
{

Resampler::Resampler() : state_(NULL), type_(kResamplerSynchronous)
{
  // Note: Push will fail until Reset() is called
}

Resampler::Resampler(int in_freq, int out_freq, ResamplerType type) :
  state_(NULL) // all others get initialized in reset
{
  Reset(in_freq, out_freq, type);
}

Resampler::~Resampler()
{
  if (state_)
  {
    speex_resampler_destroy(state_);
  }
}

int Resampler::ResetIfNeeded(int in_freq, int out_freq, ResamplerType type)
{
  if (!state_ || type != type_ ||
      in_freq != in_freq_ || out_freq != out_freq_)
  {
    // Note that fixed-rate resamplers where input == output rate will
    // have state_ == NULL, and will call Reset() here - but reset won't
    // do anything beyond overwrite the member vars unless it needs a
    // real resampler.
    return Reset(in_freq, out_freq, type);
  } else {
    return 0;
  }
}

int Resampler::Reset(int in_freq, int out_freq, ResamplerType type)
{
  uint32_t channels = (type == kResamplerSynchronousStereo ||
                       type == kResamplerFixedSynchronousStereo) ? 2 : 1;

  if (state_)
  {
    speex_resampler_destroy(state_);
    state_ = NULL;
  }
  type_ = type;
  channels_ = channels;
  in_freq_ = in_freq;
  out_freq_ = out_freq;

  // For fixed-rate, same-rate resamples we just memcpy and so don't spin up a resampler
  if (in_freq != out_freq || !IsFixedRate())
  {
    state_ = speex_resampler_init(channels, in_freq, out_freq, RESAMPLER_QUALITY, NULL);
    if (!state_)
    {
      return -1;
    }
  }
  return 0;
}

// Synchronous resampling, all output samples are written to samples_out
// TODO(jesup) Change to take samples-per-channel in and out
int Resampler::Push(const int16_t* samples_in, int length_in,
                    int16_t* samples_out, int max_len, int &out_len)
{
  if (max_len < length_in)
  {
    return -1;
  }
  if (!state_)
  {
    if (!IsFixedRate() || in_freq_ != out_freq_)
    {
      // Since we initialize to a non-Fixed type, Push() will fail
      // until Reset() is called
      return -1;
    }
    // Fixed-rate, same-freq "resample" - use memcpy, which avoids
    // filtering and delay.  For non-fixed rates, where we might tweak
    // from 48000->48000 to 48000->48001 for drift, we need to resample
    // (and filter) all the time to avoid glitches on rate changes.
    memcpy(samples_out, samples_in, length_in*sizeof(*samples_in));
    out_len = length_in;
    return 0;
  }
  assert(channels_ == 1 || channels_ == 2);
  spx_uint32_t len = length_in = (length_in >> (channels_ - 1));
  spx_uint32_t out = (spx_uint32_t) (max_len >> (channels_ - 1));
  if ((speex_resampler_process_interleaved_int(state_, samples_in, &len,
                             samples_out, &out) != RESAMPLER_ERR_SUCCESS) ||
      len != (spx_uint32_t) length_in)
  {
    return -1;
  }
  out_len = (int) (channels_ * out);
  return 0;
}

} // namespace webrtc
