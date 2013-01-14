/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"

extern "C" {
#include "modules/audio_processing/utility/delay_estimator.h"
#include "modules/audio_processing/utility/delay_estimator_internal.h"
#include "modules/audio_processing/utility/delay_estimator_wrapper.h"
}
#include "typedefs.h"

namespace {

enum { kSpectrumSize = 65 };
// Delay history sizes.
enum { kMaxDelay = 100 };
enum { kLookahead = 10 };

class DelayEstimatorTest : public ::testing::Test {
 protected:
  DelayEstimatorTest();
  virtual void SetUp();
  virtual void TearDown();

  void Init();

  void InitBinary();

  void* handle_;
  DelayEstimator* self_;
  BinaryDelayEstimator* binary_handle_;
  int spectrum_size_;
  // Dummy input spectra.
  float far_f_[kSpectrumSize];
  float near_f_[kSpectrumSize];
  uint16_t far_u16_[kSpectrumSize];
  uint16_t near_u16_[kSpectrumSize];
};

DelayEstimatorTest::DelayEstimatorTest()
    : handle_(NULL),
      self_(NULL),
      binary_handle_(NULL),
      spectrum_size_(kSpectrumSize) {
  // Dummy input data are set with more or less arbitrary non-zero values.
  memset(far_f_, 1, sizeof(far_f_));
  memset(near_f_, 2, sizeof(near_f_));
  memset(far_u16_, 1, sizeof(far_u16_));
  memset(near_u16_, 2, sizeof(near_u16_));
}

void DelayEstimatorTest::SetUp() {
  handle_ = WebRtc_CreateDelayEstimator(kSpectrumSize, kMaxDelay, kLookahead);
  ASSERT_TRUE(handle_ != NULL);
  self_ = reinterpret_cast<DelayEstimator*>(handle_);
  binary_handle_ = self_->binary_handle;
}

void DelayEstimatorTest::TearDown() {
  WebRtc_FreeDelayEstimator(handle_);
  handle_ = NULL;
  self_ = NULL;
  binary_handle_ = NULL;
}

void DelayEstimatorTest::Init() {
  // Initialize Delay Estimator
  EXPECT_EQ(0, WebRtc_InitDelayEstimator(handle_));
  // Verify initialization.
  EXPECT_EQ(0, self_->far_spectrum_initialized);
  EXPECT_EQ(0, self_->near_spectrum_initialized);
}

void DelayEstimatorTest::InitBinary() {
  // Initialize Binary Delay Estimator
  WebRtc_InitBinaryDelayEstimator(binary_handle_);
  // Verify initialization. This does not guarantee a complete check, since
  // |last_delay| may be equal to -2 before initialization if done on the fly.
  EXPECT_EQ(-2, binary_handle_->last_delay);
}

TEST_F(DelayEstimatorTest, CorrectErrorReturnsOfWrapper) {
  // In this test we verify correct error returns on invalid API calls.

  // WebRtc_CreateDelayEstimator() should return a NULL pointer on invalid input
  // values.
  // Make sure we have a non-NULL value at start, so we can detect NULL after
  // create failure.
  void* handle = handle_;
  handle = WebRtc_CreateDelayEstimator(33, kMaxDelay, kLookahead);
  EXPECT_TRUE(handle == NULL);
  handle = handle_;
  handle = WebRtc_CreateDelayEstimator(kSpectrumSize, -1, kLookahead);
  EXPECT_TRUE(handle == NULL);
  handle = handle_;
  handle = WebRtc_CreateDelayEstimator(kSpectrumSize, kMaxDelay, -1);
  EXPECT_TRUE(handle == NULL);
  handle = handle_;
  handle = WebRtc_CreateDelayEstimator(kSpectrumSize, 0, 0);
  EXPECT_TRUE(handle == NULL);

  // WebRtc_InitDelayEstimator() should return -1 if we have a NULL pointer as
  // |handle|.
  EXPECT_EQ(-1, WebRtc_InitDelayEstimator(NULL));

  // WebRtc_DelayEstimatorProcessFloat() should return -1 if we have:
  // 1) NULL pointer as |handle|.
  // 2) NULL pointer as far-end spectrum.
  // 3) NULL pointer as near-end spectrum.
  // 4) Incorrect spectrum size.
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFloat(NULL, far_f_, near_f_,
                                                  spectrum_size_));
  // Use |handle_| which is properly created at SetUp().
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFloat(handle_, NULL, near_f_,
                                                  spectrum_size_));
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFloat(handle_, far_f_, NULL,
                                                  spectrum_size_));
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFloat(handle_, far_f_, near_f_,
                                                  spectrum_size_ + 1));

  // WebRtc_DelayEstimatorProcessFix() should return -1 if we have:
  // 1) NULL pointer as |handle|.
  // 2) NULL pointer as far-end spectrum.
  // 3) NULL pointer as near-end spectrum.
  // 4) Incorrect spectrum size.
  // 5) Too high precision in far-end spectrum (Q-domain > 15).
  // 6) Too high precision in near-end spectrum (Q-domain > 15).
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFix(NULL, far_u16_, near_u16_,
                                                spectrum_size_, 0, 0));
  // Use |handle_| which is properly created at SetUp().
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFix(handle_, NULL, near_u16_,
                                                spectrum_size_, 0, 0));
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFix(handle_, far_u16_, NULL,
                                                spectrum_size_, 0, 0));
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFix(handle_, far_u16_, near_u16_,
                                                spectrum_size_ + 1, 0, 0));
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFix(handle_, far_u16_, near_u16_,
                                                spectrum_size_, 16, 0));
  EXPECT_EQ(-1, WebRtc_DelayEstimatorProcessFix(handle_, far_u16_, near_u16_,
                                                spectrum_size_, 0, 16));

  // WebRtc_last_delay() should return -1 if we have a NULL pointer as |handle|.
  EXPECT_EQ(-1, WebRtc_last_delay(NULL));

  // Free any local memory if needed.
  WebRtc_FreeDelayEstimator(handle);
}

