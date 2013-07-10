/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Modified from the Chromium original here:
// src/media/base/sinc_resampler.h

#ifndef WEBRTC_COMMON_AUDIO_RESAMPLER_SINC_RESAMPLER_H_
#define WEBRTC_COMMON_AUDIO_RESAMPLER_SINC_RESAMPLER_H_

#include "webrtc/system_wrappers/interface/aligned_malloc.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/gtest_prod_util.h"

namespace webrtc {

// Callback class to provide SincResampler with input.
class SincResamplerCallback {
 public:
  virtual ~SincResamplerCallback() {}
  virtual void Run(float* destination, int frames) = 0;
};

// SincResampler is a high-quality single-channel sample-rate converter.
class SincResampler {
 public:
  // Constructs a SincResampler with the specified |read_cb|, which is used to
  // acquire audio data for resampling.  |io_sample_rate_ratio| is the ratio of
  // input / output sample rates.  If desired, the number of destination frames
  // generated per processing pass can be specified through |block_size|.
  SincResampler(double io_sample_rate_ratio,
                SincResamplerCallback* read_cb);
  SincResampler(double io_sample_rate_ratio,
                SincResamplerCallback* read_cb,
                int block_size);
  virtual ~SincResampler();

  // Resample |frames| of data from |read_cb_| into |destination|.
  void Resample(float* destination, int frames);

  // The maximum size in frames that guarantees Resample() will only make a
  // single call to |read_cb_| for more data.
  int ChunkSize();

  // The number of source frames requested per processing pass (and equal to
  // |block_size| if provided at construction).  The first pass will request
  // more to prime the buffer.
  int BlockSize();

  // Flush all buffered data and reset internal indices.
  void Flush();

 private:
  FRIEND_TEST_ALL_PREFIXES(SincResamplerTest, Convolve);
  FRIEND_TEST_ALL_PREFIXES(SincResamplerTest, ConvolveBenchmark);

  void Initialize();
  void InitializeKernel();

  // Compute convolution of |k1| and |k2| over |input_ptr|, resultant sums are
  // linearly interpolated using |kernel_interpolation_factor|.  On x86, the
  // underlying implementation is chosen at run time based on SSE support.  On
  // ARM, NEON support is chosen at compile time based on compilation flags.
  static float Convolve(const float* input_ptr, const float* k1,
                        const float* k2, double kernel_interpolation_factor);
  static float Convolve_C(const float* input_ptr, const float* k1,
                          const float* k2, double kernel_interpolation_factor);
  static float Convolve_SSE(const float* input_ptr, const float* k1,
                            const float* k2,
                            double kernel_interpolation_factor);
  static float Convolve_NEON(const float* input_ptr, const float* k1,
                             const float* k2,
                             double kernel_interpolation_factor);

  // The ratio of input / output sample rates.
  double io_sample_rate_ratio_;

  // An index on the source input buffer with sub-sample precision.  It must be
  // double precision to avoid drift.
  double virtual_source_idx_;

  // The buffer is primed once at the very beginning of processing.
  bool buffer_primed_;

  // Source of data for resampling.
  SincResamplerCallback* read_cb_;

  // See kDefaultBlockSize.
  int block_size_;

  // See kDefaultBufferSize.
  int buffer_size_;

  // Contains kKernelOffsetCount kernels back-to-back, each of size kKernelSize.
  // The kernel offsets are sub-sample shifts of a windowed sinc shifted from
  // 0.0 to 1.0 sample.
  scoped_ptr_malloc<float, AlignedFree> kernel_storage_;

  // Data from the source is copied into this buffer for each processing pass.
  scoped_ptr_malloc<float, AlignedFree> input_buffer_;

  // Pointers to the various regions inside |input_buffer_|.  See the diagram at
  // the top of the .cc file for more information.
  float* const r0_;
  float* const r1_;
  float* const r2_;
  float* const r3_;
  float* const r4_;
  float* const r5_;

  DISALLOW_COPY_AND_ASSIGN(SincResampler);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_RESAMPLER_SINC_RESAMPLER_H_
