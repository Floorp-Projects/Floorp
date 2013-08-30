/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Modified from the Chromium original:
// src/media/base/sinc_resampler.cc

// Input buffer layout, dividing the total buffer into regions (r0_ - r5_):
//
// |----------------|-----------------------------------------|----------------|
//
//                                   kBlockSize + kKernelSize / 2
//                   <--------------------------------------------------------->
//                                              r0_
//
//  kKernelSize / 2   kKernelSize / 2         kKernelSize / 2   kKernelSize / 2
// <---------------> <--------------->       <---------------> <--------------->
//        r1_               r2_                     r3_               r4_
//
//                                                     kBlockSize
//                                     <--------------------------------------->
//                                                        r5_
//
// The algorithm:
//
// 1) Consume input frames into r0_ (r1_ is zero-initialized).
// 2) Position kernel centered at start of r0_ (r2_) and generate output frames
//    until kernel is centered at start of r4_ or we've finished generating all
//    the output frames.
// 3) Copy r3_ to r1_ and r4_ to r2_.
// 4) Consume input frames into r5_ (zero-pad if we run out of input).
// 5) Goto (2) until all of input is consumed.
//
// Note: we're glossing over how the sub-sample handling works with
// |virtual_source_idx_|, etc.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "webrtc/common_audio/resampler/sinc_resampler.h"
#include "webrtc/system_wrappers/interface/compile_assert.h"
#include "webrtc/system_wrappers/interface/cpu_features_wrapper.h"
#include "webrtc/typedefs.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace webrtc {

static double SincScaleFactor(double io_ratio) {
  // |sinc_scale_factor| is basically the normalized cutoff frequency of the
  // low-pass filter.
  double sinc_scale_factor = io_ratio > 1.0 ? 1.0 / io_ratio : 1.0;

  // The sinc function is an idealized brick-wall filter, but since we're
  // windowing it the transition from pass to stop does not happen right away.
  // So we should adjust the low pass filter cutoff slightly downward to avoid
  // some aliasing at the very high-end.
  // TODO(crogers): this value is empirical and to be more exact should vary
  // depending on kKernelSize.
  sinc_scale_factor *= 0.9;

  return sinc_scale_factor;
}

SincResampler::SincResampler(double io_sample_rate_ratio,
                             SincResamplerCallback* read_cb,
                             int block_size)
    : io_sample_rate_ratio_(io_sample_rate_ratio),
      virtual_source_idx_(0),
      buffer_primed_(false),
      read_cb_(read_cb),
      block_size_(block_size),
      buffer_size_(block_size_ + kKernelSize),
      // Create input buffers with a 16-byte alignment for SSE optimizations.
      kernel_storage_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * kKernelStorageSize, 16))),
      kernel_pre_sinc_storage_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * kKernelStorageSize, 16))),
      kernel_window_storage_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * kKernelStorageSize, 16))),
      input_buffer_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * buffer_size_, 16))),
#if defined(WEBRTC_ARCH_X86_FAMILY) && !defined(__SSE__)
      convolve_proc_(WebRtc_GetCPUInfo(kSSE2) ? Convolve_SSE : Convolve_C),
#elif defined(WEBRTC_ARCH_ARM_V7) && !defined(WEBRTC_ARCH_ARM_NEON)
      convolve_proc_(WebRtc_GetCPUFeaturesARM() & kCPUFeatureNEON ?
                     Convolve_NEON : Convolve_C),
#endif
      // Setup various region pointers in the buffer (see diagram above).
      r0_(input_buffer_.get() + kKernelSize / 2),
      r1_(input_buffer_.get()),
      r2_(r0_),
      r3_(r0_ + block_size_ - kKernelSize / 2),
      r4_(r0_ + block_size_),
      r5_(r0_ + kKernelSize / 2) {
  Initialize();
  InitializeKernel();
}

SincResampler::SincResampler(double io_sample_rate_ratio,
                             SincResamplerCallback* read_cb)
    : io_sample_rate_ratio_(io_sample_rate_ratio),
      virtual_source_idx_(0),
      buffer_primed_(false),
      read_cb_(read_cb),
      block_size_(kDefaultBlockSize),
      buffer_size_(kDefaultBufferSize),
      // Create input buffers with a 16-byte alignment for SSE optimizations.
      kernel_storage_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * kKernelStorageSize, 16))),
      kernel_pre_sinc_storage_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * kKernelStorageSize, 16))),
      kernel_window_storage_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * kKernelStorageSize, 16))),
      input_buffer_(static_cast<float*>(
          AlignedMalloc(sizeof(float) * buffer_size_, 16))),
#if defined(WEBRTC_ARCH_X86_FAMILY) && !defined(__SSE__)
      convolve_proc_(WebRtc_GetCPUInfo(kSSE2) ? Convolve_SSE : Convolve_C),
