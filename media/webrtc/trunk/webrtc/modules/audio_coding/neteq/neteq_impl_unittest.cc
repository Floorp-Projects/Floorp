/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/interface/neteq.h"
#include "webrtc/modules/audio_coding/neteq/neteq_impl.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/neteq/accelerate.h"
#include "webrtc/modules/audio_coding/neteq/expand.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_audio_decoder.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_buffer_level_filter.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_decoder_database.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_delay_manager.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_delay_peak_detector.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_dtmf_buffer.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_dtmf_tone_generator.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_packet_buffer.h"
#include "webrtc/modules/audio_coding/neteq/mock/mock_payload_splitter.h"
#include "webrtc/modules/audio_coding/neteq/preemptive_expand.h"
#include "webrtc/modules/audio_coding/neteq/sync_buffer.h"
#include "webrtc/modules/audio_coding/neteq/timestamp_scaler.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::SetArrayArgument;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::WithArg;
using ::testing::Pointee;
using ::testing::IsNull;

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
  NetEqImplTest()
      : neteq_(NULL),
        config_(),
        mock_buffer_level_filter_(NULL),
        buffer_level_filter_(NULL),
        use_mock_buffer_level_filter_(true),
        mock_decoder_database_(NULL),
        decoder_database_(NULL),
        use_mock_decoder_database_(true),
        mock_delay_peak_detector_(NULL),
        delay_peak_detector_(NULL),
        use_mock_delay_peak_detector_(true),
        mock_delay_manager_(NULL),
        delay_manager_(NULL),
        use_mock_delay_manager_(true),
        mock_dtmf_buffer_(NULL),
        dtmf_buffer_(NULL),
        use_mock_dtmf_buffer_(true),
        mock_dtmf_tone_generator_(NULL),
        dtmf_tone_generator_(NULL),
        use_mock_dtmf_tone_generator_(true),
        mock_packet_buffer_(NULL),
        packet_buffer_(NULL),
        use_mock_packet_buffer_(true),
        mock_payload_splitter_(NULL),
        payload_splitter_(NULL),
        use_mock_payload_splitter_(true),
        timestamp_scaler_(NULL) {
    config_.sample_rate_hz = 8000;
  }

  void CreateInstance() {
    if (use_mock_buffer_level_filter_) {
      mock_buffer_level_filter_ = new MockBufferLevelFilter;
      buffer_level_filter_ = mock_buffer_level_filter_;
    } else {
      buffer_level_filter_ = new BufferLevelFilter;
    }
    if (use_mock_decoder_database_) {
      mock_decoder_database_ = new MockDecoderDatabase;
      EXPECT_CALL(*mock_decoder_database_, GetActiveCngDecoder())
          .WillOnce(ReturnNull());
      decoder_database_ = mock_decoder_database_;
    } else {
      decoder_database_ = new DecoderDatabase;
    }
    if (use_mock_delay_peak_detector_) {
      mock_delay_peak_detector_ = new MockDelayPeakDetector;
      EXPECT_CALL(*mock_delay_peak_detector_, Reset()).Times(1);
      delay_peak_detector_ = mock_delay_peak_detector_;
    } else {
      delay_peak_detector_ = new DelayPeakDetector;
    }
    if (use_mock_delay_manager_) {
      mock_delay_manager_ = new MockDelayManager(config_.max_packets_in_buffer,
                                                 delay_peak_detector_);
      EXPECT_CALL(*mock_delay_manager_, set_streaming_mode(false)).Times(1);
      delay_manager_ = mock_delay_manager_;
    } else {
      delay_manager_ =
          new DelayManager(config_.max_packets_in_buffer, delay_peak_detector_);
    }
    if (use_mock_dtmf_buffer_) {
      mock_dtmf_buffer_ = new MockDtmfBuffer(config_.sample_rate_hz);
      dtmf_buffer_ = mock_dtmf_buffer_;
    } else {
      dtmf_buffer_ = new DtmfBuffer(config_.sample_rate_hz);
    }
    if (use_mock_dtmf_tone_generator_) {
      mock_dtmf_tone_generator_ = new MockDtmfToneGenerator;
      dtmf_tone_generator_ = mock_dtmf_tone_generator_;
    } else {
      dtmf_tone_generator_ = new DtmfToneGenerator;
    }
    if (use_mock_packet_buffer_) {
      mock_packet_buffer_ = new MockPacketBuffer(config_.max_packets_in_buffer);
      packet_buffer_ = mock_packet_buffer_;
    } else {
      packet_buffer_ = new PacketBuffer(config_.max_packets_in_buffer);
    }
    if (use_mock_payload_splitter_) {
      mock_payload_splitter_ = new MockPayloadSplitter;
      payload_splitter_ = mock_payload_splitter_;
    } else {
      payload_splitter_ = new PayloadSplitter;
    }
    timestamp_scaler_ = new TimestampScaler(*decoder_database_);
    AccelerateFactory* accelerate_factory = new AccelerateFactory;
    ExpandFactory* expand_factory = new ExpandFactory;
    PreemptiveExpandFactory* preemptive_expand_factory =
        new PreemptiveExpandFactory;

    neteq_ = new NetEqImpl(config_,
                           buffer_level_filter_,
                           decoder_database_,
                           delay_manager_,
                           delay_peak_detector_,
                           dtmf_buffer_,
                           dtmf_tone_generator_,
                           packet_buffer_,
                           payload_splitter_,
                           timestamp_scaler_,
                           accelerate_factory,
                           expand_factory,
                           preemptive_expand_factory);
    ASSERT_TRUE(neteq_ != NULL);
  }

  void UseNoMocks() {
    ASSERT_TRUE(neteq_ == NULL) << "Must call UseNoMocks before CreateInstance";
    use_mock_buffer_level_filter_ = false;
    use_mock_decoder_database_ = false;
    use_mock_delay_peak_detector_ = false;
    use_mock_delay_manager_ = false;
    use_mock_dtmf_buffer_ = false;
    use_mock_dtmf_tone_generator_ = false;
    use_mock_packet_buffer_ = false;
    use_mock_payload_splitter_ = false;
  }

  virtual ~NetEqImplTest() {
    if (use_mock_buffer_level_filter_) {
      EXPECT_CALL(*mock_buffer_level_filter_, Die()).Times(1);
    }
    if (use_mock_decoder_database_) {
      EXPECT_CALL(*mock_decoder_database_, Die()).Times(1);
    }
    if (use_mock_delay_manager_) {
      EXPECT_CALL(*mock_delay_manager_, Die()).Times(1);
    }
    if (use_mock_delay_peak_detector_) {
      EXPECT_CALL(*mock_delay_peak_detector_, Die()).Times(1);
    }
    if (use_mock_dtmf_buffer_) {
      EXPECT_CALL(*mock_dtmf_buffer_, Die()).Times(1);
    }
    if (use_mock_dtmf_tone_generator_) {
      EXPECT_CALL(*mock_dtmf_tone_generator_, Die()).Times(1);
    }
    if (use_mock_packet_buffer_) {
      EXPECT_CALL(*mock_packet_buffer_, Die()).Times(1);
    }
    delete neteq_;
  }

  NetEqImpl* neteq_;
  NetEq::Config config_;
  MockBufferLevelFilter* mock_buffer_level_filter_;
  BufferLevelFilter* buffer_level_filter_;
  bool use_mock_buffer_level_filter_;
  MockDecoderDatabase* mock_decoder_database_;
  DecoderDatabase* decoder_database_;
  bool use_mock_decoder_database_;
  MockDelayPeakDetector* mock_delay_peak_detector_;
  DelayPeakDetector* delay_peak_detector_;
  bool use_mock_delay_peak_detector_;
  MockDelayManager* mock_delay_manager_;
  DelayManager* delay_manager_;
  bool use_mock_delay_manager_;
  MockDtmfBuffer* mock_dtmf_buffer_;
  DtmfBuffer* dtmf_buffer_;
  bool use_mock_dtmf_buffer_;
  MockDtmfToneGenerator* mock_dtmf_tone_generator_;
  DtmfToneGenerator* dtmf_tone_generator_;
  bool use_mock_dtmf_tone_generator_;
  MockPacketBuffer* mock_packet_buffer_;
  PacketBuffer* packet_buffer_;
  bool use_mock_packet_buffer_;
  MockPayloadSplitter* mock_payload_splitter_;
  PayloadSplitter* payload_splitter_;
  bool use_mock_payload_splitter_;
  TimestampScaler* timestamp_scaler_;
};