TEST_F(DelayEstimatorTest, InitializedSpectrumAfterProcess) {
  // In this test we verify that the mean spectra are initialized after first
  // time we call Process().

  // For floating point operations, process one frame and verify initialization
  // flag.
  Init();
  EXPECT_EQ(-2, WebRtc_DelayEstimatorProcessFloat(handle_, far_f_, near_f_,
                                                  spectrum_size_));
  EXPECT_EQ(1, self_->far_spectrum_initialized);
  EXPECT_EQ(1, self_->near_spectrum_initialized);

  // For fixed point operations, process one frame and verify initialization
  // flag.
  Init();
  EXPECT_EQ(-2, WebRtc_DelayEstimatorProcessFix(handle_, far_u16_, near_u16_,
                                                spectrum_size_, 0, 0));
  EXPECT_EQ(1, self_->far_spectrum_initialized);
  EXPECT_EQ(1, self_->near_spectrum_initialized);
}

TEST_F(DelayEstimatorTest, CorrectLastDelay) {
  // In this test we verify that we get the correct last delay upon valid call.
  // We simply process the same data until we leave the initialized state
  // (|last_delay| = -2). Then we compare the Process() output with the
  // last_delay() call.

  int last_delay = 0;
  // Floating point operations.
  Init();
  for (int i = 0; i < 200; i++) {
    last_delay = WebRtc_DelayEstimatorProcessFloat(handle_, far_f_, near_f_,
                                                   spectrum_size_);
    if (last_delay != -2) {
      EXPECT_EQ(last_delay, WebRtc_last_delay(handle_));
      break;
    }
  }
  // Verify that we have left the initialized state.
  EXPECT_NE(-2, WebRtc_last_delay(handle_));

  // Fixed point operations.
  Init();
  for (int i = 0; i < 200; i++) {
    last_delay = WebRtc_DelayEstimatorProcessFix(handle_, far_u16_, near_u16_,
                                                 spectrum_size_, 0, 0);
    if (last_delay != -2) {
      EXPECT_EQ(last_delay, WebRtc_last_delay(handle_));
      break;
    }
  }
  // Verify that we have left the initialized state.
  EXPECT_NE(-2, WebRtc_last_delay(handle_));
}

