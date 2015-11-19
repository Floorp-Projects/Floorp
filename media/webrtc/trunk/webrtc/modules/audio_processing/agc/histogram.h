/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_HISTOGRAM_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_HISTOGRAM_H_

#include <string.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// This class implements the histogram of loudness with circular buffers so that
// the histogram tracks the last T seconds of the loudness.
class Histogram {
 public:
  // Create a non-sliding Histogram.
  static Histogram* Create();

  // Create a sliding Histogram, i.e. the histogram represents the last
  // |window_size| samples.
  static Histogram* Create(int window_size);
  ~Histogram();

  // Insert RMS and the corresponding activity probability.
  void Update(double rms, double activity_probability);

  // Reset the histogram, forget the past.
  void Reset();

  // Current loudness, which is actually the mean of histogram in loudness
  // domain.
  double CurrentRms() const;

  // Sum of the histogram content.
  double AudioContent() const;

  // Number of times the histogram has been updated.
  int num_updates() const { return num_updates_; }

 private:
  Histogram();
  explicit Histogram(int window);

  // Find the histogram bin associated with the given |rms|.
  int GetBinIndex(double rms);

  void RemoveOldestEntryAndUpdate();
  void InsertNewestEntryAndUpdate(int activity_prob_q10, int hist_index);
  void UpdateHist(int activity_prob_q10, int hist_index);
  void RemoveTransient();

  // Number of histogram bins.
  static const int kHistSize = 77;

  // Number of times the histogram is updated
  int num_updates_;
  // Audio content, this should be equal to the sum of the components of
  // |bin_count_q10_|.
  int64_t audio_content_q10_;

  // Histogram of input RMS in Q10 with |kHistSize_| bins. In each 'Update(),'
  // we increment the associated histogram-bin with the given probability. The
  // increment is implemented in Q10 to avoid rounding errors.
  int64_t bin_count_q10_[kHistSize];

  // Circular buffer for probabilities
  rtc::scoped_ptr<int[]> activity_probability_;
  // Circular buffer for histogram-indices of probabilities.
  rtc::scoped_ptr<int[]> hist_bin_index_;
  // Current index of circular buffer, where the newest data will be written to,
  // therefore, pointing to the oldest data if buffer is full.
  int buffer_index_;
  // Indicating if buffer is full and we had a wrap around.
  int buffer_is_full_;
  // Size of circular buffer.
  int len_circular_buffer_;
  int len_high_activity_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_HISTOGRAM_H_