// This tests the interface class NetEq.
// TODO(hlundin): Move to separate file?
TEST(NetEq, CreateAndDestroy) {
  NetEq::Config config;
  NetEq* neteq = NetEq::Create(config);
  delete neteq;
}

TEST_F(NetEqImplTest, RegisterPayloadType) {
  CreateInstance();
  uint8_t rtp_payload_type = 0;
  NetEqDecoder codec_type = kDecoderPCMu;
  EXPECT_CALL(*mock_decoder_database_,
              RegisterPayload(rtp_payload_type, codec_type));
  neteq_->RegisterPayloadType(codec_type, rtp_payload_type);
}

TEST_F(NetEqImplTest, RemovePayloadType) {
  CreateInstance();
  uint8_t rtp_payload_type = 0;
  EXPECT_CALL(*mock_decoder_database_, Remove(rtp_payload_type))
      .WillOnce(Return(DecoderDatabase::kDecoderNotFound));
  // Check that kFail is returned when database returns kDecoderNotFound.
  EXPECT_EQ(NetEq::kFail, neteq_->RemovePayloadType(rtp_payload_type));
}

TEST_F(NetEqImplTest, InsertPacket) {
  CreateInstance();
  const size_t kPayloadLength = 100;
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
  EXPECT_CALL(mock_decoder, Channels()).WillRepeatedly(Return(1));
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
  EXPECT_CALL(*mock_decoder_database_, IsRed(kPayloadType))
      .WillRepeatedly(Return(false));  // This is not RED.
  EXPECT_CALL(*mock_decoder_database_, CheckPayloadTypes(_))
      .Times(2)
      .WillRepeatedly(Return(DecoderDatabase::kOK));  // Payload type is valid.
  EXPECT_CALL(*mock_decoder_database_, IsDtmf(kPayloadType))
      .WillRepeatedly(Return(false));  // This is not DTMF.
  EXPECT_CALL(*mock_decoder_database_, GetDecoder(kPayloadType))
      .Times(3)
      .WillRepeatedly(Return(&mock_decoder));
  EXPECT_CALL(*mock_decoder_database_, IsComfortNoise(kPayloadType))
      .WillRepeatedly(Return(false));  // This is not CNG.
  DecoderDatabase::DecoderInfo info;
  info.codec_type = kDecoderPCMu;
  EXPECT_CALL(*mock_decoder_database_, GetDecoderInfo(kPayloadType))
      .WillRepeatedly(Return(&info));

  // Expectations for packet buffer.
  EXPECT_CALL(*mock_packet_buffer_, NumPacketsInBuffer())
      .WillOnce(Return(0))   // First packet.
      .WillOnce(Return(1))   // Second packet.
      .WillOnce(Return(2));  // Second packet, checking after it was inserted.
  EXPECT_CALL(*mock_packet_buffer_, Empty())
      .WillOnce(Return(false));  // Called once after first packet is inserted.
  EXPECT_CALL(*mock_packet_buffer_, Flush())
      .Times(1);
  EXPECT_CALL(*mock_packet_buffer_, InsertPacketList(_, _, _, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<2>(kPayloadType),
                            WithArg<0>(Invoke(DeletePacketsAndReturnOk))));
  // SetArgPointee<2>(kPayloadType) means that the third argument (zero-based
  // index) is a pointer, and the variable pointed to is set to kPayloadType.
  // Also invoke the function DeletePacketsAndReturnOk to properly delete all
  // packets in the list (to avoid memory leaks in the test).
  EXPECT_CALL(*mock_packet_buffer_, NextRtpHeader())
      .Times(1)
      .WillOnce(Return(&rtp_header.header));

  // Expectations for DTMF buffer.
  EXPECT_CALL(*mock_dtmf_buffer_, Flush())
      .Times(1);

  // Expectations for delay manager.
  {
    // All expectations within this block must be called in this specific order.
    InSequence sequence;  // Dummy variable.
    // Expectations when the first packet is inserted.
    EXPECT_CALL(*mock_delay_manager_, LastDecoderType(kDecoderPCMu))
        .Times(1);
    EXPECT_CALL(*mock_delay_manager_, last_pack_cng_or_dtmf())
        .Times(2)
        .WillRepeatedly(Return(-1));
    EXPECT_CALL(*mock_delay_manager_, set_last_pack_cng_or_dtmf(0))
        .Times(1);
    EXPECT_CALL(*mock_delay_manager_, ResetPacketIatCount()).Times(1);
    // Expectations when the second packet is inserted. Slightly different.
    EXPECT_CALL(*mock_delay_manager_, LastDecoderType(kDecoderPCMu))
        .Times(1);
    EXPECT_CALL(*mock_delay_manager_, last_pack_cng_or_dtmf())
        .WillOnce(Return(0));
    EXPECT_CALL(*mock_delay_manager_, SetPacketAudioLength(30))
        .WillOnce(Return(0));
  }

  // Expectations for payload splitter.
  EXPECT_CALL(*mock_payload_splitter_, SplitAudio(_, _))
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

TEST_F(NetEqImplTest, InsertPacketsUntilBufferIsFull) {
  UseNoMocks();
  CreateInstance();

  const int kPayloadLengthSamples = 80;
  const size_t kPayloadLengthBytes = 2 * kPayloadLengthSamples;  // PCM 16-bit.
  const uint8_t kPayloadType = 17;  // Just an arbitrary number.
  const uint32_t kReceiveTime = 17;  // Value doesn't matter for this test.
  uint8_t payload[kPayloadLengthBytes] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = 0x1234;
  rtp_header.header.timestamp = 0x12345678;
  rtp_header.header.ssrc = 0x87654321;

  EXPECT_EQ(NetEq::kOK,
            neteq_->RegisterPayloadType(kDecoderPCM16B, kPayloadType));

  // Insert packets. The buffer should not flush.
  for (int i = 1; i <= config_.max_packets_in_buffer; ++i) {
    EXPECT_EQ(NetEq::kOK,
              neteq_->InsertPacket(
                  rtp_header, payload, kPayloadLengthBytes, kReceiveTime));
    rtp_header.header.timestamp += kPayloadLengthSamples;
    rtp_header.header.sequenceNumber += 1;
    EXPECT_EQ(i, packet_buffer_->NumPacketsInBuffer());
  }

  // Insert one more packet and make sure the buffer got flushed. That is, it
  // should only hold one single packet.
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));
  EXPECT_EQ(1, packet_buffer_->NumPacketsInBuffer());
  const RTPHeader* test_header = packet_buffer_->NextRtpHeader();
  EXPECT_EQ(rtp_header.header.timestamp, test_header->timestamp);
  EXPECT_EQ(rtp_header.header.sequenceNumber, test_header->sequenceNumber);
}