#elif defined(WEBRTC_ARCH_ARM_V7) && !defined(WEBRTC_ARCH_ARM_NEON)
      convolve_proc_(WebRtc_GetCPUFeaturesARM() & kCPUFeatureNEON ?
                     Convolve_NEON : Convolve_C),
#endif
      // Setup various region pointers in the buffer (see diagram above).
      r0_(input_buffer_.get() + kKernelSize / 2),
      r1_(input_buffer_.get()),
      r2_(r0_),
      r3_(r0_ + block_size_ - kKernelSize / 2),
      r4_(r0_ + block_size_),
      r5_(r0_ + kKernelSize / 2) {
  Initialize();
  InitializeKernel();
}

SincResampler::~SincResampler() {}

void SincResampler::Initialize() {
  // Ensure kKernelSize is a multiple of 32 for easy SSE optimizations; causes
  // r0_ and r5_ (used for input) to always be 16-byte aligned by virtue of
  // input_buffer_ being 16-byte aligned.
  COMPILE_ASSERT(kKernelSize % 32 == 0);
  assert(block_size_ > kKernelSize);
  // Basic sanity checks to ensure buffer regions are laid out correctly:
  // r0_ and r2_ should always be the same position.
  assert(r0_ == r2_);
  // r1_ at the beginning of the buffer.
  assert(r1_ == input_buffer_.get());
  // r1_ left of r2_, r2_ left of r5_ and r1_, r2_ size correct.
  assert(r2_ - r1_ == r5_ - r2_);
  // r3_ left of r4_, r5_ left of r0_ and r3_ size correct.
  assert(r4_ - r3_ == r5_ - r0_);
  // r3_, r4_ size correct and r4_ at the end of the buffer.
  assert(r4_ + (r4_ - r3_) == r1_ + buffer_size_);
  // r5_ size correct and at the end of the buffer.
  assert(r5_ + block_size_ == r1_ + buffer_size_);

  memset(kernel_storage_.get(), 0,
         sizeof(*kernel_storage_.get()) * kKernelStorageSize);
  memset(kernel_pre_sinc_storage_.get(), 0,
         sizeof(*kernel_pre_sinc_storage_.get()) * kKernelStorageSize);
  memset(kernel_window_storage_.get(), 0,
         sizeof(*kernel_window_storage_.get()) * kKernelStorageSize);
  memset(input_buffer_.get(), 0, sizeof(*input_buffer_.get()) * buffer_size_);
}

void SincResampler::InitializeKernel() {
  // Blackman window parameters.
  static const double kAlpha = 0.16;
  static const double kA0 = 0.5 * (1.0 - kAlpha);
  static const double kA1 = 0.5;
  static const double kA2 = 0.5 * kAlpha;

  // Generates a set of windowed sinc() kernels.
  // We generate a range of sub-sample offsets from 0.0 to 1.0.
  const double sinc_scale_factor = SincScaleFactor(io_sample_rate_ratio_);
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    const float subsample_offset =
        static_cast<float>(offset_idx) / kKernelOffsetCount;

    for (int i = 0; i < kKernelSize; ++i) {
      const int idx = i + offset_idx * kKernelSize;
      const float pre_sinc = M_PI * (i - kKernelSize / 2 - subsample_offset);
      kernel_pre_sinc_storage_.get()[idx] = pre_sinc;

      // Compute Blackman window, matching the offset of the sinc().
      const float x = (i - subsample_offset) / kKernelSize;
      const float window = kA0 - kA1 * cos(2.0 * M_PI * x) + kA2
          * cos(4.0 * M_PI * x);
      kernel_window_storage_.get()[idx] = window;

      // Compute the sinc with offset, then window the sinc() function and store
      // at the correct offset.
      if (pre_sinc == 0) {
        kernel_storage_.get()[idx] = sinc_scale_factor * window;
      } else {
        kernel_storage_.get()[idx] =
            window * sin(sinc_scale_factor * pre_sinc) / pre_sinc;
      }
    }
  }
}

void SincResampler::SetRatio(double io_sample_rate_ratio) {
  if (fabs(io_sample_rate_ratio_ - io_sample_rate_ratio) <
      std::numeric_limits<double>::epsilon()) {
    return;
  }

  io_sample_rate_ratio_ = io_sample_rate_ratio;

  // Optimize reinitialization by reusing values which are independent of
  // |sinc_scale_factor|.  Provides a 3x speedup.
  const double sinc_scale_factor = SincScaleFactor(io_sample_rate_ratio_);
  for (int offset_idx = 0; offset_idx <= kKernelOffsetCount; ++offset_idx) {
    for (int i = 0; i < kKernelSize; ++i) {
      const int idx = i + offset_idx * kKernelSize;
      const float window = kernel_window_storage_.get()[idx];
      const float pre_sinc = kernel_pre_sinc_storage_.get()[idx];

      if (pre_sinc == 0) {
        kernel_storage_.get()[idx] = sinc_scale_factor * window;
      } else {
        kernel_storage_.get()[idx] =
            window * sin(sinc_scale_factor * pre_sinc) / pre_sinc;
      }
    }
  }
}

