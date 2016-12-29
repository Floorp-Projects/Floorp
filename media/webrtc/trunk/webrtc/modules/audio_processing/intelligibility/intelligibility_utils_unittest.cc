/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Unit tests for intelligibility utils.
//

#include <math.h>
#include <complex>
#include <iostream>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_utils.h"

using std::complex;
using std::vector;

namespace webrtc {

namespace intelligibility {

vector<vector<complex<float>>> GenerateTestData(int freqs, int samples) {
  vector<vector<complex<float>>> data(samples);
  for (int i = 0; i < samples; i++) {
    for (int j = 0; j < freqs; j++) {
      const float val = 0.99f / ((i + 1) * (j + 1));
      data[i].push_back(complex<float>(val, val));
    }
  }
  return data;
}

// Tests UpdateFactor.
TEST(IntelligibilityUtilsTest, TestUpdateFactor) {
  EXPECT_EQ(0, intelligibility::UpdateFactor(0, 0, 0));
  EXPECT_EQ(4, intelligibility::UpdateFactor(4, 2, 3));
  EXPECT_EQ(3, intelligibility::UpdateFactor(4, 2, 1));
  EXPECT_EQ(2, intelligibility::UpdateFactor(2, 4, 3));
  EXPECT_EQ(3, intelligibility::UpdateFactor(2, 4, 1));
}

// Tests zerofudge.
TEST(IntelligibilityUtilsTest, TestCplx) {
  complex<float> t0(1.f, 0.f);
  t0 = intelligibility::zerofudge(t0);
  EXPECT_NE(t0.imag(), 0.f);
  EXPECT_NE(t0.real(), 0.f);
}

// Tests NewMean and AddToMean.
TEST(IntelligibilityUtilsTest, TestMeanUpdate) {
  const complex<float> data[] = {{3, 8}, {7, 6}, {2, 1}, {8, 9}, {0, 6}};
  const complex<float> means[] = {{3, 8}, {5, 7}, {4, 5}, {5, 6}, {4, 6}};
  complex<float> mean(3, 8);
  for (size_t i = 0; i < arraysize(data); i++) {
    EXPECT_EQ(means[i], NewMean(mean, data[i], i + 1));
    AddToMean(data[i], i + 1, &mean);
    EXPECT_EQ(means[i], mean);
  }
}

// Tests VarianceArray, for all variance step types.
TEST(IntelligibilityUtilsTest, TestVarianceArray) {
  const int kFreqs = 10;
  const int kSamples = 100;
  const int kWindowSize = 10;  // Should pass for all kWindowSize > 1.
  const float kDecay = 0.5f;
  vector<VarianceArray::StepType> step_types;
  step_types.push_back(VarianceArray::kStepInfinite);
  step_types.push_back(VarianceArray::kStepDecaying);
  step_types.push_back(VarianceArray::kStepWindowed);
  step_types.push_back(VarianceArray::kStepBlocked);
  step_types.push_back(VarianceArray::kStepBlockBasedMovingAverage);
  const vector<vector<complex<float>>> test_data(
      GenerateTestData(kFreqs, kSamples));
  for (auto step_type : step_types) {
    VarianceArray variance_array(kFreqs, step_type, kWindowSize, kDecay);
    EXPECT_EQ(0, variance_array.variance()[0]);
    EXPECT_EQ(0, variance_array.array_mean());
    variance_array.ApplyScale(2.0f);
    EXPECT_EQ(0, variance_array.variance()[0]);
    EXPECT_EQ(0, variance_array.array_mean());

    // Makes sure Step is doing something.
    variance_array.Step(&test_data[0][0]);
    for (int i = 1; i < kSamples; i++) {
      variance_array.Step(&test_data[i][0]);
      EXPECT_GE(variance_array.array_mean(), 0.0f);
      EXPECT_LE(variance_array.array_mean(), 1.0f);
      for (int j = 0; j < kFreqs; j++) {
        EXPECT_GE(variance_array.variance()[j], 0.0f);
        EXPECT_LE(variance_array.variance()[j], 1.0f);
      }
    }
    variance_array.Clear();
    EXPECT_EQ(0, variance_array.variance()[0]);
    EXPECT_EQ(0, variance_array.array_mean());
  }
}

// Tests exact computation on synthetic data.
TEST(IntelligibilityUtilsTest, TestMovingBlockAverage) {
  // Exact, not unbiased estimates.
  const float kTestVarianceBufferNotFull = 16.5f;
  const float kTestVarianceBufferFull1 = 66.5f;
  const float kTestVarianceBufferFull2 = 333.375f;
  const int kFreqs = 2;
  const int kSamples = 50;
  const int kWindowSize = 2;
  const float kDecay = 0.5f;
  const float kMaxError = 0.0001f;

  VarianceArray variance_array(
      kFreqs, VarianceArray::kStepBlockBasedMovingAverage, kWindowSize, kDecay);

  vector<vector<complex<float>>> test_data(kSamples);
  for (int i = 0; i < kSamples; i++) {
    for (int j = 0; j < kFreqs; j++) {
      if (i < 30) {
        test_data[i].push_back(complex<float>(static_cast<float>(kSamples - i),
                                              static_cast<float>(i + 1)));
      } else {
        test_data[i].push_back(complex<float>(0.f, 0.f));
      }
    }
  }

  for (int i = 0; i < kSamples; i++) {
    variance_array.Step(&test_data[i][0]);
    for (int j = 0; j < kFreqs; j++) {
      if (i < 9) {  // In utils, kWindowBlockSize = 10.
        EXPECT_EQ(0, variance_array.variance()[j]);
      } else if (i < 19) {
        EXPECT_NEAR(kTestVarianceBufferNotFull, variance_array.variance()[j],
                    kMaxError);
      } else if (i < 39) {
        EXPECT_NEAR(kTestVarianceBufferFull1, variance_array.variance()[j],
                    kMaxError);
      } else if (i < 49) {
        EXPECT_NEAR(kTestVarianceBufferFull2, variance_array.variance()[j],
                    kMaxError);
      } else {
        EXPECT_EQ(0, variance_array.variance()[j]);
      }
    }
  }
}

// Tests gain applier.
TEST(IntelligibilityUtilsTest, TestGainApplier) {
  const int kFreqs = 10;
  const int kSamples = 100;
  const float kChangeLimit = 0.1f;
  GainApplier gain_applier(kFreqs, kChangeLimit);
  const vector<vector<complex<float>>> in_data(
      GenerateTestData(kFreqs, kSamples));
  vector<vector<complex<float>>> out_data(GenerateTestData(kFreqs, kSamples));
  for (int i = 0; i < kSamples; i++) {
    gain_applier.Apply(&in_data[i][0], &out_data[i][0]);
    for (int j = 0; j < kFreqs; j++) {
      EXPECT_GT(out_data[i][j].real(), 0.0f);
      EXPECT_LT(out_data[i][j].real(), 1.0f);
      EXPECT_GT(out_data[i][j].imag(), 0.0f);
      EXPECT_LT(out_data[i][j].imag(), 1.0f);
    }
  }
}

}  // namespace intelligibility

}  // namespace webrtc