// This test verifies that timestamps propagate from the incoming packets
// through to the sync buffer and to the playout timestamp.
TEST_F(NetEqImplTest, VerifyTimestampPropagation) {
  UseNoMocks();
  CreateInstance();

  const uint8_t kPayloadType = 17;   // Just an arbitrary number.
  const uint32_t kReceiveTime = 17;  // Value doesn't matter for this test.
  const int kSampleRateHz = 8000;
  const int kPayloadLengthSamples = 10 * kSampleRateHz / 1000;  // 10 ms.
  const size_t kPayloadLengthBytes = kPayloadLengthSamples;
  uint8_t payload[kPayloadLengthBytes] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = 0x1234;
  rtp_header.header.timestamp = 0x12345678;
  rtp_header.header.ssrc = 0x87654321;

  // This is a dummy decoder that produces as many output samples as the input
  // has bytes. The output is an increasing series, starting at 1 for the first
  // sample, and then increasing by 1 for each sample.
  class CountingSamplesDecoder : public AudioDecoder {
   public:
    CountingSamplesDecoder() : next_value_(1) {}

    // Produce as many samples as input bytes (|encoded_len|).
    int Decode(const uint8_t* encoded,
               size_t encoded_len,
               int /* sample_rate_hz */,
               size_t /* max_decoded_bytes */,
               int16_t* decoded,
               SpeechType* speech_type) override {
      for (size_t i = 0; i < encoded_len; ++i) {
        decoded[i] = next_value_++;
      }
      *speech_type = kSpeech;
      return encoded_len;
    }

    virtual int Init() {
      next_value_ = 1;
      return 0;
    }

    size_t Channels() const override { return 1; }

    uint16_t next_value() const { return next_value_; }

   private:
    int16_t next_value_;
  } decoder_;

  EXPECT_EQ(NetEq::kOK,
            neteq_->RegisterExternalDecoder(
                &decoder_, kDecoderPCM16B, kPayloadType));

  // Insert one packet.
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  // Pull audio once.
  const int kMaxOutputSize = 10 * kSampleRateHz / 1000;
  int16_t output[kMaxOutputSize];
  int samples_per_channel;
  int num_channels;
  NetEqOutputType type;
  EXPECT_EQ(
      NetEq::kOK,
      neteq_->GetAudio(
          kMaxOutputSize, output, &samples_per_channel, &num_channels, &type));
  ASSERT_EQ(kMaxOutputSize, samples_per_channel);
  EXPECT_EQ(1, num_channels);
  EXPECT_EQ(kOutputNormal, type);

  // Start with a simple check that the fake decoder is behaving as expected.
  EXPECT_EQ(kPayloadLengthSamples, decoder_.next_value() - 1);

  // The value of the last of the output samples is the same as the number of
  // samples played from the decoded packet. Thus, this number + the RTP
  // timestamp should match the playout timestamp.
  uint32_t timestamp = 0;
  EXPECT_TRUE(neteq_->GetPlayoutTimestamp(&timestamp));
  EXPECT_EQ(rtp_header.header.timestamp + output[samples_per_channel - 1],
            timestamp);

  // Check the timestamp for the last value in the sync buffer. This should
  // be one full frame length ahead of the RTP timestamp.
  const SyncBuffer* sync_buffer = neteq_->sync_buffer_for_test();
  ASSERT_TRUE(sync_buffer != NULL);
  EXPECT_EQ(rtp_header.header.timestamp + kPayloadLengthSamples,
            sync_buffer->end_timestamp());

  // Check that the number of samples still to play from the sync buffer add
  // up with what was already played out.
  EXPECT_EQ(kPayloadLengthSamples - output[samples_per_channel - 1],
            static_cast<int>(sync_buffer->FutureLength()));
}

