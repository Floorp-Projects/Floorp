/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor_evaluator.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using testing::Eq;
using testing::Field;
using testing::Optional;

constexpr bool kDetected = true;
constexpr bool kNotDetected = false;

constexpr bool kPredicted = true;
constexpr bool kNotPredicted = false;

int SumTrueFalsePositivesNegatives(
    const ClippingPredictorEvaluator& evaluator) {
  return evaluator.counters().true_positives +
         evaluator.counters().true_negatives +
         evaluator.counters().false_positives +
         evaluator.counters().false_negatives;
}

// Checks the metrics after init - i.e., no call to `Observe()`.
TEST(ClippingPredictionEvalTest, Init) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

class ClippingPredictorEvaluatorParameterization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  uint64_t seed() const {
    return rtc::checked_cast<uint64_t>(std::get<0>(GetParam()));
  }
  int history_size() const { return std::get<1>(GetParam()); }
};

// Checks that after each call to `Observe()` at most one metric changes.
TEST_P(ClippingPredictorEvaluatorParameterization, AtMostOneMetricChanges) {
  constexpr int kNumCalls = 123;
  Random random_generator(seed());
  ClippingPredictorEvaluator evaluator(history_size());

  for (int i = 0; i < kNumCalls; ++i) {
    SCOPED_TRACE(i);
    // Read metrics before `Observe()` is called.
    const int last_tp = evaluator.counters().true_positives;
    const int last_tn = evaluator.counters().true_negatives;
    const int last_fp = evaluator.counters().false_positives;
    const int last_fn = evaluator.counters().false_negatives;
    // `Observe()` a random observation.
    bool clipping_detected = random_generator.Rand<bool>();
    bool clipping_predicted = random_generator.Rand<bool>();
    evaluator.Observe(clipping_detected, clipping_predicted);

    // Check that at most one metric has changed.
    int num_changes = 0;
    num_changes += last_tp == evaluator.counters().true_positives ? 0 : 1;
    num_changes += last_tn == evaluator.counters().true_negatives ? 0 : 1;
    num_changes += last_fp == evaluator.counters().false_positives ? 0 : 1;
    num_changes += last_fn == evaluator.counters().false_negatives ? 0 : 1;
    EXPECT_GE(num_changes, 0);
    EXPECT_LE(num_changes, 1);
  }
}

// Checks that after each call to `Observe()` each metric either remains
// unchanged or grows.
TEST_P(ClippingPredictorEvaluatorParameterization, MetricsAreWeaklyMonotonic) {
  constexpr int kNumCalls = 123;
  Random random_generator(seed());
  ClippingPredictorEvaluator evaluator(history_size());

  for (int i = 0; i < kNumCalls; ++i) {
    SCOPED_TRACE(i);
    // Read metrics before `Observe()` is called.
    const int last_tp = evaluator.counters().true_positives;
    const int last_tn = evaluator.counters().true_negatives;
    const int last_fp = evaluator.counters().false_positives;
    const int last_fn = evaluator.counters().false_negatives;
    // `Observe()` a random observation.
    bool clipping_detected = random_generator.Rand<bool>();
    bool clipping_predicted = random_generator.Rand<bool>();
    evaluator.Observe(clipping_detected, clipping_predicted);

    // Check that metrics are weakly monotonic.
    EXPECT_GE(evaluator.counters().true_positives, last_tp);
    EXPECT_GE(evaluator.counters().true_negatives, last_tn);
    EXPECT_GE(evaluator.counters().false_positives, last_fp);
    EXPECT_GE(evaluator.counters().false_negatives, last_fn);
  }
}

