/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_RESAMPLER_PUSH_SINC_RESAMPLER_H_
#define WEBRTC_COMMON_AUDIO_RESAMPLER_PUSH_SINC_RESAMPLER_H_

#include "webrtc/common_audio/resampler/sinc_resampler.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// A thin wrapper over SincResampler to provide a push-based interface as
// required by WebRTC.
class PushSincResampler : public SincResamplerCallback {
 public:
  // Provide the size of the source and destination blocks in samples. These
  // must correspond to the same time duration (typically 10 ms) as the sample
  // ratio is inferred from them.
  PushSincResampler(int source_frames, int destination_frames);
  virtual ~PushSincResampler();

  // Perform the resampling. |source_frames| must always equal the
  // |source_frames| provided at construction. |destination_capacity| must be
  // at least as large as |destination_frames|. Returns the number of samples
  // provided in destination (for convenience, since this will always be equal
  // to |destination_frames|).
  int Resample(const int16_t* source, int source_frames,
               int16_t* destination, int destination_capacity);

  // Implements SincResamplerCallback.
  virtual void Run(int frames, float* destination) OVERRIDE;

  SincResampler* get_resampler_for_testing() { return resampler_.get(); }

 private:
  scoped_ptr<SincResampler> resampler_;
  scoped_array<float> float_buffer_;
  const int16_t* source_ptr_;
  const int destination_frames_;

  // True on the first call to Resample(), to prime the SincResampler buffer.
  bool first_pass_;

  // Used to assert we are only requested for as much data as is available.
  int source_available_;

  DISALLOW_COPY_AND_ASSIGN(PushSincResampler);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_RESAMPLER_PUSH_SINC_RESAMPLER_H_