TEST_F(NetEqImplTest, ReorderedPacket) {
  UseNoMocks();
  CreateInstance();

  const uint8_t kPayloadType = 17;   // Just an arbitrary number.
  const uint32_t kReceiveTime = 17;  // Value doesn't matter for this test.
  const int kSampleRateHz = 8000;
  const int kPayloadLengthSamples = 10 * kSampleRateHz / 1000;  // 10 ms.
  const size_t kPayloadLengthBytes = kPayloadLengthSamples;
  uint8_t payload[kPayloadLengthBytes] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = 0x1234;
  rtp_header.header.timestamp = 0x12345678;
  rtp_header.header.ssrc = 0x87654321;

  // Create a mock decoder object.
  MockAudioDecoder mock_decoder;
  EXPECT_CALL(mock_decoder, Init()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_decoder, Channels()).WillRepeatedly(Return(1));
  EXPECT_CALL(mock_decoder, IncomingPacket(_, kPayloadLengthBytes, _, _, _))
      .WillRepeatedly(Return(0));
  int16_t dummy_output[kPayloadLengthSamples] = {0};
  // The below expectation will make the mock decoder write
  // |kPayloadLengthSamples| zeros to the output array, and mark it as speech.
  EXPECT_CALL(mock_decoder,
              Decode(Pointee(0), kPayloadLengthBytes, kSampleRateHz, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<4>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<5>(AudioDecoder::kSpeech),
                      Return(kPayloadLengthSamples)));
  EXPECT_EQ(NetEq::kOK,
            neteq_->RegisterExternalDecoder(
                &mock_decoder, kDecoderPCM16B, kPayloadType));

  // Insert one packet.
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  // Pull audio once.
  const int kMaxOutputSize = 10 * kSampleRateHz / 1000;
  int16_t output[kMaxOutputSize];
  int samples_per_channel;
  int num_channels;
  NetEqOutputType type;
  EXPECT_EQ(
      NetEq::kOK,
      neteq_->GetAudio(
          kMaxOutputSize, output, &samples_per_channel, &num_channels, &type));
  ASSERT_EQ(kMaxOutputSize, samples_per_channel);
  EXPECT_EQ(1, num_channels);
  EXPECT_EQ(kOutputNormal, type);

  // Insert two more packets. The first one is out of order, and is already too
  // old, the second one is the expected next packet.
  rtp_header.header.sequenceNumber -= 1;
  rtp_header.header.timestamp -= kPayloadLengthSamples;
  payload[0] = 1;
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));
  rtp_header.header.sequenceNumber += 2;
  rtp_header.header.timestamp += 2 * kPayloadLengthSamples;
  payload[0] = 2;
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  // Expect only the second packet to be decoded (the one with "2" as the first
  // payload byte).
  EXPECT_CALL(mock_decoder,
              Decode(Pointee(2), kPayloadLengthBytes, kSampleRateHz, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<4>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<5>(AudioDecoder::kSpeech),
                      Return(kPayloadLengthSamples)));

  // Pull audio once.
  EXPECT_EQ(
      NetEq::kOK,
      neteq_->GetAudio(
          kMaxOutputSize, output, &samples_per_channel, &num_channels, &type));
  ASSERT_EQ(kMaxOutputSize, samples_per_channel);
  EXPECT_EQ(1, num_channels);
  EXPECT_EQ(kOutputNormal, type);

  // Now check the packet buffer, and make sure it is empty, since the
  // out-of-order packet should have been discarded.
  EXPECT_TRUE(packet_buffer_->Empty());

  EXPECT_CALL(mock_decoder, Die());
}