// Checks that after each call to `Observe()` the growth speed of each metrics
// is bounded.
TEST_P(ClippingPredictorEvaluatorParameterization, BoundedMetricsGrowth) {
  constexpr int kNumCalls = 123;
  Random random_generator(seed());
  ClippingPredictorEvaluator evaluator(history_size());

  for (int i = 0; i < kNumCalls; ++i) {
    SCOPED_TRACE(i);
    // Read metrics before `Observe()` is called.
    const int last_tp = evaluator.counters().true_positives;
    const int last_tn = evaluator.counters().true_negatives;
    const int last_fp = evaluator.counters().false_positives;
    const int last_fn = evaluator.counters().false_negatives;
    // `Observe()` a random observation.
    bool clipping_detected = random_generator.Rand<bool>();
    bool clipping_predicted = random_generator.Rand<bool>();
    evaluator.Observe(clipping_detected, clipping_predicted);

    // Check that TPs grow by at most `history_size() + 1`. Such an upper bound
    // is reached when multiple predictions are matched by a single detection.
    EXPECT_LE(evaluator.counters().true_positives - last_tp,
              history_size() + 1);
    // Check that TNs, FPs and FNs grow by at most one. `max_growth`.
    EXPECT_LE(evaluator.counters().true_negatives - last_tn, 1);
    EXPECT_LE(evaluator.counters().false_positives - last_fp, 1);
    EXPECT_LE(evaluator.counters().false_negatives - last_fn, 1);
  }
}

// Checks that `Observe()` returns a prediction interval if and only if one or
// more true positives are found.
TEST_P(ClippingPredictorEvaluatorParameterization,
       PredictionIntervalIfAndOnlyIfTruePositives) {
  constexpr int kNumCalls = 123;
  Random random_generator(seed());
  ClippingPredictorEvaluator evaluator(history_size());

  for (int i = 0; i < kNumCalls; ++i) {
    SCOPED_TRACE(i);
    // Read true positives before `Observe()` is called.
    const int last_tp = evaluator.counters().true_positives;
    // `Observe()` a random observation.
    bool clipping_detected = random_generator.Rand<bool>();
    bool clipping_predicted = random_generator.Rand<bool>();
    absl::optional<int> prediction_interval =
        evaluator.Observe(clipping_detected, clipping_predicted);

    // Check that the prediction interval is returned when a true positive is
    // found.
    if (evaluator.counters().true_positives == last_tp) {
      EXPECT_FALSE(prediction_interval.has_value());
    } else {
      EXPECT_TRUE(prediction_interval.has_value());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ClippingPredictionEvalTest,
    ClippingPredictorEvaluatorParameterization,
    ::testing::Combine(::testing::Values(4, 8, 15, 16, 23, 42),
                       ::testing::Values(1, 10, 21)));

// Checks that, observing a detection and a prediction after init, produces a
// true positive.
TEST(ClippingPredictionEvalTest, OneTruePositiveAfterInit) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kDetected, kPredicted);
  EXPECT_EQ(evaluator.counters().true_positives, 1);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that, observing a detection but no prediction after init, produces a
// false negative.
TEST(ClippingPredictionEvalTest, OneFalseNegativeAfterInit) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_negatives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
}

// Checks that, observing no detection but a prediction after init, produces a
// false positive after expiration.
TEST(ClippingPredictionEvalTest, OneFalsePositiveAfterInit) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that, observing no detection and no prediction after init, produces a
// true negative.
TEST(ClippingPredictionEvalTest, OneTrueNegativeAfterInit) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_negatives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that the evaluator detects true negatives when clipping is neither
// predicted nor detected.
TEST(ClippingPredictionEvalTest, NeverDetectedAndNotPredicted) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_negatives, 4);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that the evaluator detects a false negative when clipping is detected
// but not predicted.
TEST(ClippingPredictionEvalTest, DetectedButNotPredicted) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_negatives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().true_negatives, 3);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
}

// Checks that the evaluator does not detect a false positive when clipping is
// predicted but not detected until the observation period expires.
TEST(ClippingPredictionEvalTest, PredictedOnceAndNeverDetectedBeforeDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that the evaluator detects a false positive when clipping is predicted
// but detected after the observation period expires.
TEST(ClippingPredictionEvalTest, PredictedOnceButDetectedAfterDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 0);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 1);
}