TEST_F(DelayEstimatorTest, CorrectErrorReturnsOfBinaryEstimator) {
  // In this test we verify correct output on invalid API calls to the Binary
  // Delay Estimator.

  BinaryDelayEstimator* binary_handle = binary_handle_;
  // WebRtc_CreateBinaryDelayEstimator() should return -1 if we have a NULL
  // pointer as |binary_handle| or invalid input values. Upon failure, the
  // |binary_handle| should be NULL.
  // Make sure we have a non-NULL value at start, so we can detect NULL after
  // create failure.
  binary_handle = WebRtc_CreateBinaryDelayEstimator(-1, kLookahead);
  EXPECT_TRUE(binary_handle == NULL);
  binary_handle = binary_handle_;
  binary_handle = WebRtc_CreateBinaryDelayEstimator(kMaxDelay, -1);
  EXPECT_TRUE(binary_handle == NULL);
  binary_handle = binary_handle_;
  binary_handle = WebRtc_CreateBinaryDelayEstimator(0, 0);
  EXPECT_TRUE(binary_handle == NULL);

  // TODO(bjornv): It is not feasible to force an error of
  // WebRtc_ProcessBinarySpectrum(). This can only happen if we have more than
  // 32 bits in our binary spectrum comparison, which by definition can't
  // happen.
  // We should therefore remove that option from the code.

  // WebRtc_binary_last_delay() can't return -1 either.
}

TEST_F(DelayEstimatorTest, MeanEstimatorFix) {
  // In this test we verify that we update the mean value in correct direction
  // only. With "direction" we mean increase or decrease.

  InitBinary();

  int32_t mean_value = 4000;
  int32_t mean_value_before = mean_value;
  int32_t new_mean_value = mean_value * 2;

  // Increasing |mean_value|.
  WebRtc_MeanEstimatorFix(new_mean_value, 10, &mean_value);
  EXPECT_LT(mean_value_before, mean_value);
  EXPECT_GT(new_mean_value, mean_value);

  // Decreasing |mean_value|.
  new_mean_value = mean_value / 2;
  mean_value_before = mean_value;
  WebRtc_MeanEstimatorFix(new_mean_value, 10, &mean_value);
  EXPECT_GT(mean_value_before, mean_value);
  EXPECT_LT(new_mean_value, mean_value);
}

TEST_F(DelayEstimatorTest, ExactDelayEstimate) {
  // In this test we verify that we get the correct delay estimate if we shift
  // the signal accordingly. We verify both causal and non-causal delays.

  // Construct a sequence of binary spectra used to verify delay estimate. The
  // |sequence_length| has to be long enough for the delay estimation to leave
  // the initialized state.
  const int sequence_length = 400;
  uint32_t binary_spectrum[sequence_length + kMaxDelay + kLookahead];
  binary_spectrum[0] = 1;
  for (int i = 1; i < (sequence_length + kMaxDelay + kLookahead); i++) {
    binary_spectrum[i] = 3 * binary_spectrum[i - 1];
  }

  // Verify the delay for both causal and non-causal systems. For causal systems
  // the delay is equivalent with a positive |offset| of the far-end sequence.
  // For non-causal systems the delay is equivalent with a negative |offset| of
  // the far-end sequence.
  for (int offset = -kLookahead; offset < kMaxDelay; offset++) {
    InitBinary();
    for (int i = kLookahead; i < (sequence_length + kLookahead); i++) {
      int delay = WebRtc_ProcessBinarySpectrum(binary_handle_,
                                               binary_spectrum[i + offset],
                                               binary_spectrum[i]);

      // Verify that we WebRtc_binary_last_delay() returns correct delay.
      EXPECT_EQ(delay, WebRtc_binary_last_delay(binary_handle_));

      if (delay != -2) {
        // Verify correct delay estimate. In the non-causal case the true delay
        // is equivalent with the |offset|.
        EXPECT_EQ(offset, delay - kLookahead);
      }
    }
    // Verify that we have left the initialized state.
    EXPECT_NE(-2, WebRtc_binary_last_delay(binary_handle_));
  }
}

}  // namespace
