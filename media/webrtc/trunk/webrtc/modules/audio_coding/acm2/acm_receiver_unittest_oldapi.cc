/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/acm2/acm_receiver.h"

#include <algorithm>  // std::min

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/modules/audio_coding/acm2/audio_coding_module_impl.h"
#include "webrtc/modules/audio_coding/neteq/tools/rtp_generator.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/test_suite.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

namespace acm2 {
namespace {

bool CodecsEqual(const CodecInst& codec_a, const CodecInst& codec_b) {
    if (strcmp(codec_a.plname, codec_b.plname) != 0 ||
        codec_a.plfreq != codec_b.plfreq ||
        codec_a.pltype != codec_b.pltype ||
        codec_b.channels != codec_a.channels)
      return false;
    return true;
}

struct CodecIdInst {
  explicit CodecIdInst(RentACodec::CodecId codec_id) {
    const auto codec_ix = RentACodec::CodecIndexFromId(codec_id);
    EXPECT_TRUE(codec_ix);
    id = *codec_ix;
    const auto codec_inst = RentACodec::CodecInstById(codec_id);
    EXPECT_TRUE(codec_inst);
    inst = *codec_inst;
  }
  int id;
  CodecInst inst;
};

}  // namespace

class AcmReceiverTestOldApi : public AudioPacketizationCallback,
                              public ::testing::Test {
 protected:
  AcmReceiverTestOldApi()
      : timestamp_(0),
        packet_sent_(false),
        last_packet_send_timestamp_(timestamp_),
        last_frame_type_(kEmptyFrame) {
    AudioCodingModule::Config config;
    acm_.reset(new AudioCodingModuleImpl(config));
    receiver_.reset(new AcmReceiver(config));
  }

  ~AcmReceiverTestOldApi() {}

  void SetUp() override {
    ASSERT_TRUE(receiver_.get() != NULL);
    ASSERT_TRUE(acm_.get() != NULL);
    codecs_ = RentACodec::Database();

    acm_->InitializeReceiver();
    acm_->RegisterTransportCallback(this);

    rtp_header_.header.sequenceNumber = 0;
    rtp_header_.header.timestamp = 0;
    rtp_header_.header.markerBit = false;
    rtp_header_.header.ssrc = 0x12345678;  // Arbitrary.
    rtp_header_.header.numCSRCs = 0;
    rtp_header_.header.payloadType = 0;
    rtp_header_.frameType = kAudioFrameSpeech;
    rtp_header_.type.Audio.isCNG = false;
  }

  void TearDown() override {}

  void InsertOnePacketOfSilence(int codec_id) {
    CodecInst codec =
        *RentACodec::CodecInstById(*RentACodec::CodecIdFromIndex(codec_id));
    if (timestamp_ == 0) {  // This is the first time inserting audio.
      ASSERT_EQ(0, acm_->RegisterSendCodec(codec));
    } else {
      auto current_codec = acm_->SendCodec();
      ASSERT_TRUE(current_codec);
      if (!CodecsEqual(codec, *current_codec))
        ASSERT_EQ(0, acm_->RegisterSendCodec(codec));
    }
    AudioFrame frame;
    // Frame setup according to the codec.
    frame.sample_rate_hz_ = codec.plfreq;
    frame.samples_per_channel_ = codec.plfreq / 100;  // 10 ms.
    frame.num_channels_ = codec.channels;
    memset(frame.data_, 0, frame.samples_per_channel_ * frame.num_channels_ *
           sizeof(int16_t));
    packet_sent_ = false;
    last_packet_send_timestamp_ = timestamp_;
    while (!packet_sent_) {
      frame.timestamp_ = timestamp_;
      timestamp_ += frame.samples_per_channel_;
      ASSERT_GE(acm_->Add10MsData(frame), 0);
    }
  }

  template <size_t N>
  void AddSetOfCodecs(const RentACodec::CodecId(&ids)[N]) {
    for (auto id : ids) {
      const auto i = RentACodec::CodecIndexFromId(id);
      ASSERT_TRUE(i);
      ASSERT_EQ(
          0, receiver_->AddCodec(*i, codecs_[*i].pltype, codecs_[*i].channels,
                                 codecs_[*i].plfreq, nullptr, ""));
    }
  }

  int SendData(FrameType frame_type,
               uint8_t payload_type,
               uint32_t timestamp,
               const uint8_t* payload_data,
               size_t payload_len_bytes,
               const RTPFragmentationHeader* fragmentation) override {
    if (frame_type == kEmptyFrame)
      return 0;

    rtp_header_.header.payloadType = payload_type;
    rtp_header_.frameType = frame_type;
    if (frame_type == kAudioFrameSpeech)
      rtp_header_.type.Audio.isCNG = false;
    else
      rtp_header_.type.Audio.isCNG = true;
    rtp_header_.header.timestamp = timestamp;

    int ret_val = receiver_->InsertPacket(
        rtp_header_,
        rtc::ArrayView<const uint8_t>(payload_data, payload_len_bytes));
    if (ret_val < 0) {
      assert(false);
      return -1;
    }
    rtp_header_.header.sequenceNumber++;
    packet_sent_ = true;
    last_frame_type_ = frame_type;
    return 0;
  }

  rtc::scoped_ptr<AcmReceiver> receiver_;
  rtc::ArrayView<const CodecInst> codecs_;
  rtc::scoped_ptr<AudioCodingModule> acm_;
  WebRtcRTPHeader rtp_header_;
  uint32_t timestamp_;
  bool packet_sent_;  // Set when SendData is called reset when inserting audio.
  uint32_t last_packet_send_timestamp_;
  FrameType last_frame_type_;
};

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecGetCodec DISABLED_AddCodecGetCodec
#else
#define MAYBE_AddCodecGetCodec AddCodecGetCodec
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_AddCodecGetCodec) {
  // Add codec.
  for (size_t n = 0; n < codecs_.size(); ++n) {
    if (n & 0x1)  // Just add codecs with odd index.
      EXPECT_EQ(0,
                receiver_->AddCodec(n, codecs_[n].pltype, codecs_[n].channels,
                                    codecs_[n].plfreq, NULL, ""));
  }
  // Get codec and compare.
  for (size_t n = 0; n < codecs_.size(); ++n) {
    CodecInst my_codec;
    if (n & 0x1) {
      // Codecs with odd index should match the reference.
      EXPECT_EQ(0, receiver_->DecoderByPayloadType(codecs_[n].pltype,
                                                   &my_codec));
      EXPECT_TRUE(CodecsEqual(codecs_[n], my_codec));
    } else {
      // Codecs with even index are not registered.
      EXPECT_EQ(-1, receiver_->DecoderByPayloadType(codecs_[n].pltype,
                                                    &my_codec));
    }
  }
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecChangePayloadType DISABLED_AddCodecChangePayloadType
#else
#define MAYBE_AddCodecChangePayloadType AddCodecChangePayloadType
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_AddCodecChangePayloadType) {
  const CodecIdInst codec1(RentACodec::CodecId::kPCMA);
  CodecInst codec2 = codec1.inst;
  ++codec2.pltype;
  CodecInst test_codec;

  // Register the same codec with different payloads.
  EXPECT_EQ(0, receiver_->AddCodec(codec1.id, codec1.inst.pltype,
                                   codec1.inst.channels, codec1.inst.plfreq,
                                   nullptr, ""));
  EXPECT_EQ(0, receiver_->AddCodec(codec1.id, codec2.pltype, codec2.channels,
                                   codec2.plfreq, NULL, ""));

  // Both payload types should exist.
  EXPECT_EQ(0,
            receiver_->DecoderByPayloadType(codec1.inst.pltype, &test_codec));
  EXPECT_EQ(true, CodecsEqual(codec1.inst, test_codec));
  EXPECT_EQ(0, receiver_->DecoderByPayloadType(codec2.pltype, &test_codec));
  EXPECT_EQ(true, CodecsEqual(codec2, test_codec));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecChangeCodecId DISABLED_AddCodecChangeCodecId
#else
#define MAYBE_AddCodecChangeCodecId AddCodecChangeCodecId
#endif
TEST_F(AcmReceiverTestOldApi, AddCodecChangeCodecId) {
  const CodecIdInst codec1(RentACodec::CodecId::kPCMU);
  CodecIdInst codec2(RentACodec::CodecId::kPCMA);
  codec2.inst.pltype = codec1.inst.pltype;
  CodecInst test_codec;

  // Register the same payload type with different codec ID.
  EXPECT_EQ(0, receiver_->AddCodec(codec1.id, codec1.inst.pltype,
                                   codec1.inst.channels, codec1.inst.plfreq,
                                   nullptr, ""));
  EXPECT_EQ(0, receiver_->AddCodec(codec2.id, codec2.inst.pltype,
                                   codec2.inst.channels, codec2.inst.plfreq,
                                   nullptr, ""));

  // Make sure that the last codec is used.
  EXPECT_EQ(0,
            receiver_->DecoderByPayloadType(codec2.inst.pltype, &test_codec));
  EXPECT_EQ(true, CodecsEqual(codec2.inst, test_codec));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_AddCodecRemoveCodec DISABLED_AddCodecRemoveCodec
#else
#define MAYBE_AddCodecRemoveCodec AddCodecRemoveCodec
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_AddCodecRemoveCodec) {
  const CodecIdInst codec(RentACodec::CodecId::kPCMA);
  const int payload_type = codec.inst.pltype;
  EXPECT_EQ(
      0, receiver_->AddCodec(codec.id, codec.inst.pltype, codec.inst.channels,
                             codec.inst.plfreq, nullptr, ""));

  // Remove non-existing codec should not fail. ACM1 legacy.
  EXPECT_EQ(0, receiver_->RemoveCodec(payload_type + 1));

  // Remove an existing codec.
  EXPECT_EQ(0, receiver_->RemoveCodec(payload_type));

  // Ask for the removed codec, must fail.
  CodecInst ci;
  EXPECT_EQ(-1, receiver_->DecoderByPayloadType(payload_type, &ci));
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_SampleRate DISABLED_SampleRate
#else
#define MAYBE_SampleRate SampleRate
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_SampleRate) {
  const RentACodec::CodecId kCodecId[] = {RentACodec::CodecId::kISAC,
                                          RentACodec::CodecId::kISACSWB};
  AddSetOfCodecs(kCodecId);

  AudioFrame frame;
  const int kOutSampleRateHz = 8000;  // Different than codec sample rate.
  for (const auto codec_id : kCodecId) {
    const CodecIdInst codec(codec_id);
    const int num_10ms_frames = codec.inst.pacsize / (codec.inst.plfreq / 100);
    InsertOnePacketOfSilence(codec.id);
    for (int k = 0; k < num_10ms_frames; ++k) {
      EXPECT_EQ(0, receiver_->GetAudio(kOutSampleRateHz, &frame));
    }
    EXPECT_EQ(codec.inst.plfreq, receiver_->last_output_sample_rate_hz());
  }
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_PostdecodingVad DISABLED_PostdecodingVad
#else
#define MAYBE_PostdecodingVad PostdecodingVad
#endif
TEST_F(AcmReceiverTestOldApi, MAYBE_PostdecodingVad) {
  receiver_->EnableVad();
  EXPECT_TRUE(receiver_->vad_enabled());
  const CodecIdInst codec(RentACodec::CodecId::kPCM16Bwb);
  ASSERT_EQ(
      0, receiver_->AddCodec(codec.id, codec.inst.pltype, codec.inst.channels,
                             codec.inst.plfreq, nullptr, ""));
  const int kNumPackets = 5;
  const int num_10ms_frames = codec.inst.pacsize / (codec.inst.plfreq / 100);
  AudioFrame frame;
  for (int n = 0; n < kNumPackets; ++n) {
    InsertOnePacketOfSilence(codec.id);
    for (int k = 0; k < num_10ms_frames; ++k)
      ASSERT_EQ(0, receiver_->GetAudio(codec.inst.plfreq, &frame));
  }
  EXPECT_EQ(AudioFrame::kVadPassive, frame.vad_activity_);

  receiver_->DisableVad();
  EXPECT_FALSE(receiver_->vad_enabled());

  for (int n = 0; n < kNumPackets; ++n) {
    InsertOnePacketOfSilence(codec.id);
    for (int k = 0; k < num_10ms_frames; ++k)
      ASSERT_EQ(0, receiver_->GetAudio(codec.inst.plfreq, &frame));
  }
  EXPECT_EQ(AudioFrame::kVadUnknown, frame.vad_activity_);
}

#if defined(WEBRTC_ANDROID)
#define MAYBE_LastAudioCodec DISABLED_LastAudioCodec
#else
#define MAYBE_LastAudioCodec LastAudioCodec
#endif
#if defined(WEBRTC_CODEC_ISAC)
TEST_F(AcmReceiverTestOldApi, MAYBE_LastAudioCodec) {
  const RentACodec::CodecId kCodecId[] = {
      RentACodec::CodecId::kISAC, RentACodec::CodecId::kPCMA,
      RentACodec::CodecId::kISACSWB, RentACodec::CodecId::kPCM16Bswb32kHz};
  AddSetOfCodecs(kCodecId);

  const RentACodec::CodecId kCngId[] = {
      // Not including full-band.
      RentACodec::CodecId::kCNNB, RentACodec::CodecId::kCNWB,
      RentACodec::CodecId::kCNSWB};
  AddSetOfCodecs(kCngId);

  // Register CNG at sender side.
  for (auto id : kCngId)
    ASSERT_EQ(0, acm_->RegisterSendCodec(CodecIdInst(id).inst));

  CodecInst codec;
  // No audio payload is received.
  EXPECT_EQ(-1, receiver_->LastAudioCodec(&codec));

  // Start with sending DTX.
  ASSERT_EQ(0, acm_->SetVAD(true, true, VADVeryAggr));
  packet_sent_ = false;
  InsertOnePacketOfSilence(CodecIdInst(kCodecId[0]).id);  // Enough to test
                                                          // with one codec.
  ASSERT_TRUE(packet_sent_);
  EXPECT_EQ(kAudioFrameCN, last_frame_type_);

  // Has received, only, DTX. Last Audio codec is undefined.
  EXPECT_EQ(-1, receiver_->LastAudioCodec(&codec));
  EXPECT_FALSE(receiver_->last_packet_sample_rate_hz());

  for (auto id : kCodecId) {
    const CodecIdInst c(id);

    // Set DTX off to send audio payload.
    acm_->SetVAD(false, false, VADAggr);
    packet_sent_ = false;
    InsertOnePacketOfSilence(c.id);

    // Sanity check if Actually an audio payload received, and it should be
    // of type "speech."
    ASSERT_TRUE(packet_sent_);
    ASSERT_EQ(kAudioFrameSpeech, last_frame_type_);
    EXPECT_EQ(rtc::Optional<int>(c.inst.plfreq),
              receiver_->last_packet_sample_rate_hz());

    // Set VAD on to send DTX. Then check if the "Last Audio codec" returns
    // the expected codec.
    acm_->SetVAD(true, true, VADAggr);

    // Do as many encoding until a DTX is sent.
    while (last_frame_type_ != kAudioFrameCN) {
      packet_sent_ = false;
      InsertOnePacketOfSilence(c.id);
      ASSERT_TRUE(packet_sent_);
    }
    EXPECT_EQ(rtc::Optional<int>(c.inst.plfreq),
              receiver_->last_packet_sample_rate_hz());
    EXPECT_EQ(0, receiver_->LastAudioCodec(&codec));
    EXPECT_TRUE(CodecsEqual(c.inst, codec));
  }
}
#endif

}  // namespace acm2

}  // namespace webrtc
