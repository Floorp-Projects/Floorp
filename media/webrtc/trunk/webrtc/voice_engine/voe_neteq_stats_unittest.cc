/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_neteq_stats.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/main/acm2/audio_coding_module_impl.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/source/audio_coding_module_impl.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/gtest_disable.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {
namespace {

const int kSampleRateHz = 16000;
const int kNumSamples10ms = kSampleRateHz / 100;
const int kFrameSizeMs = 10;  // Multiple of 10.
const int kFrameSizeSamples = kFrameSizeMs / 10 * kNumSamples10ms;
const int kPayloadSizeBytes = kFrameSizeSamples * sizeof(int16_t);
const uint8_t kPayloadType = 111;

class RtpUtility {
 public:
  RtpUtility(int samples_per_packet, uint8_t payload_type)
      : samples_per_packet_(samples_per_packet), payload_type_(payload_type) {}

  virtual ~RtpUtility() {}

  void Populate(WebRtcRTPHeader* rtp_header) {
    rtp_header->header.sequenceNumber = 0xABCD;
    rtp_header->header.timestamp = 0xABCDEF01;
    rtp_header->header.payloadType = payload_type_;
    rtp_header->header.markerBit = false;
    rtp_header->header.ssrc = 0x1234;
    rtp_header->header.numCSRCs = 0;
    rtp_header->frameType = kAudioFrameSpeech;

    rtp_header->header.payload_type_frequency = kSampleRateHz;
    rtp_header->type.Audio.channel = 1;
    rtp_header->type.Audio.isCNG = false;
  }

  void Forward(WebRtcRTPHeader* rtp_header) {
    ++rtp_header->header.sequenceNumber;
    rtp_header->header.timestamp += samples_per_packet_;
  }

 private:
  int samples_per_packet_;
  uint8_t payload_type_;
};

// This factory method allows access to ACM of a channel, facilitating insertion
// of packets to and pulling audio of ACM.
struct InsertAcm : AudioCodingModuleFactory {
  explicit InsertAcm(AudioCodingModule* acm) : acm_(acm) {}
  ~InsertAcm() {}
  virtual AudioCodingModule* Create(int /*id*/) const { return acm_; }

  AudioCodingModule* acm_;
};

class VoENetEqStatsTest : public ::testing::Test {
 protected:
  VoENetEqStatsTest()
      : acm1_(new acm1::AudioCodingModuleImpl(1, Clock::GetRealTimeClock())),
        acm2_(new acm2::AudioCodingModuleImpl(2)),
        voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)),
        voe_neteq_stats_(VoENetEqStats::GetInterface(voe_)),
        channel_acm1_(-1),
        channel_acm2_(-1),
        adm_(new FakeAudioDeviceModule),
        rtp_utility_(new RtpUtility(kFrameSizeSamples, kPayloadType)) {}

  ~VoENetEqStatsTest() {}

  void TearDown() {
    voe_neteq_stats_->Release();
    base_->DeleteChannel(channel_acm1_);
    base_->DeleteChannel(channel_acm2_);
    base_->Terminate();
    base_->Release();
    VoiceEngine::Delete(voe_);
  }

  void SetUp() {
    // Check if all components are valid.
    ASSERT_TRUE(voe_ != NULL);
    ASSERT_TRUE(base_ != NULL);
    ASSERT_TRUE(adm_.get() != NULL);
    ASSERT_EQ(0, base_->Init(adm_.get()));

    // Set configs.
    config_acm1_.Set<AudioCodingModuleFactory>(new InsertAcm(acm1_));
    config_acm2_.Set<AudioCodingModuleFactory>(new InsertAcm(acm2_));

    // Create channe1s;
    channel_acm1_ = base_->CreateChannel(config_acm1_);
    ASSERT_NE(-1, channel_acm1_);

    channel_acm2_ = base_->CreateChannel(config_acm2_);
    ASSERT_NE(-1, channel_acm2_);

    CodecInst codec;
    AudioCodingModule::Codec("L16", &codec, kSampleRateHz, 1);
    codec.pltype = kPayloadType;

    // Register L16 codec in ACMs.
    ASSERT_EQ(0, acm1_->RegisterReceiveCodec(codec));
    ASSERT_EQ(0, acm2_->RegisterReceiveCodec(codec));

    rtp_utility_->Populate(&rtp_header_);
  }

  void InsertPacketAndPullAudio() {
    AudioFrame audio_frame;
    const uint8_t kPayload[kPayloadSizeBytes] = {0};

    ASSERT_EQ(0,
              acm1_->IncomingPacket(kPayload, kPayloadSizeBytes, rtp_header_));
    ASSERT_EQ(0,
              acm2_->IncomingPacket(kPayload, kPayloadSizeBytes, rtp_header_));

    ASSERT_EQ(0, acm1_->PlayoutData10Ms(-1, &audio_frame));
    ASSERT_EQ(0, acm2_->PlayoutData10Ms(-1, &audio_frame));
    rtp_utility_->Forward(&rtp_header_);
  }

  void JustPullAudio() {
    AudioFrame audio_frame;
    ASSERT_EQ(0, acm1_->PlayoutData10Ms(-1, &audio_frame));
    ASSERT_EQ(0, acm2_->PlayoutData10Ms(-1, &audio_frame));
  }

  Config config_acm1_;
  Config config_acm2_;

  // ACMs are inserted into VoE channels, and this class is not the owner of
  // them. Therefore, they should not be deleted, not even in destructor.
  AudioCodingModule* acm1_;
  AudioCodingModule* acm2_;

  VoiceEngine* voe_;
  VoEBase* base_;
  VoENetEqStats* voe_neteq_stats_;
  int channel_acm1_;
  int channel_acm2_;
  scoped_ptr<FakeAudioDeviceModule> adm_;
  scoped_ptr<RtpUtility> rtp_utility_;
  WebRtcRTPHeader rtp_header_;
};

