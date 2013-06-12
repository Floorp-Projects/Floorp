/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"
#include "webrtc/modules/audio_coding/neteq4/neteq_impl.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_audio_decoder.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_buffer_level_filter.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_decoder_database.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_delay_manager.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_delay_peak_detector.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_dtmf_buffer.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_dtmf_tone_generator.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_packet_buffer.h"
#include "webrtc/modules/audio_coding/neteq4/mock/mock_payload_splitter.h"
#include "webrtc/modules/audio_coding/neteq4/timestamp_scaler.h"

using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::WithArg;

namespace webrtc {

// This function is called when inserting a packet list into the mock packet
// buffer. The purpose is to delete all inserted packets properly, to avoid
// memory leaks in the test.
int DeletePacketsAndReturnOk(PacketList* packet_list) {
  PacketBuffer::DeleteAllPackets(packet_list);
  return PacketBuffer::kOK;
}

class NetEqImplTest : public ::testing::Test {
 protected:
  static const int kInitSampleRateHz = 8000;
  NetEqImplTest() {
    buffer_level_filter_ = new MockBufferLevelFilter;
    decoder_database_ = new MockDecoderDatabase;
    delay_peak_detector_ = new MockDelayPeakDetector;
    EXPECT_CALL(*delay_peak_detector_, Reset()).Times(1);
    delay_manager_ = new MockDelayManager(NetEq::kMaxNumPacketsInBuffer,
                                          delay_peak_detector_);
    dtmf_buffer_ = new MockDtmfBuffer(kInitSampleRateHz);
    dtmf_tone_generator_ = new MockDtmfToneGenerator;
    packet_buffer_ = new MockPacketBuffer(NetEq::kMaxNumPacketsInBuffer,
                                          NetEq::kMaxBytesInBuffer);
    payload_splitter_ = new MockPayloadSplitter;
    timestamp_scaler_ = new TimestampScaler(*decoder_database_);
    EXPECT_CALL(*decoder_database_, GetActiveCngDecoder())
        .WillOnce(ReturnNull());
    neteq_ = new NetEqImpl(kInitSampleRateHz,
                           buffer_level_filter_,
                           decoder_database_,
                           delay_manager_,
                           delay_peak_detector_,
                           dtmf_buffer_,
                           dtmf_tone_generator_,
                           packet_buffer_,
                           payload_splitter_,
                           timestamp_scaler_);
  }

  virtual ~NetEqImplTest() {
    EXPECT_CALL(*buffer_level_filter_, Die()).Times(1);
    EXPECT_CALL(*decoder_database_, Die()).Times(1);
    EXPECT_CALL(*delay_manager_, Die()).Times(1);
    EXPECT_CALL(*delay_peak_detector_, Die()).Times(1);
    EXPECT_CALL(*dtmf_buffer_, Die()).Times(1);
    EXPECT_CALL(*dtmf_tone_generator_, Die()).Times(1);
    EXPECT_CALL(*packet_buffer_, Die()).Times(1);
    delete neteq_;
  }