// Checks that a prediction followed by a detection counts as true positive.
TEST(ClippingPredictionEvalTest, PredictedOnceAndThenImmediatelyDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_positives, 1);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that a prediction followed by a delayed detection counts as true
// positive if the delay is within the observation period.
TEST(ClippingPredictionEvalTest, PredictedOnceAndDetectedBeforeDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_positives, 1);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that a prediction followed by a delayed detection counts as true
// positive if the delay equals the observation period.
TEST(ClippingPredictionEvalTest, PredictedOnceAndDetectedAtDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_positives, 1);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that a prediction followed by a multiple adjacent detections within
// the deadline counts as a single true positive and that, after the deadline,
// a detection counts as a false negative.
TEST(ClippingPredictionEvalTest, PredictedOnceAndDetectedMultipleTimes) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  // Multiple detections.
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_positives, 1);
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_positives, 1);
  // A detection outside of the observation period counts as false negative.
  evaluator.Observe(kDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().false_negatives, 1);
  EXPECT_EQ(SumTrueFalsePositivesNegatives(evaluator), 2);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
}

// Checks that a false positive is added when clipping is detected after a too
// early prediction.
TEST(ClippingPredictionEvalTest,
     PredictedMultipleTimesAndDetectedOnceAfterDeadline) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // ---+
  evaluator.Observe(kNotDetected, kPredicted);  //    |
  evaluator.Observe(kNotDetected, kPredicted);  //    |
  evaluator.Observe(kNotDetected, kPredicted);  // <--+ Not matched.
  // The time to match a detection after the first prediction expired.
  EXPECT_EQ(evaluator.counters().false_positives, 1);
  evaluator.Observe(kDetected, kNotPredicted);
  // The detection above does not match the first prediction because it happened
  // after the deadline of the 1st prediction.
  EXPECT_EQ(evaluator.counters().false_positives, 1);

  EXPECT_EQ(evaluator.counters().true_positives, 3);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that multiple consecutive predictions match the first detection
// observed before the expected detection deadline expires.
TEST(ClippingPredictionEvalTest, PredictedMultipleTimesAndDetectedOnce) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // --+
  evaluator.Observe(kNotDetected, kPredicted);  //   | --+
  evaluator.Observe(kNotDetected, kPredicted);  //   |   | --+
  evaluator.Observe(kDetected, kNotPredicted);  // <-+ <-+ <-+
  EXPECT_EQ(evaluator.counters().true_positives, 3);
  // The following observations do not generate any true negatives as they
  // belong to the observation period of the last prediction - for which a
  // detection has already been matched.
  const int true_negatives = evaluator.counters().true_negatives;
  evaluator.Observe(kNotDetected, kNotPredicted);
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_negatives, true_negatives);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that multiple consecutive predictions match the multiple detections
// observed before the expected detection deadline expires.
TEST(ClippingPredictionEvalTest,
     PredictedMultipleTimesAndDetectedMultipleTimes) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // --+
  evaluator.Observe(kNotDetected, kPredicted);  //   | --+
  evaluator.Observe(kNotDetected, kPredicted);  //   |   | --+
  evaluator.Observe(kDetected, kNotPredicted);  // <-+ <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);  //     <-+ <-+
  EXPECT_EQ(evaluator.counters().true_positives, 3);
  // The following observation does not generate a true negative as it belongs
  // to the observation period of the last prediction - for which two detections
  // have already been matched.
  const int true_negatives = evaluator.counters().true_negatives;
  evaluator.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(evaluator.counters().true_negatives, true_negatives);

  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that multiple consecutive predictions match all the detections
// observed before the expected detection deadline expires.
TEST(ClippingPredictionEvalTest, PredictedMultipleTimesAndAllDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);  // --+
  evaluator.Observe(kNotDetected, kPredicted);  //   | --+
  evaluator.Observe(kNotDetected, kPredicted);  //   |   | --+
  evaluator.Observe(kDetected, kNotPredicted);  // <-+ <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);  //     <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);  //         <-+
  EXPECT_EQ(evaluator.counters().true_positives, 3);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

// Checks that multiple non-consecutive predictions match all the detections
// observed before the expected detection deadline expires.
TEST(ClippingPredictionEvalTest, PredictedMultipleTimesWithGapAndAllDetected) {
  ClippingPredictorEvaluator evaluator(/*history_size=*/3);
  evaluator.Observe(kNotDetected, kPredicted);     // --+
  evaluator.Observe(kNotDetected, kNotPredicted);  //   |
  evaluator.Observe(kNotDetected, kPredicted);     //   | --+
  evaluator.Observe(kDetected, kNotPredicted);     // <-+ <-+
  evaluator.Observe(kDetected, kNotPredicted);     //     <-+
  evaluator.Observe(kDetected, kNotPredicted);     //     <-+
  EXPECT_EQ(evaluator.counters().true_positives, 2);
  EXPECT_EQ(evaluator.counters().true_negatives, 0);
  EXPECT_EQ(evaluator.counters().false_positives, 0);
  EXPECT_EQ(evaluator.counters().false_negatives, 0);
}

