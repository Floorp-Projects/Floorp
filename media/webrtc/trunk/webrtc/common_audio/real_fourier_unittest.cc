/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/real_fourier.h"

#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

using std::complex;

TEST(RealFourierStaticsTest, AllocatorAlignment) {
  {
    RealFourier::fft_real_scoper real;
    real = RealFourier::AllocRealBuffer(3);
    ASSERT_TRUE(real.get() != nullptr);
    int64_t ptr_value = reinterpret_cast<int64_t>(real.get());
    ASSERT_EQ(ptr_value % RealFourier::kFftBufferAlignment, 0);
  }
  {
    RealFourier::fft_cplx_scoper cplx;
    cplx = RealFourier::AllocCplxBuffer(3);
    ASSERT_TRUE(cplx.get() != nullptr);
    int64_t ptr_value = reinterpret_cast<int64_t>(cplx.get());
    ASSERT_EQ(ptr_value % RealFourier::kFftBufferAlignment, 0);
  }
}

TEST(RealFourierStaticsTest, OrderComputation) {
  ASSERT_EQ(RealFourier::FftOrder(2000000), -1);
  ASSERT_EQ(RealFourier::FftOrder((1 << RealFourier::kMaxFftOrder) + 1), -1);
  ASSERT_EQ(RealFourier::FftOrder(1 << RealFourier::kMaxFftOrder),
            RealFourier::kMaxFftOrder);
  ASSERT_EQ(RealFourier::FftOrder(13), 4);
  ASSERT_EQ(RealFourier::FftOrder(32), 5);
  ASSERT_EQ(RealFourier::FftOrder(2), 1);
  ASSERT_EQ(RealFourier::FftOrder(1), 0);
  ASSERT_EQ(RealFourier::FftOrder(0), 0);
}

TEST(RealFourierStaticsTest, ComplexLengthComputation) {
  ASSERT_EQ(RealFourier::ComplexLength(1), 2);
  ASSERT_EQ(RealFourier::ComplexLength(2), 3);
  ASSERT_EQ(RealFourier::ComplexLength(3), 5);
  ASSERT_EQ(RealFourier::ComplexLength(4), 9);
  ASSERT_EQ(RealFourier::ComplexLength(5), 17);
  ASSERT_EQ(RealFourier::ComplexLength(7), 65);
}

class RealFourierTest : public ::testing::Test {
 protected:
  RealFourierTest()
      : rf_(new RealFourier(2)),
        real_buffer_(RealFourier::AllocRealBuffer(4)),
        cplx_buffer_(RealFourier::AllocCplxBuffer(3)) {}

  ~RealFourierTest() {
    delete rf_;
  }

  const RealFourier* rf_;
  const RealFourier::fft_real_scoper real_buffer_;
  const RealFourier::fft_cplx_scoper cplx_buffer_;
};

TEST_F(RealFourierTest, SimpleForwardTransform) {
  real_buffer_[0] = 1.0f;
  real_buffer_[1] = 2.0f;
  real_buffer_[2] = 3.0f;
  real_buffer_[3] = 4.0f;

  rf_->Forward(real_buffer_.get(), cplx_buffer_.get());

  ASSERT_NEAR(cplx_buffer_[0].real(), 10.0f, 1e-8f);
  ASSERT_NEAR(cplx_buffer_[0].imag(), 0.0f, 1e-8f);
  ASSERT_NEAR(cplx_buffer_[1].real(), -2.0f, 1e-8f);
  ASSERT_NEAR(cplx_buffer_[1].imag(), 2.0f, 1e-8f);
  ASSERT_NEAR(cplx_buffer_[2].real(), -2.0f, 1e-8f);
  ASSERT_NEAR(cplx_buffer_[2].imag(), 0.0f, 1e-8f);
}

TEST_F(RealFourierTest, SimpleBackwardTransform) {
  cplx_buffer_[0] = complex<float>(10.0f, 0.0f);
  cplx_buffer_[1] = complex<float>(-2.0f, 2.0f);
  cplx_buffer_[2] = complex<float>(-2.0f, 0.0f);

  rf_->Inverse(cplx_buffer_.get(), real_buffer_.get());

  ASSERT_NEAR(real_buffer_[0], 1.0f, 1e-8f);
  ASSERT_NEAR(real_buffer_[1], 2.0f, 1e-8f);
  ASSERT_NEAR(real_buffer_[2], 3.0f, 1e-8f);
  ASSERT_NEAR(real_buffer_[3], 4.0f, 1e-8f);
}

}  // namespace webrtc