// If we know the minimum architecture avoid function hopping for CPU detection.
#if defined(WEBRTC_ARCH_X86_FAMILY)
#if defined(__SSE__)
#define CONVOLVE_FUNC Convolve_SSE
#else
// X86 CPU detection required.  |convolve_proc_| will be set upon construction.
// TODO(dalecurtis): Once Chrome moves to a SSE baseline this can be removed.
#define CONVOLVE_FUNC convolve_proc_
#endif
#elif defined(WEBRTC_ARCH_ARM_V7)
#if defined(WEBRTC_ARCH_ARM_NEON)
#define CONVOLVE_FUNC Convolve_NEON
#else
// NEON CPU detection required.  |convolve_proc_| will be set upon construction.
#define CONVOLVE_FUNC convolve_proc_
#endif
#else
// Unknown architecture.
#define CONVOLVE_FUNC Convolve_C
#endif

void SincResampler::Resample(float* destination, int frames) {
  int remaining_frames = frames;

  // Step (1) -- Prime the input buffer at the start of the input stream.
  if (!buffer_primed_) {
    read_cb_->Run(r0_, block_size_ + kKernelSize / 2);
    buffer_primed_ = true;
  }

  // Step (2) -- Resample!
  while (remaining_frames) {
    while (virtual_source_idx_ < block_size_) {
      // |virtual_source_idx_| lies in between two kernel offsets so figure out
      // what they are.
      int source_idx = static_cast<int>(virtual_source_idx_);
      double subsample_remainder = virtual_source_idx_ - source_idx;

      double virtual_offset_idx = subsample_remainder * kKernelOffsetCount;
      int offset_idx = static_cast<int>(virtual_offset_idx);

      // We'll compute "convolutions" for the two kernels which straddle
      // |virtual_source_idx_|.
      float* k1 = kernel_storage_.get() + offset_idx * kKernelSize;
      float* k2 = k1 + kKernelSize;

      // Ensure |k1|, |k2| are 16-byte aligned for SIMD usage.  Should always be
      // true so long as kKernelSize is a multiple of 16.
      assert((reinterpret_cast<uintptr_t>(k1) & 0x0F) == 0u);
      assert((reinterpret_cast<uintptr_t>(k2) & 0x0F) == 0u);

      // Initialize input pointer based on quantized |virtual_source_idx_|.
      float* input_ptr = r1_ + source_idx;

      // Figure out how much to weight each kernel's "convolution".
      double kernel_interpolation_factor = virtual_offset_idx - offset_idx;
      *destination++ = CONVOLVE_FUNC(
          input_ptr, k1, k2, kernel_interpolation_factor);

      // Advance the virtual index.
      virtual_source_idx_ += io_sample_rate_ratio_;

      if (!--remaining_frames)
        return;
    }

    // Wrap back around to the start.
    virtual_source_idx_ -= block_size_;

    // Step (3) Copy r3_ to r1_ and r4_ to r2_.
    // This wraps the last input frames back to the start of the buffer.
    memcpy(r1_, r3_, sizeof(*input_buffer_.get()) * (kKernelSize / 2));
    memcpy(r2_, r4_, sizeof(*input_buffer_.get()) * (kKernelSize / 2));

    // Step (4)
    // Refresh the buffer with more input.
    read_cb_->Run(r5_, block_size_);
  }
}

#undef CONVOLVE_FUNC

int SincResampler::ChunkSize() {
  return block_size_ / io_sample_rate_ratio_;
}

int SincResampler::BlockSize() {
  return block_size_;
}

void SincResampler::Flush() {
  virtual_source_idx_ = 0;
  buffer_primed_ = false;
  memset(input_buffer_.get(), 0, sizeof(*input_buffer_.get()) * buffer_size_);
}

float SincResampler::Convolve_C(const float* input_ptr, const float* k1,
                                const float* k2,
                                double kernel_interpolation_factor) {
  float sum1 = 0;
  float sum2 = 0;

  // Generate a single output sample.  Unrolling this loop hurt performance in
  // local testing.
  int n = kKernelSize;
  while (n--) {
    sum1 += *input_ptr * *k1++;
    sum2 += *input_ptr++ * *k2++;
  }

  // Linearly interpolate the two "convolutions".
  return (1.0 - kernel_interpolation_factor) * sum1
      + kernel_interpolation_factor * sum2;
}

}  // namespace webrtc