// Check if the statistics are initialized correctly. Before any call to ACM
// all fields have to be zero.
TEST_F(VoENetEqStatsTest, DISABLED_ON_ANDROID(InitializedToZero)) {
  AudioDecodingCallStats stats;
  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm1_, &stats));
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);

  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm2_, &stats));
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);
}

// Apply an initial playout delay. Calls to AudioCodingModule::PlayoutData10ms()
// should result in generating silence, check the associated field.
TEST_F(VoENetEqStatsTest, DISABLED_ON_ANDROID(SilenceGeneratorCalled)) {
  AudioDecodingCallStats stats;
  const int kInitialDelay = 100;

  acm1_->SetInitialPlayoutDelay(kInitialDelay);
  acm2_->SetInitialPlayoutDelay(kInitialDelay);

  AudioFrame audio_frame;
  int num_calls = 0;
  for (int time_ms = 0; time_ms < kInitialDelay;
       time_ms += kFrameSizeMs, ++num_calls) {
    InsertPacketAndPullAudio();
  }
  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm1_, &stats));
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(num_calls, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);

  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm2_, &stats));
  EXPECT_EQ(0, stats.calls_to_neteq);
  EXPECT_EQ(num_calls, stats.calls_to_silence_generator);
  EXPECT_EQ(0, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);
}

// Insert some packets and pull audio. Check statistics are valid. Then,
// simulate packet loss and check if PLC and PLC-to-CNG statistics are
// correctly updated.
TEST_F(VoENetEqStatsTest, DISABLED_ON_ANDROID(NetEqCalls)) {
  AudioDecodingCallStats stats;
  const int kNumNormalCalls = 10;

  AudioFrame audio_frame;
  for (int num_calls = 0; num_calls < kNumNormalCalls; ++num_calls) {
    InsertPacketAndPullAudio();
  }
  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm1_, &stats));
  EXPECT_EQ(kNumNormalCalls, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);

  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm2_, &stats));
  EXPECT_EQ(kNumNormalCalls, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(0, stats.decoded_plc);
  EXPECT_EQ(0, stats.decoded_plc_cng);

  const int kNumPlc = 3;
  const int kNumPlcCng = 5;

  // Simulate packet-loss. NetEq first performs PLC then PLC fades to CNG.
  for (int n = 0; n < kNumPlc + kNumPlcCng; ++n) {
    JustPullAudio();
  }
  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm1_, &stats));
  EXPECT_EQ(kNumNormalCalls + kNumPlc + kNumPlcCng, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(kNumPlc, stats.decoded_plc);
  EXPECT_EQ(kNumPlcCng, stats.decoded_plc_cng);

  ASSERT_EQ(0,
            voe_neteq_stats_->GetDecodingCallStatistics(channel_acm2_, &stats));
  EXPECT_EQ(kNumNormalCalls + kNumPlc + kNumPlcCng, stats.calls_to_neteq);
  EXPECT_EQ(0, stats.calls_to_silence_generator);
  EXPECT_EQ(kNumNormalCalls, stats.decoded_normal);
  EXPECT_EQ(0, stats.decoded_cng);
  EXPECT_EQ(kNumPlc, stats.decoded_plc);
  EXPECT_EQ(kNumPlcCng, stats.decoded_plc_cng);
}

}  // namespace

}  // namespace voe

}  // namespace webrtc
