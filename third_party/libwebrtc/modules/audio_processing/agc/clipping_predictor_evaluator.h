/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_EVALUATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_EVALUATOR_H_

#include <vector>

#include "absl/types/optional.h"

namespace webrtc {

// Counts true/false positives/negatives while observing sequences of flag pairs
// that indicate whether clipping has been detected and/or if clipping is
// predicted. When a true positive is found measures the time interval between
// prediction and detection events.
// From the time a prediction is observed and for a period equal to
// `history_size` calls to `Observe()`, one or more detections are expected. If
// the expectation is met, a true positives is added and the time interval
// between the earliest prediction and the detection is recorded; otherwise,
// when the deadline is reached, a false positive is added. Note that one
// detection matches all the expected detections that have not expired - i.e.,
// one detection counts as multiple true positives.
// If a detection is observed, but no prediction has been observed over the past
// `history_size` calls to `Observe()`, then a false negative is added;
// otherwise, a true negative is added.
class ClippingPredictorEvaluator {
 public:
  // Ctor. `history_size` indicates how long to wait for a call to `Observe()`
  // having `clipping_detected` set to true from the time clipping is predicted.
  explicit ClippingPredictorEvaluator(int history_size);
  ClippingPredictorEvaluator(const ClippingPredictorEvaluator&) = delete;
  ClippingPredictorEvaluator& operator=(const ClippingPredictorEvaluator&) =
      delete;
  ~ClippingPredictorEvaluator();

  // Observes whether clipping has been detected and/or if clipping is
  // predicted. When predicted one or more detections are expected in the next
  // `history_size_` calls of `Observe()`. When true positives are found returns
  // the prediction interval between the earliest prediction and the detection.
  absl::optional<int> Observe(bool clipping_detected, bool clipping_predicted);

  // Removes any expectation recently set after a call to `Observe()` having
  // `clipping_predicted` set to true.
  void Reset();

  // Metrics getters.
  int true_positives() const { return true_positives_; }
  int true_negatives() const { return true_negatives_; }
  int false_positives() const { return false_positives_; }
  int false_negatives() const { return false_negatives_; }

 private:
  const int history_size_;

  // State of a detection expected to be observed after a prediction.
  struct ExpectedDetection {
    // Time to live (TTL); remaining number of `Observe()` calls to match a call
    // having `clipping_detected` set to true.
    int ttl;
    // True if an `Observe()` call having `clipping_detected` set to true has
    // been observed.
    bool detected;
  };
  // Ring buffer of expected detections.
  const int ring_buffer_capacity_;
  std::vector<ExpectedDetection> ring_buffer_;
  int ring_buffer_tail_;
  int ring_buffer_size_;

  // Pushes `expected_detection` into `expected_matches_ring_buffer_`.
  void Push(ExpectedDetection expected_detection);
  // Decreased the TTLs in `expected_matches_ring_buffer_` and removes expired
  // items.
  void DecreaseTimesToLive();
  // Returns the prediction interval for the earliest unexpired expected
  // detection if any.
  absl::optional<int> FindEarliestPredictionInterval() const;
  // Marks all the items in `expected_matches_ring_buffer_` as `detected` and
  // returns the number of updated items.
  int MarkExpectedDetectionAsDetected();
  // Returns true if `expected_matches_ring_buffer_` has an item having `ttl`
  // equal to 0 (expired) and `detected` equal to false (unmatched).
  bool HasExpiredUnmatchedExpectedDetection() const;

  // Metrics.
  int true_positives_;
  int true_negatives_;
  int false_positives_;
  int false_negatives_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_EVALUATOR_H_