// This test verifies that NetEq can handle the situation where the first
// incoming packet is rejected.
TEST_F(NetEqImplTest, FirstPacketUnknown) {
  UseNoMocks();
  CreateInstance();

  const uint8_t kPayloadType = 17;   // Just an arbitrary number.
  const uint32_t kReceiveTime = 17;  // Value doesn't matter for this test.
  const int kSampleRateHz = 8000;
  const int kPayloadLengthSamples = 10 * kSampleRateHz / 1000;  // 10 ms.
  const size_t kPayloadLengthBytes = kPayloadLengthSamples;
  uint8_t payload[kPayloadLengthBytes] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = 0x1234;
  rtp_header.header.timestamp = 0x12345678;
  rtp_header.header.ssrc = 0x87654321;

  // Insert one packet. Note that we have not registered any payload type, so
  // this packet will be rejected.
  EXPECT_EQ(NetEq::kFail,
            neteq_->InsertPacket(rtp_header, payload, kPayloadLengthBytes,
                                 kReceiveTime));
  EXPECT_EQ(NetEq::kUnknownRtpPayloadType, neteq_->LastError());

  // Pull audio once.
  const int kMaxOutputSize = 10 * kSampleRateHz / 1000;
  int16_t output[kMaxOutputSize];
  int samples_per_channel;
  int num_channels;
  NetEqOutputType type;
  EXPECT_EQ(NetEq::kOK,
            neteq_->GetAudio(kMaxOutputSize, output, &samples_per_channel,
                             &num_channels, &type));
  ASSERT_LE(samples_per_channel, kMaxOutputSize);
  EXPECT_EQ(kMaxOutputSize, samples_per_channel);
  EXPECT_EQ(1, num_channels);
  EXPECT_EQ(kOutputPLC, type);

  // Register the payload type.
  EXPECT_EQ(NetEq::kOK,
            neteq_->RegisterPayloadType(kDecoderPCM16B, kPayloadType));

  // Insert 10 packets.
  for (int i = 0; i < 10; ++i) {
    rtp_header.header.sequenceNumber++;
    rtp_header.header.timestamp += kPayloadLengthSamples;
    EXPECT_EQ(NetEq::kOK,
              neteq_->InsertPacket(rtp_header, payload, kPayloadLengthBytes,
                                   kReceiveTime));
    EXPECT_EQ(i + 1, packet_buffer_->NumPacketsInBuffer());
  }

  // Pull audio repeatedly and make sure we get normal output, that is not PLC.
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(NetEq::kOK,
              neteq_->GetAudio(kMaxOutputSize, output, &samples_per_channel,
                               &num_channels, &type));
    ASSERT_LE(samples_per_channel, kMaxOutputSize);
    EXPECT_EQ(kMaxOutputSize, samples_per_channel);
    EXPECT_EQ(1, num_channels);
    EXPECT_EQ(kOutputNormal, type)
        << "NetEq did not decode the packets as expected.";
  }
}