class ClippingPredictorEvaluatorPredictionIntervalParameterization
    : public ::testing::TestWithParam<std::tuple<int, int>> {
 protected:
  int num_extra_observe_calls() const { return std::get<0>(GetParam()); }
  int history_size() const { return std::get<1>(GetParam()); }
};

// Checks that the minimum prediction interval is returned if clipping is
// correctly predicted as soon as detected - i.e., no anticipation.
TEST_P(ClippingPredictorEvaluatorPredictionIntervalParameterization,
       MinimumPredictionInterval) {
  ClippingPredictorEvaluator evaluator(history_size());
  for (int i = 0; i < num_extra_observe_calls(); ++i) {
    EXPECT_EQ(evaluator.Observe(kNotDetected, kNotPredicted), absl::nullopt);
  }
  absl::optional<int> prediction_interval =
      evaluator.Observe(kDetected, kPredicted);
  EXPECT_THAT(prediction_interval, Optional(Eq(0)));
}

// Checks that a prediction interval between the minimum and the maximum is
// returned if clipping is correctly predicted before it is detected but not as
// early as possible.
TEST_P(ClippingPredictorEvaluatorPredictionIntervalParameterization,
       IntermediatePredictionInterval) {
  ClippingPredictorEvaluator evaluator(history_size());
  for (int i = 0; i < num_extra_observe_calls(); ++i) {
    EXPECT_EQ(evaluator.Observe(kNotDetected, kNotPredicted), absl::nullopt);
  }
  EXPECT_EQ(evaluator.Observe(kNotDetected, kPredicted), absl::nullopt);
  EXPECT_EQ(evaluator.Observe(kNotDetected, kPredicted), absl::nullopt);
  EXPECT_EQ(evaluator.Observe(kNotDetected, kPredicted), absl::nullopt);
  absl::optional<int> prediction_interval =
      evaluator.Observe(kDetected, kPredicted);
  EXPECT_THAT(prediction_interval, Optional(Eq(3)));
}

// Checks that the maximum prediction interval is returned if clipping is
// correctly predicted as early as possible.
TEST_P(ClippingPredictorEvaluatorPredictionIntervalParameterization,
       MaximumPredictionInterval) {
  ClippingPredictorEvaluator evaluator(history_size());
  for (int i = 0; i < num_extra_observe_calls(); ++i) {
    EXPECT_EQ(evaluator.Observe(kNotDetected, kNotPredicted), absl::nullopt);
  }
  for (int i = 0; i < history_size(); ++i) {
    EXPECT_EQ(evaluator.Observe(kNotDetected, kPredicted), absl::nullopt);
  }
  absl::optional<int> prediction_interval =
      evaluator.Observe(kDetected, kPredicted);
  EXPECT_THAT(prediction_interval, Optional(Eq(history_size())));
}

