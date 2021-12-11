/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_
#define MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_

#include <complex>
#include <vector>

namespace webrtc {

namespace intelligibility {

// Internal helper for computing the power of a stream of arrays.
// The result is an array of power per position: the i-th power is the power of
// the stream of data on the i-th positions in the input arrays.
template <typename T>
class PowerEstimator {
 public:
  // Construct an instance for the given input array length (|freqs|), with the
  // appropriate parameters. |decay| is the forgetting factor.
  PowerEstimator(size_t freqs, float decay);

  // Add a new data point to the series.
  void Step(const T* data);

  // The current power array.
  const std::vector<float>& power() { return power_; };

 private:
  // The current power array.
  std::vector<float> power_;

  const float decay_;
};

// Helper class for smoothing gain changes. On each application step, the
// currently used gains are changed towards a set of settable target gains,
// constrained by a limit on the relative changes.
class GainApplier {
 public:
  GainApplier(size_t freqs, float relative_change_limit);

  ~GainApplier();

  // Copy |in_block| to |out_block|, multiplied by the current set of gains,
  // and step the current set of gains towards the target set.
  void Apply(const std::complex<float>* in_block,
             std::complex<float>* out_block);

  // Return the current target gain set. Modify this array to set the targets.
  float* target() { return target_.data(); }

 private:
  const size_t num_freqs_;
  const float relative_change_limit_;
  std::vector<float> target_;
  std::vector<float> current_;
};

// Helper class to delay a signal by an integer number of samples.
class DelayBuffer {
 public:
  DelayBuffer(size_t delay, size_t num_channels);

  ~DelayBuffer();

  void Delay(float* const* data, size_t length);

 private:
  std::vector<std::vector<float>> buffer_;
  size_t read_index_;
};

}  // namespace intelligibility

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_
