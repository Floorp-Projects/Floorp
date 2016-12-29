/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Specifies helper classes for intelligibility enhancement.
//

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_

#include <complex>

#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

namespace intelligibility {

// Return |current| changed towards |target|, with the change being at most
// |limit|.
float UpdateFactor(float target, float current, float limit);

// Apply a small fudge to degenerate complex values. The numbers in the array
// were chosen randomly, so that even a series of all zeroes has some small
// variability.
std::complex<float> zerofudge(std::complex<float> c);

// Incremental mean computation. Return the mean of the series with the
// mean |mean| with added |data|.
std::complex<float> NewMean(std::complex<float> mean,
                            std::complex<float> data,
                            size_t count);

// Updates |mean| with added |data|;
void AddToMean(std::complex<float> data,
               size_t count,
               std::complex<float>* mean);

// Internal helper for computing the variances of a stream of arrays.
// The result is an array of variances per position: the i-th variance
// is the variance of the stream of data on the i-th positions in the
// input arrays.
// There are four methods of computation:
//  * kStepInfinite computes variances from the beginning onwards
//  * kStepDecaying uses a recursive exponential decay formula with a
//    settable forgetting factor
//  * kStepWindowed computes variances within a moving window
//  * kStepBlocked is similar to kStepWindowed, but history is kept
//    as a rolling window of blocks: multiple input elements are used for
//    one block and the history then consists of the variances of these blocks
//    with the same effect as kStepWindowed, but less storage, so the window
//    can be longer
class VarianceArray {
 public:
  enum StepType {
    kStepInfinite = 0,
    kStepDecaying,
    kStepWindowed,
    kStepBlocked,
    kStepBlockBasedMovingAverage
  };

  // Construct an instance for the given input array length (|freqs|) and
  // computation algorithm (|type|), with the appropriate parameters.
  // |window_size| is the number of samples for kStepWindowed and
  // the number of blocks for kStepBlocked. |decay| is the forgetting factor
  // for kStepDecaying.
  VarianceArray(size_t freqs, StepType type, size_t window_size, float decay);

  // Add a new data point to the series and compute the new variances.
  // TODO(bercic) |skip_fudge| is a flag for kStepWindowed and kStepDecaying,
  // whether they should skip adding some small dummy values to the input
  // to prevent problems with all-zero inputs. Can probably be removed.
  void Step(const std::complex<float>* data, bool skip_fudge = false) {
    (this->*step_func_)(data, skip_fudge);
  }
  // Reset variances to zero and forget all history.
  void Clear();
  // Scale the input data by |scale|. Effectively multiply variances
  // by |scale^2|.
  void ApplyScale(float scale);

  // The current set of variances.
  const float* variance() const { return variance_.get(); }

  // The mean value of the current set of variances.
  float array_mean() const { return array_mean_; }

 private:
  void InfiniteStep(const std::complex<float>* data, bool dummy);
  void DecayStep(const std::complex<float>* data, bool dummy);
  void WindowedStep(const std::complex<float>* data, bool dummy);
  void BlockedStep(const std::complex<float>* data, bool dummy);
  void BlockBasedMovingAverage(const std::complex<float>* data, bool dummy);

  // TODO(ekmeyerson): Switch the following running means
  // and histories from rtc::scoped_ptr to std::vector.

  // The current average X and X^2.
  rtc::scoped_ptr<std::complex<float>[]> running_mean_;
  rtc::scoped_ptr<std::complex<float>[]> running_mean_sq_;

  // Average X and X^2 for the current block in kStepBlocked.
  rtc::scoped_ptr<std::complex<float>[]> sub_running_mean_;
  rtc::scoped_ptr<std::complex<float>[]> sub_running_mean_sq_;

  // Sample history for the rolling window in kStepWindowed and block-wise
  // histories for kStepBlocked.
  rtc::scoped_ptr<rtc::scoped_ptr<std::complex<float>[]>[]> history_;
  rtc::scoped_ptr<rtc::scoped_ptr<std::complex<float>[]>[]> subhistory_;
  rtc::scoped_ptr<rtc::scoped_ptr<std::complex<float>[]>[]> subhistory_sq_;

  // The current set of variances and sums for Welford's algorithm.
  rtc::scoped_ptr<float[]> variance_;
  rtc::scoped_ptr<float[]> conj_sum_;

  const size_t num_freqs_;
  const size_t window_size_;
  const float decay_;
  size_t history_cursor_;
  size_t count_;
  float array_mean_;
  bool buffer_full_;
  void (VarianceArray::*step_func_)(const std::complex<float>*, bool);
};

// Helper class for smoothing gain changes. On each applicatiion step, the
// currently used gains are changed towards a set of settable target gains,
// constrained by a limit on the magnitude of the changes.
class GainApplier {
 public:
  GainApplier(size_t freqs, float change_limit);

  // Copy |in_block| to |out_block|, multiplied by the current set of gains,
  // and step the current set of gains towards the target set.
  void Apply(const std::complex<float>* in_block,
             std::complex<float>* out_block);

  // Return the current target gain set. Modify this array to set the targets.
  float* target() const { return target_.get(); }

 private:
  const size_t num_freqs_;
  const float change_limit_;
  rtc::scoped_ptr<float[]> target_;
  rtc::scoped_ptr<float[]> current_;
};

}  // namespace intelligibility

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_INTELLIGIBILITY_INTELLIGIBILITY_UTILS_H_
