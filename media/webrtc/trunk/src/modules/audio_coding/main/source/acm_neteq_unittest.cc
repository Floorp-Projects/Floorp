/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains unit tests for ACM's NetEQ wrapper (class ACMNetEQ).

#include <stdlib.h>

#include "gtest/gtest.h"
#include "modules/audio_coding/codecs/pcm16b/include/pcm16b.h"
#include "modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "modules/audio_coding/main/source/acm_codec_database.h"
#include "modules/audio_coding/main/source/acm_neteq.h"
#include "modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "modules/interface/module_common_types.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class AcmNetEqTest : public ::testing::Test {
 protected:
  static const size_t kMaxPayloadLen = 5760;  // 60 ms, 48 kHz, 16 bit samples.
  static const int kPcm16WbPayloadType = 94;
  AcmNetEqTest() {}
  virtual void SetUp();
  virtual void TearDown() {}

  void InsertZeroPacket(uint16_t sequence_number,
                        uint32_t timestamp,
                        uint8_t payload_type,
                        uint32_t ssrc,
                        bool marker_bit,
                        size_t len_payload_bytes);
  void PullData(int expected_num_samples);

  ACMNetEQ neteq_;
};

void AcmNetEqTest::SetUp() {
  ASSERT_EQ(0, neteq_.Init());
  ASSERT_EQ(0, neteq_.AllocatePacketBuffer(ACMCodecDB::NetEQDecoders(),
                                           ACMCodecDB::kNumCodecs));
  WebRtcNetEQ_CodecDef codec_def;
  SET_CODEC_PAR(codec_def, kDecoderPCM16Bwb, kPcm16WbPayloadType, NULL, 16000);
  SET_PCM16B_WB_FUNCTIONS(codec_def);
  ASSERT_EQ(0, neteq_.AddCodec(&codec_def, true));
}

void AcmNetEqTest::InsertZeroPacket(uint16_t sequence_number,
                                    uint32_t timestamp,
                                    uint8_t payload_type,
                                    uint32_t ssrc,
                                    bool marker_bit,
                                    size_t len_payload_bytes) {
  ASSERT_TRUE(len_payload_bytes <= kMaxPayloadLen);
  uint16_t payload[kMaxPayloadLen] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.sequenceNumber = sequence_number;
  rtp_header.header.timestamp = timestamp;
  rtp_header.header.ssrc = ssrc;
  rtp_header.header.payloadType = payload_type;
  rtp_header.header.markerBit = marker_bit;
  rtp_header.type.Audio.channel = 1;
  ASSERT_EQ(0, neteq_.RecIn(reinterpret_cast<WebRtc_UWord8*>(payload),
                            len_payload_bytes, rtp_header));
}

void AcmNetEqTest::PullData(int expected_num_samples) {
  AudioFrame out_frame;
  ASSERT_EQ(0, neteq_.RecOut(out_frame));
  ASSERT_EQ(expected_num_samples, out_frame._payloadDataLengthInSamples);
}

TEST_F(AcmNetEqTest, NetworkStatistics) {
  // Use fax mode to avoid time-scaling. This is to simplify the testing of
  // packet waiting times in the packet buffer.
  neteq_.SetPlayoutMode(fax);
  // Insert 31 dummy packets at once. Each packet contains 10 ms 16 kHz audio.
  int num_frames = 30;
  const int kSamples = 10 * 16;
  const int kPayloadBytes = kSamples * 2;
  int i, j;
  for (i = 0; i < num_frames; ++i) {
    InsertZeroPacket(i, i * kSamples, kPcm16WbPayloadType, 0x1234, false,
                     kPayloadBytes);
  }
  // Pull out data once.
  PullData(kSamples);
  // Insert one more packet (to produce different mean and median).
  i = num_frames;
  InsertZeroPacket(i, i * kSamples, kPcm16WbPayloadType, 0x1234, false,
                   kPayloadBytes);
  // Pull out all data.
  for (j = 1; j < num_frames + 1; ++j) {
    PullData(kSamples);
  }

  ACMNetworkStatistics stats;
  ASSERT_EQ(0, neteq_.NetworkStatistics(&stats));
  EXPECT_EQ(0, stats.currentBufferSize);
  EXPECT_EQ(0, stats.preferredBufferSize);
  EXPECT_FALSE(stats.jitterPeaksFound);
  EXPECT_EQ(0, stats.currentPacketLossRate);
  EXPECT_EQ(0, stats.currentDiscardRate);
  EXPECT_EQ(0, stats.currentExpandRate);
  EXPECT_EQ(0, stats.currentPreemptiveRate);
  EXPECT_EQ(0, stats.currentAccelerateRate);
  EXPECT_EQ(-916, stats.clockDriftPPM);  // Initial value is slightly off.
  EXPECT_EQ(300, stats.maxWaitingTimeMs);
  EXPECT_EQ(10, stats.minWaitingTimeMs);
  EXPECT_EQ(159, stats.meanWaitingTimeMs);
  EXPECT_EQ(160, stats.medianWaitingTimeMs);
}

TEST_F(AcmNetEqTest, TestZeroLengthWaitingTimesVector) {
  // Insert one packet.
  const int kSamples = 10 * 16;
  const int kPayloadBytes = kSamples * 2;
  int i = 0;
  InsertZeroPacket(i, i * kSamples, kPcm16WbPayloadType, 0x1234, false,
                   kPayloadBytes);
  // Do not pull out any data.

  ACMNetworkStatistics stats;
  ASSERT_EQ(0, neteq_.NetworkStatistics(&stats));
  EXPECT_EQ(0, stats.currentBufferSize);
  EXPECT_EQ(0, stats.preferredBufferSize);
  EXPECT_FALSE(stats.jitterPeaksFound);
  EXPECT_EQ(0, stats.currentPacketLossRate);
  EXPECT_EQ(0, stats.currentDiscardRate);
  EXPECT_EQ(0, stats.currentExpandRate);
  EXPECT_EQ(0, stats.currentPreemptiveRate);
  EXPECT_EQ(0, stats.currentAccelerateRate);
  EXPECT_EQ(-916, stats.clockDriftPPM);  // Initial value is slightly off.
  EXPECT_EQ(-1, stats.minWaitingTimeMs);
  EXPECT_EQ(-1, stats.maxWaitingTimeMs);
  EXPECT_EQ(-1, stats.meanWaitingTimeMs);
  EXPECT_EQ(-1, stats.medianWaitingTimeMs);
}

}  // namespace