// Checks that `Observe()` returns the prediction interval as soon as a true
// positive is found and never again while ongoing detections are matched to a
// previously observed prediction.
TEST_P(ClippingPredictorEvaluatorPredictionIntervalParameterization,
       PredictionIntervalReturnedOnce) {
  ASSERT_LT(num_extra_observe_calls(), history_size());
  ClippingPredictorEvaluator evaluator(history_size());
  // Observe predictions before detection.
  for (int i = 0; i < num_extra_observe_calls(); ++i) {
    EXPECT_EQ(evaluator.Observe(kNotDetected, kPredicted), absl::nullopt);
  }
  // Observe a detection.
  absl::optional<int> prediction_interval =
      evaluator.Observe(kDetected, kPredicted);
  EXPECT_TRUE(prediction_interval.has_value());
  // `Observe()` does not return a prediction interval anymore during ongoing
  // detections observed while a detection is still expected.
  for (int i = 0; i < history_size(); ++i) {
    EXPECT_EQ(evaluator.Observe(kDetected, kNotPredicted), absl::nullopt);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ClippingPredictionEvalTest,
    ClippingPredictorEvaluatorPredictionIntervalParameterization,
    ::testing::Combine(::testing::Values(0, 3, 5), ::testing::Values(7, 11)));

// Checks that, when a detection is expected, the expectation is removed if and
// only if `Reset()` is called after a prediction is observed.
TEST(ClippingPredictionEvalTest, NoFalsePositivesAfterReset) {
  constexpr int kHistorySize = 2;

  ClippingPredictorEvaluator with_reset(kHistorySize);
  with_reset.Observe(kNotDetected, kPredicted);
  with_reset.Reset();
  with_reset.Observe(kNotDetected, kNotPredicted);
  with_reset.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(with_reset.counters().true_positives, 0);
  EXPECT_EQ(with_reset.counters().true_negatives, 2);
  EXPECT_EQ(with_reset.counters().false_positives, 0);
  EXPECT_EQ(with_reset.counters().false_negatives, 0);

  ClippingPredictorEvaluator no_reset(kHistorySize);
  no_reset.Observe(kNotDetected, kPredicted);
  no_reset.Observe(kNotDetected, kNotPredicted);
  no_reset.Observe(kNotDetected, kNotPredicted);
  EXPECT_EQ(no_reset.counters().true_positives, 0);
  EXPECT_EQ(no_reset.counters().true_negatives, 0);
  EXPECT_EQ(no_reset.counters().false_positives, 1);
  EXPECT_EQ(no_reset.counters().false_negatives, 0);
}

class ComputeClippingPredictionMetricsParameterization
    : public ::testing::TestWithParam<int> {
 protected:
  int true_negatives() const { return GetParam(); }
};

// Checks that `ComputeClippingPredictionMetrics()` does not return metrics if
// precision cannot be defined - i.e., TP + FP is zero.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       NoMetricsWithUndefinedPrecision) {
  EXPECT_EQ(ComputeClippingPredictionMetrics(
                /*counters=*/{/*true_positives=*/0,
                              /*true_negatives=*/true_negatives(),
                              /*false_positives=*/0,
                              /*false_negatives=*/0}),
            absl::nullopt);
  EXPECT_EQ(ComputeClippingPredictionMetrics(
                /*counters=*/{/*true_positives=*/0,
                              /*true_negatives=*/true_negatives(),
                              /*false_positives=*/0,
                              /*false_negatives=*/1}),
            absl::nullopt);
}

// Checks that `ComputeClippingPredictionMetrics()` does not return metrics if
// recall cannot be defined - i.e., TP + FN is zero.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       NoMetricsWithUndefinedRecall) {
  EXPECT_EQ(ComputeClippingPredictionMetrics(
                /*counters=*/{/*true_positives=*/0,
                              /*true_negatives=*/true_negatives(),
                              /*false_positives=*/0,
                              /*false_negatives=*/0}),
            absl::nullopt);
  EXPECT_EQ(ComputeClippingPredictionMetrics(
                /*counters=*/{/*true_positives=*/0,
                              /*true_negatives=*/true_negatives(),
                              /*false_positives=*/1,
                              /*false_negatives=*/0}),
            absl::nullopt);
}

// Checks that `ComputeClippingPredictionMetrics()` does not return metrics if
// the F1 score cannot be defined - i.e., P + R is zero.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       NoMetricsWithUndefinedF1Score) {
  EXPECT_EQ(ComputeClippingPredictionMetrics(
                /*counters=*/{/*true_positives=*/0,
                              /*true_negatives=*/true_negatives(),
                              /*false_positives=*/1,
                              /*false_negatives=*/1}),
            absl::nullopt);
}

// Checks that the highest precision is reached when there are no false
// positives.
TEST_P(ComputeClippingPredictionMetricsParameterization, HighestPrecision) {
  EXPECT_THAT(ComputeClippingPredictionMetrics(
                  /*counters=*/{/*true_positives=*/1,
                                /*true_negatives=*/true_negatives(),
                                /*false_positives=*/0,
                                /*false_negatives=*/1}),
              Optional(Field(&ClippingPredictionMetrics::precision, Eq(1.0f))));
}

