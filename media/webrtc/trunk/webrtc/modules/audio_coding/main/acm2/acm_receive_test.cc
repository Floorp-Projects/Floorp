/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/acm_receive_test.h"

#include <assert.h>
#include <stdio.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/neteq/tools/audio_sink.h"
#include "webrtc/modules/audio_coding/neteq/tools/packet.h"
#include "webrtc/modules/audio_coding/neteq/tools/packet_source.h"

namespace webrtc {
namespace test {

AcmReceiveTest::AcmReceiveTest(PacketSource* packet_source,
                               AudioSink* audio_sink,
                               int output_freq_hz,
                               NumOutputChannels exptected_output_channels)
    : clock_(0),
      packet_source_(packet_source),
      audio_sink_(audio_sink),
      output_freq_hz_(output_freq_hz),
      exptected_output_channels_(exptected_output_channels) {
  webrtc::AudioCoding::Config config;
  config.clock = &clock_;
  config.playout_frequency_hz = output_freq_hz_;
  acm_.reset(webrtc::AudioCoding::Create(config));
}

void AcmReceiveTest::RegisterDefaultCodecs() {
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kOpus, 120));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kISAC, 103));
#ifndef WEBRTC_ANDROID
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kISACSWB, 104));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kISACFB, 105));
#endif
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16B, 107));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16Bwb, 108));
  ASSERT_TRUE(
      acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16Bswb32kHz, 109));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16B_2ch, 111));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16Bwb_2ch, 112));
  ASSERT_TRUE(
      acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16Bswb32kHz_2ch, 113));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCMU, 0));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCMA, 8));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCMU_2ch, 110));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCMA_2ch, 118));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kILBC, 102));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kG722, 9));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kG722_2ch, 119));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kCNNB, 13));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kCNWB, 98));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kCNSWB, 99));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kRED, 127));
}

void AcmReceiveTest::RegisterNetEqTestCodecs() {
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kISAC, 103));
#ifndef WEBRTC_ANDROID
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kISACSWB, 104));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kISACFB, 124));
#endif
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16B, 93));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16Bwb, 94));
  ASSERT_TRUE(
      acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCM16Bswb32kHz, 95));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCMU, 0));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kPCMA, 8));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kILBC, 102));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kG722, 9));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kCNNB, 13));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kCNWB, 98));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kCNSWB, 99));
  ASSERT_TRUE(acm_->RegisterReceiveCodec(acm2::ACMCodecDB::kRED, 117));
}

void AcmReceiveTest::Run() {
  for (scoped_ptr<Packet> packet(packet_source_->NextPacket()); packet;
       packet.reset(packet_source_->NextPacket())) {
    // Pull audio until time to insert packet.
    while (clock_.TimeInMilliseconds() < packet->time_ms()) {
      AudioFrame output_frame;
      EXPECT_TRUE(acm_->Get10MsAudio(&output_frame));
      EXPECT_EQ(output_freq_hz_, output_frame.sample_rate_hz_);
      const int samples_per_block = output_freq_hz_ * 10 / 1000;
      EXPECT_EQ(samples_per_block, output_frame.samples_per_channel_);
      if (exptected_output_channels_ != kArbitraryChannels) {
        if (output_frame.speech_type_ == webrtc::AudioFrame::kPLC) {
          // Don't check number of channels for PLC output, since each test run
          // usually starts with a short period of mono PLC before decoding the
          // first packet.
        } else {
          EXPECT_EQ(exptected_output_channels_, output_frame.num_channels_);
        }
      }
      ASSERT_TRUE(audio_sink_->WriteAudioFrame(output_frame));
      clock_.AdvanceTimeMilliseconds(10);
    }

    // Insert packet after converting from RTPHeader to WebRtcRTPHeader.
    WebRtcRTPHeader header;
    header.header = packet->header();
    header.frameType = kAudioFrameSpeech;
    memset(&header.type.Audio, 0, sizeof(RTPAudioHeader));
    EXPECT_TRUE(
        acm_->InsertPacket(packet->payload(),
                           static_cast<int32_t>(packet->payload_length_bytes()),
                           header))
        << "Failure when inserting packet:" << std::endl
        << "  PT = " << static_cast<int>(header.header.payloadType) << std::endl
        << "  TS = " << header.header.timestamp << std::endl
        << "  SN = " << header.header.sequenceNumber;
  }
}

}  // namespace test
}  // namespace webrtc
