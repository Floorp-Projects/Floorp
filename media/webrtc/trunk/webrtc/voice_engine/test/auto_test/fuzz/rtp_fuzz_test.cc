/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <time.h>

#include "webrtc/test/libtest/include/bit_flip_encryption.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"

class RtpFuzzTest : public AfterStreamingFixture {
 protected:
  void BitFlipFuzzTest(float flip_probability) {
    BitFlipEncryption bit_flip_encryption(time(NULL), flip_probability);

    TEST_LOG("Starting to flip bits in RTP/RTCP packets.\n");
    voe_encrypt_->RegisterExternalEncryption(channel_, bit_flip_encryption);

    Sleep(5000);

    voe_encrypt_->DeRegisterExternalEncryption(channel_);

    TEST_LOG("Flipped %d bits. Back to normal.\n",
             static_cast<int>(bit_flip_encryption.flip_count()));
    Sleep(2000);
  }
};

TEST_F(RtpFuzzTest, VoiceEngineDealsWithASmallNumberOfTamperedRtpPackets) {
  BitFlipFuzzTest(0.00005f);
}

TEST_F(RtpFuzzTest, VoiceEngineDealsWithAMediumNumberOfTamperedRtpPackets) {
  BitFlipFuzzTest(0.0005f);
}

TEST_F(RtpFuzzTest, VoiceEngineDealsWithALargeNumberOfTamperedRtpPackets) {
  BitFlipFuzzTest(0.05f);
}

TEST_F(RtpFuzzTest, VoiceEngineDealsWithAHugeNumberOfTamperedRtpPackets) {
  BitFlipFuzzTest(0.5f);
}