// Checks that the highest recall is reached when there are no false
// negatives.
TEST_P(ComputeClippingPredictionMetricsParameterization, HighestRecall) {
  EXPECT_THAT(ComputeClippingPredictionMetrics(
                  /*counters=*/{/*true_positives=*/1,
                                /*true_negatives=*/true_negatives(),
                                /*false_positives=*/1,
                                /*false_negatives=*/0}),
              Optional(Field(&ClippingPredictionMetrics::recall, Eq(1.0f))));
}

// Checks that 50% precision and 50% recall is reached when the number of true
// positives, false positives and false negatives are the same.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       PrecisionAndRecall50Percent) {
  absl::optional<ClippingPredictionMetrics> metrics =
      ComputeClippingPredictionMetrics(
          /*counters=*/{/*true_positives=*/42,
                        /*true_negatives=*/true_negatives(),
                        /*false_positives=*/42,
                        /*false_negatives=*/42});
  ASSERT_TRUE(metrics.has_value());
  EXPECT_EQ(metrics->precision, 0.5f);
  EXPECT_EQ(metrics->recall, 0.5f);
  EXPECT_EQ(metrics->f1_score, 0.5f);
}

// Checks that the highest precision, recall and F1 score are jointly reached
// when there are no false positives and no false negatives.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       HighestPrecisionRecallF1Score) {
  absl::optional<ClippingPredictionMetrics> metrics =
      ComputeClippingPredictionMetrics(
          /*counters=*/{/*true_positives=*/123,
                        /*true_negatives=*/true_negatives(),
                        /*false_positives=*/0,
                        /*false_negatives=*/0});
  ASSERT_TRUE(metrics.has_value());
  EXPECT_EQ(metrics->precision, 1.0f);
  EXPECT_EQ(metrics->recall, 1.0f);
  EXPECT_EQ(metrics->f1_score, 1.0f);
}

// Checks that precision is lower than recall when there are more false
// positives than false negatives.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       PrecisionLowerThanRecall) {
  absl::optional<ClippingPredictionMetrics> metrics =
      ComputeClippingPredictionMetrics(
          /*counters=*/{/*true_positives=*/1,
                        /*true_negatives=*/true_negatives(),
                        /*false_positives=*/8,
                        /*false_negatives=*/1});
  ASSERT_TRUE(metrics.has_value());
  EXPECT_LT(metrics->precision, metrics->recall);
}

// Checks that precision is greater than recall when there are less false
// positives than false negatives.
TEST_P(ComputeClippingPredictionMetricsParameterization,
       PrecisionGreaterThanRecall) {
  absl::optional<ClippingPredictionMetrics> metrics =
      ComputeClippingPredictionMetrics(
          /*counters=*/{/*true_positives=*/1,
                        /*true_negatives=*/true_negatives(),
                        /*false_positives=*/1,
                        /*false_negatives=*/8});
  ASSERT_TRUE(metrics.has_value());
  EXPECT_GT(metrics->precision, metrics->recall);
}

// Checks that swapping precision and recall does not change the F1 score.
TEST_P(ComputeClippingPredictionMetricsParameterization, SameF1Score) {
  absl::optional<ClippingPredictionMetrics> m1 =
      ComputeClippingPredictionMetrics(
          /*counters=*/{/*true_positives=*/1,
                        /*true_negatives=*/true_negatives(),
                        /*false_positives=*/8,
                        /*false_negatives=*/1});
  absl::optional<ClippingPredictionMetrics> m2 =
      ComputeClippingPredictionMetrics(
          /*counters=*/{/*true_positives=*/1,
                        /*true_negatives=*/true_negatives(),
                        /*false_positives=*/1,
                        /*false_negatives=*/8});
  // Preconditions.
  ASSERT_TRUE(m1.has_value());
  ASSERT_TRUE(m2.has_value());
  ASSERT_EQ(m1->precision, m2->recall);
  ASSERT_EQ(m1->recall, m2->precision);
  // Same F1 score.
  EXPECT_EQ(m1->f1_score, m2->f1_score);
}

INSTANTIATE_TEST_SUITE_P(ClippingPredictionEvalTest,
                         ComputeClippingPredictionMetricsParameterization,
                         ::testing::Values(0, 1, 11));

}  // namespace
}  // namespace webrtc