// This test verifies that NetEq can handle comfort noise and enters/quits codec
// internal CNG mode properly.
TEST_F(NetEqImplTest, CodecInternalCng) {
  UseNoMocks();
  CreateInstance();

  const uint8_t kPayloadType = 17;   // Just an arbitrary number.
  const uint32_t kReceiveTime = 17;  // Value doesn't matter for this test.
  const int kSampleRateKhz = 48;
  const int kPayloadLengthSamples = 20 * kSampleRateKhz;  // 20 ms.
  const int kPayloadLengthBytes = 10;
  uint8_t payload[kPayloadLengthBytes] = {0};
  int16_t dummy_output[kPayloadLengthSamples] = {0};

  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = 0x1234;
  rtp_header.header.timestamp = 0x12345678;
  rtp_header.header.ssrc = 0x87654321;

  // Create a mock decoder object.
  MockAudioDecoder mock_decoder;
  EXPECT_CALL(mock_decoder, Init()).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_decoder, Channels()).WillRepeatedly(Return(1));
  EXPECT_CALL(mock_decoder, IncomingPacket(_, kPayloadLengthBytes, _, _, _))
      .WillRepeatedly(Return(0));

  // Pointee(x) verifies that first byte of the payload equals x, this makes it
  // possible to verify that the correct payload is fed to Decode().
  EXPECT_CALL(mock_decoder, Decode(Pointee(0), kPayloadLengthBytes,
                                   kSampleRateKhz * 1000, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<4>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<5>(AudioDecoder::kSpeech),
                      Return(kPayloadLengthSamples)));

  EXPECT_CALL(mock_decoder, Decode(Pointee(1), kPayloadLengthBytes,
                                   kSampleRateKhz * 1000, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<4>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<5>(AudioDecoder::kComfortNoise),
                      Return(kPayloadLengthSamples)));

  EXPECT_CALL(mock_decoder, Decode(IsNull(), 0, kSampleRateKhz * 1000, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<4>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<5>(AudioDecoder::kComfortNoise),
                      Return(kPayloadLengthSamples)));

  EXPECT_CALL(mock_decoder, Decode(Pointee(2), kPayloadLengthBytes,
                                   kSampleRateKhz * 1000, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<4>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<5>(AudioDecoder::kSpeech),
                      Return(kPayloadLengthSamples)));

  EXPECT_EQ(NetEq::kOK,
            neteq_->RegisterExternalDecoder(
                &mock_decoder, kDecoderOpus, kPayloadType));

  // Insert one packet (decoder will return speech).
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  // Insert second packet (decoder will return CNG).
  payload[0] = 1;
  rtp_header.header.sequenceNumber++;
  rtp_header.header.timestamp += kPayloadLengthSamples;
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  const int kMaxOutputSize = 10 * kSampleRateKhz;
  int16_t output[kMaxOutputSize];
  int samples_per_channel;
  int num_channels;
  uint32_t timestamp;
  uint32_t last_timestamp;
  NetEqOutputType type;
  NetEqOutputType expected_type[8] = {
      kOutputNormal, kOutputNormal,
      kOutputCNG, kOutputCNG,
      kOutputCNG, kOutputCNG,
      kOutputNormal, kOutputNormal
  };
  int expected_timestamp_increment[8] = {
      -1,  // will not be used.
      10 * kSampleRateKhz,
      0, 0,  // timestamp does not increase during CNG mode.
      0, 0,
      50 * kSampleRateKhz, 10 * kSampleRateKhz
  };

  EXPECT_EQ(NetEq::kOK,
            neteq_->GetAudio(kMaxOutputSize, output, &samples_per_channel,
                             &num_channels, &type));
  EXPECT_TRUE(neteq_->GetPlayoutTimestamp(&last_timestamp));

  for (int i = 1; i < 6; ++i) {
    ASSERT_EQ(kMaxOutputSize, samples_per_channel);
    EXPECT_EQ(1, num_channels);
    EXPECT_EQ(expected_type[i - 1], type);
    EXPECT_TRUE(neteq_->GetPlayoutTimestamp(&timestamp));
    EXPECT_EQ(NetEq::kOK,
              neteq_->GetAudio(kMaxOutputSize, output, &samples_per_channel,
                               &num_channels, &type));
    EXPECT_TRUE(neteq_->GetPlayoutTimestamp(&timestamp));
    EXPECT_EQ(timestamp, last_timestamp + expected_timestamp_increment[i]);
    last_timestamp = timestamp;
  }

  // Insert third packet, which leaves a gap from last packet.
  payload[0] = 2;
  rtp_header.header.sequenceNumber += 2;
  rtp_header.header.timestamp += 2 * kPayloadLengthSamples;
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  for (int i = 6; i < 8; ++i) {
    ASSERT_EQ(kMaxOutputSize, samples_per_channel);
    EXPECT_EQ(1, num_channels);
    EXPECT_EQ(expected_type[i - 1], type);
    EXPECT_EQ(NetEq::kOK,
              neteq_->GetAudio(kMaxOutputSize, output, &samples_per_channel,
                               &num_channels, &type));
    EXPECT_TRUE(neteq_->GetPlayoutTimestamp(&timestamp));
    EXPECT_EQ(timestamp, last_timestamp + expected_timestamp_increment[i]);
    last_timestamp = timestamp;
  }

  // Now check the packet buffer, and make sure it is empty.
  EXPECT_TRUE(packet_buffer_->Empty());

  EXPECT_CALL(mock_decoder, Die());
}