  NetEqImpl* neteq_;
  MockBufferLevelFilter* buffer_level_filter_;
  MockDecoderDatabase* decoder_database_;
  MockDelayPeakDetector* delay_peak_detector_;
  MockDelayManager* delay_manager_;
  MockDtmfBuffer* dtmf_buffer_;
  MockDtmfToneGenerator* dtmf_tone_generator_;
  MockPacketBuffer* packet_buffer_;
  MockPayloadSplitter* payload_splitter_;
  TimestampScaler* timestamp_scaler_;
};


// This tests the interface class NetEq.
// TODO(hlundin): Move to separate file?
TEST(NetEq, CreateAndDestroy) {
  NetEq* neteq = NetEq::Create(8000);
  delete neteq;
}

TEST_F(NetEqImplTest, RegisterPayloadType) {
  uint8_t rtp_payload_type = 0;
  NetEqDecoder codec_type = kDecoderPCMu;
  EXPECT_CALL(*decoder_database_,
              RegisterPayload(rtp_payload_type, codec_type));
  neteq_->RegisterPayloadType(codec_type, rtp_payload_type);
}

TEST_F(NetEqImplTest, RemovePayloadType) {
  uint8_t rtp_payload_type = 0;
  EXPECT_CALL(*decoder_database_,
              Remove(rtp_payload_type))
      .WillOnce(Return(DecoderDatabase::kDecoderNotFound));
  // Check that kFail is returned when database returns kDecoderNotFound.
  EXPECT_EQ(NetEq::kFail, neteq_->RemovePayloadType(rtp_payload_type));
}

TEST_F(NetEqImplTest, InsertPacket) {
  const int kPayloadLength = 100;
  const uint8_t kPayloadType = 0;
  const uint16_t kFirstSequenceNumber = 0x1234;
  const uint32_t kFirstTimestamp = 0x12345678;
  const uint32_t kSsrc = 0x87654321;
  const uint32_t kFirstReceiveTime = 17;
  uint8_t payload[kPayloadLength] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = kFirstSequenceNumber;
  rtp_header.header.timestamp = kFirstTimestamp;
  rtp_header.header.ssrc = kSsrc;

  // Create a mock decoder object.
  MockAudioDecoder mock_decoder;
  // BWE update function called with first packet.
  EXPECT_CALL(mock_decoder, IncomingPacket(_,
                                           kPayloadLength,
                                           kFirstSequenceNumber,
                                           kFirstTimestamp,
                                           kFirstReceiveTime));
  // BWE update function called with second packet.
  EXPECT_CALL(mock_decoder, IncomingPacket(_,
                                           kPayloadLength,
                                           kFirstSequenceNumber + 1,
                                           kFirstTimestamp + 160,
                                           kFirstReceiveTime + 155));
  EXPECT_CALL(mock_decoder, Die()).Times(1);  // Called when deleted.

  // Expectations for decoder database.
  EXPECT_CALL(*decoder_database_, IsRed(kPayloadType))
      .WillRepeatedly(Return(false));  // This is not RED.
  EXPECT_CALL(*decoder_database_, CheckPayloadTypes(_))
      .Times(2)
      .WillRepeatedly(Return(DecoderDatabase::kOK));  // Payload type is valid.
  EXPECT_CALL(*decoder_database_, IsDtmf(kPayloadType))
      .WillRepeatedly(Return(false));  // This is not DTMF.
  EXPECT_CALL(*decoder_database_, GetDecoder(kPayloadType))
      .Times(2)
      .WillRepeatedly(Return(&mock_decoder));
  EXPECT_CALL(*decoder_database_, IsComfortNoise(kPayloadType))
      .WillRepeatedly(Return(false));  // This is not CNG.
  DecoderDatabase::DecoderInfo info;
  info.codec_type = kDecoderPCMu;
  EXPECT_CALL(*decoder_database_, GetDecoderInfo(kPayloadType))
      .WillRepeatedly(Return(&info));

  // Expectations for packet buffer.
  EXPECT_CALL(*packet_buffer_, NumPacketsInBuffer())
      .WillOnce(Return(0))   // First packet.
      .WillOnce(Return(1))   // Second packet.
      .WillOnce(Return(2));  // Second packet, checking after it was inserted.
  EXPECT_CALL(*packet_buffer_, Flush())
      .Times(1);
  EXPECT_CALL(*packet_buffer_, InsertPacketList(_, _, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<2>(kPayloadType),
                            WithArg<0>(Invoke(DeletePacketsAndReturnOk))));
  // SetArgPointee<2>(kPayloadType) means that the third argument (zero-based
  // index) is a pointer, and the variable pointed to is set to kPayloadType.
  // Also invoke the function DeletePacketsAndReturnOk to properly delete all
  // packets in the list (to avoid memory leaks in the test).

  // Expectations for DTMF buffer.
  EXPECT_CALL(*dtmf_buffer_, Flush())
      .Times(1);

  // Expectations for delay manager.
  {
    // All expectations within this block must be called in this specific order.
    InSequence sequence;  // Dummy variable.
    // Expectations when the first packet is inserted.
    EXPECT_CALL(*delay_manager_, LastDecoderType(kDecoderPCMu))
        .Times(1);
    EXPECT_CALL(*delay_manager_, last_pack_cng_or_dtmf())
        .Times(2)
        .WillRepeatedly(Return(-1));
    EXPECT_CALL(*delay_manager_, set_last_pack_cng_or_dtmf(0))
        .Times(1);
    EXPECT_CALL(*delay_manager_, ResetPacketIatCount()).Times(1);
    // Expectations when the second packet is inserted. Slightly different.
    EXPECT_CALL(*delay_manager_, LastDecoderType(kDecoderPCMu))
        .Times(1);
    EXPECT_CALL(*delay_manager_, last_pack_cng_or_dtmf())
        .WillOnce(Return(0));
  }

  // Expectations for payload splitter.
  EXPECT_CALL(*payload_splitter_, SplitAudio(_, _))
      .Times(2)
      .WillRepeatedly(Return(PayloadSplitter::kOK));

  // Insert first packet.
  neteq_->InsertPacket(rtp_header, payload, kPayloadLength, kFirstReceiveTime);

  // Insert second packet.
  rtp_header.header.timestamp += 160;
  rtp_header.header.sequenceNumber += 1;
  neteq_->InsertPacket(rtp_header, payload, kPayloadLength,
                       kFirstReceiveTime + 155);
}

}  // namespace webrtc
