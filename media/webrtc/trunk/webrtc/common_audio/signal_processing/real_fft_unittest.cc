/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/signal_processing/include/real_fft.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/typedefs.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {
namespace {

const int kOrder = 4;
const int kLength = 1 << (kOrder + 1);  // +1 to hold complex data.
const int16_t kRefData[kLength] = {
  11739, 6848, -8688, 31980, -30295, 25242, 27085, 19410,
  -26299, 15607, -10791, 11778, -23819, 14498, -25772, 10076,
  1173, 6848, -8688, 31980, -30295, 2522, 27085, 19410,
  -2629, 5607, -3, 1178, -23819, 1498, -25772, 10076
};

class RealFFTTest : public ::testing::Test {
 protected:
   RealFFTTest() {
     WebRtcSpl_Init();
   }
};

TEST_F(RealFFTTest, CreateFailsOnBadInput) {
  RealFFT* fft = WebRtcSpl_CreateRealFFT(11);
  EXPECT_TRUE(fft == NULL);
  fft = WebRtcSpl_CreateRealFFT(-1);
  EXPECT_TRUE(fft == NULL);
}

// TODO(andrew): This won't always be the case, but verifies the current code
// at least.
TEST_F(RealFFTTest, RealAndComplexAreIdentical) {
  int16_t real_data[kLength] = {0};
  int16_t real_data_out[kLength] = {0};
  int16_t complex_data[kLength] = {0};
  memcpy(real_data, kRefData, sizeof(kRefData));
  memcpy(complex_data, kRefData, sizeof(kRefData));

  RealFFT* fft = WebRtcSpl_CreateRealFFT(kOrder);
  EXPECT_TRUE(fft != NULL);

  EXPECT_EQ(0, WebRtcSpl_RealForwardFFT(fft, real_data, real_data_out));
  WebRtcSpl_ComplexBitReverse(complex_data, kOrder);
  EXPECT_EQ(0, WebRtcSpl_ComplexFFT(complex_data, kOrder, 1));

  for (int i = 0; i < kLength; i++) {
    EXPECT_EQ(real_data_out[i], complex_data[i]);
  }

  memcpy(complex_data, kRefData, sizeof(kRefData));

  int real_scale = WebRtcSpl_RealInverseFFT(fft, real_data, real_data_out);
  EXPECT_GE(real_scale, 0);
  WebRtcSpl_ComplexBitReverse(complex_data, kOrder);
  int complex_scale = WebRtcSpl_ComplexIFFT(complex_data, kOrder, 1);
  EXPECT_EQ(real_scale, complex_scale);
  for (int i = 0; i < kLength; i++) {
    EXPECT_EQ(real_data_out[i], complex_data[i]);
  }
  WebRtcSpl_FreeRealFFT(fft);
}

}  // namespace
}  // namespace webrtc