TEST_F(NetEqImplTest, UnsupportedDecoder) {
  UseNoMocks();
  CreateInstance();
  static const size_t kNetEqMaxFrameSize = 2880;  // 60 ms @ 48 kHz.

  const uint8_t kPayloadType = 17;   // Just an arbitrary number.
  const uint32_t kReceiveTime = 17;  // Value doesn't matter for this test.
  const int kSampleRateHz = 8000;
  const int kChannles = 1;

  const int kPayloadLengthSamples = 10 * kSampleRateHz / 1000;  // 10 ms.
  const size_t kPayloadLengthBytes = 1;
  uint8_t payload[kPayloadLengthBytes]= {0};
  int16_t dummy_output[kPayloadLengthSamples] = {0};
  WebRtcRTPHeader rtp_header;
  rtp_header.header.payloadType = kPayloadType;
  rtp_header.header.sequenceNumber = 0x1234;
  rtp_header.header.timestamp = 0x12345678;
  rtp_header.header.ssrc = 0x87654321;

  class MockAudioDecoder : public AudioDecoder {
   public:
    int Init() override {
      return 0;
    }
    MOCK_CONST_METHOD2(PacketDuration, int(const uint8_t*, size_t));
    MOCK_METHOD5(DecodeInternal, int(const uint8_t*, size_t, int, int16_t*,
                                     SpeechType*));
    size_t Channels() const override { return 1; }
  } decoder_;

  const uint8_t kFirstPayloadValue = 1;
  const uint8_t kSecondPayloadValue = 2;

  EXPECT_CALL(decoder_, PacketDuration(Pointee(kFirstPayloadValue),
                                       kPayloadLengthBytes))
    .Times(AtLeast(1))
    .WillRepeatedly(Return(kNetEqMaxFrameSize * kChannles + 1));

  EXPECT_CALL(decoder_,
              DecodeInternal(Pointee(kFirstPayloadValue), _, _, _, _))
      .Times(0);

  EXPECT_CALL(decoder_, DecodeInternal(Pointee(kSecondPayloadValue),
                                       kPayloadLengthBytes,
                                       kSampleRateHz, _, _))
      .Times(1)
      .WillOnce(DoAll(SetArrayArgument<3>(dummy_output,
                                          dummy_output + kPayloadLengthSamples),
                      SetArgPointee<4>(AudioDecoder::kSpeech),
                      Return(kPayloadLengthSamples)));

  EXPECT_CALL(decoder_, PacketDuration(Pointee(kSecondPayloadValue),
                                       kPayloadLengthBytes))
    .Times(AtLeast(1))
    .WillRepeatedly(Return(kNetEqMaxFrameSize * kChannles));

  EXPECT_EQ(NetEq::kOK,
            neteq_->RegisterExternalDecoder(
                &decoder_, kDecoderPCM16B, kPayloadType));

  // Insert one packet.
  payload[0] = kFirstPayloadValue;  // This will make Decode() fail.
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  // Insert another packet.
  payload[0] = kSecondPayloadValue;  // This will make Decode() successful.
  rtp_header.header.sequenceNumber++;
  // The second timestamp needs to be at least 30 ms after the first to make
  // the second packet get decoded.
  rtp_header.header.timestamp += 3 * kPayloadLengthSamples;
  EXPECT_EQ(NetEq::kOK,
            neteq_->InsertPacket(
                rtp_header, payload, kPayloadLengthBytes, kReceiveTime));

  const int kMaxOutputSize = 10 * kSampleRateHz / 1000;
  int16_t output[kMaxOutputSize];
  int samples_per_channel;
  int num_channels;
  NetEqOutputType type;

  EXPECT_EQ(NetEq::kFail, neteq_->GetAudio(kMaxOutputSize, output,
                                           &samples_per_channel, &num_channels,
                                           &type));
  EXPECT_EQ(NetEq::kOtherDecoderError, neteq_->LastError());
  EXPECT_EQ(kMaxOutputSize, samples_per_channel);
  EXPECT_EQ(kChannles, num_channels);

  EXPECT_EQ(NetEq::kOK, neteq_->GetAudio(kMaxOutputSize, output,
                                         &samples_per_channel, &num_channels,
                                         &type));
  EXPECT_EQ(kMaxOutputSize, samples_per_channel);
  EXPECT_EQ(kChannles, num_channels);
}

}  // namespace webrtc
