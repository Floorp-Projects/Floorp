/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"

namespace webrtc {

class AudioEncoderOpusTest : public ::testing::Test {
 protected:
  // The constructor simply creates an Opus encoder with default configuration.
  AudioEncoderOpusTest()
      : opus_(new AudioEncoderOpus(AudioEncoderOpus::Config())) {}

  // Repeatedly sets packet loss rates in the range [from, to], increasing by
  // 0.01 in each step. The function verifies that the actual loss rate is
  // |expected_return|.
  void TestSetPacketLossRate(double from, double to, double expected_return) {
    ASSERT_TRUE(opus_);
    for (double loss = from; loss <= to;
         (to >= from) ? loss += 0.01 : loss -= 0.01) {
      opus_->SetProjectedPacketLossRate(loss);
      EXPECT_DOUBLE_EQ(expected_return, opus_->packet_loss_rate());
    }
  }

  rtc::scoped_ptr<AudioEncoderOpus> opus_;
};

namespace {
// These constants correspond to those used in
// AudioEncoderOpus::SetProjectedPacketLossRate.
const double kPacketLossRate20 = 0.20;
const double kPacketLossRate10 = 0.10;
const double kPacketLossRate5 = 0.05;
const double kPacketLossRate1 = 0.01;
const double kLossRate20Margin = 0.02;
const double kLossRate10Margin = 0.01;
const double kLossRate5Margin = 0.01;
}  // namespace

TEST_F(AudioEncoderOpusTest, PacketLossRateOptimized) {
  // Note that the order of the following calls is critical.
  TestSetPacketLossRate(0.0, 0.0, 0.0);
  TestSetPacketLossRate(kPacketLossRate1,
                        kPacketLossRate5 + kLossRate5Margin - 0.01,
                        kPacketLossRate1);
  TestSetPacketLossRate(kPacketLossRate5 + kLossRate5Margin,
                        kPacketLossRate10 + kLossRate10Margin - 0.01,
                        kPacketLossRate5);
  TestSetPacketLossRate(kPacketLossRate10 + kLossRate10Margin,
                        kPacketLossRate20 + kLossRate20Margin - 0.01,
                        kPacketLossRate10);
  TestSetPacketLossRate(kPacketLossRate20 + kLossRate20Margin,
                        1.0,
                        kPacketLossRate20);
  TestSetPacketLossRate(kPacketLossRate20 + kLossRate20Margin,
                        kPacketLossRate20 - kLossRate20Margin,
                        kPacketLossRate20);
  TestSetPacketLossRate(kPacketLossRate20 - kLossRate20Margin - 0.01,
                        kPacketLossRate10 - kLossRate10Margin,
                        kPacketLossRate10);
  TestSetPacketLossRate(kPacketLossRate10 - kLossRate10Margin - 0.01,
                        kPacketLossRate5 - kLossRate5Margin,
                        kPacketLossRate5);
  TestSetPacketLossRate(kPacketLossRate5 - kLossRate5Margin - 0.01,
                        kPacketLossRate1,
                        kPacketLossRate1);
  TestSetPacketLossRate(0.0, 0.0, 0.0);
}

}  // namespace webrtc
