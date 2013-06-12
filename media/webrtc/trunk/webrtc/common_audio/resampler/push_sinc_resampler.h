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
  PushSincResampler(int src_block_size, int dst_block_size);
  virtual ~PushSincResampler();

  // Perform the resampling. |source_length| must always equal the
  // |src_block_size| provided at construction. |destination_capacity| must be
  // at least as large as |dst_block_size|. Returns the number of samples
  // provided in destination (for convenience, since this will always be equal
  // to |dst_block_size|).
  int Resample(const int16_t* source, int source_length,
               int16_t* destination, int destination_capacity);

  // Implements SincResamplerCallback.
  virtual void Run(float* destination, int frames);

 private:
  scoped_ptr<SincResampler> resampler_;
  scoped_array<float> float_buffer_;
  const int16_t* source_ptr_;
  const int dst_size_;

  DISALLOW_COPY_AND_ASSIGN(PushSincResampler);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_RESAMPLER_PUSH_SINC_RESAMPLER_H_
