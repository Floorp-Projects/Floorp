/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_LAPPED_TRANSFORM_H_
#define WEBRTC_COMMON_AUDIO_LAPPED_TRANSFORM_H_

#include <complex>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/blocker.h"
#include "webrtc/common_audio/real_fourier.h"
#include "webrtc/system_wrappers/interface/aligned_array.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

// Helper class for audio processing modules which operate on frequency domain
// input derived from the windowed time domain audio stream.
//
// The input audio chunk is sliced into possibly overlapping blocks, multiplied
// by a window and transformed with an FFT implementation. The transformed data
// is supplied to the given callback for processing. The processed output is
// then inverse transformed into the time domain and spliced back into a chunk
// which constitutes the final output of this processing module.
class LappedTransform {
 public:
  class Callback {
   public:
    virtual ~Callback() {}

    virtual void ProcessAudioBlock(const std::complex<float>* const* in_block,
                                   int in_channels, int frames,
                                   int out_channels,
                                   std::complex<float>* const* out_block) = 0;
  };

  // Construct a transform instance. |chunk_length| is the number of samples in
  // each channel. |window| defines the window, owned by the caller (a copy is
  // made internally); can be NULL to disable windowing entirely.
  // |block_length| defines the length of a block, in samples, even when
  // windowing is disabled. |shift_length| is in samples. |callback| is the
  // caller-owned audio processing function called for each block of the input
  // chunk.
  LappedTransform(int in_channels, int out_channels, int chunk_length,
                  const float* window, int block_length, int shift_amount,
                  Callback* callback);
  ~LappedTransform();

  // Main audio processing helper method. Internally slices |in_chunk| into
  // blocks, transforms them to frequency domain, calls the callback for each
  // block and returns a de-blocked time domain chunk of audio through
  // |out_chunk|. Both buffers are caller-owned.
  void ProcessChunk(const float* const* in_chunk, float* const* out_chunk);

 private:
  // Internal middleware callback, given to the blocker. Transforms each block
  // and hands it over to the processing method given at construction time.
  friend class BlockThunk;
  class BlockThunk : public BlockerCallback {
   public:
    explicit BlockThunk(LappedTransform* parent) : parent_(parent) {}
    virtual ~BlockThunk() {}

    virtual void ProcessBlock(const float* const* input, int num_frames,
                              int num_input_channels, int num_output_channels,
                              float* const* output);

   private:
    LappedTransform* parent_;
  } blocker_callback_;

  int in_channels_;
  int out_channels_;

  const float* window_;
  bool own_window_;
  int window_shift_amount_;

  int block_length_;
  int chunk_length_;
  Callback* block_processor_;
  scoped_ptr<Blocker> blocker_;

  RealFourier fft_;
  int cplx_length_;
  AlignedArray<float> real_buf_;
  AlignedArray<std::complex<float> > cplx_pre_;
  AlignedArray<std::complex<float> > cplx_post_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_LAPPED_TRANSFORM_H_

