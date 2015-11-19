/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <vector>
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/modules/rtp_rtcp/test/testAPI/test_api.h"

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_audio.h"

using namespace webrtc;

#define test_rate 64000u

class VerifyingAudioReceiver : public NullRtpData {
 public:
  int32_t OnReceivedPayloadData(
      const uint8_t* payloadData,
      const size_t payloadSize,
      const webrtc::WebRtcRTPHeader* rtpHeader) override {
    if (rtpHeader->header.payloadType == 98 ||
        rtpHeader->header.payloadType == 99) {
      EXPECT_EQ(4u, payloadSize);
      char str[5];
      memcpy(str, payloadData, payloadSize);
      str[4] = 0;
      // All our test vectors for payload type 96 and 97 even the stereo is on
      // a per channel base equal to the 4 chars "test".
      // Note there is no null termination so we add that to use the
      // test EXPECT_STRCASEEQ.
      EXPECT_STRCASEEQ("test", str);
      return 0;
    }
    if (rtpHeader->header.payloadType == 100 ||
        rtpHeader->header.payloadType == 101 ||
        rtpHeader->header.payloadType == 102) {
      if (rtpHeader->type.Audio.channel == 1) {
        if (payloadData[0] == 0xff) {
          // All our test vectors for payload type 100, 101 and 102 have the
          // first channel data being equal to 0xff.
          return 0;
        }
      }
      ADD_FAILURE() << "This code path should never happen.";
      return -1;
    }
    return 0;
  }
};

class RTPCallback : public NullRtpFeedback {
 public:
  int32_t OnInitializeDecoder(const int32_t id,
                              const int8_t payloadType,
                              const char payloadName[RTP_PAYLOAD_NAME_SIZE],
                              const int frequency,
                              const uint8_t channels,
                              const uint32_t rate) override {
    if (payloadType == 96) {
      EXPECT_EQ(test_rate, rate) <<
          "The rate should be 64K for this payloadType";
    }
    return 0;
  }
};

class RtpRtcpAudioTest : public ::testing::Test {
 protected:
  RtpRtcpAudioTest() : fake_clock(123456) {
    test_CSRC[0] = 1234;
    test_CSRC[2] = 2345;
    test_id = 123;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpAudioTest() {}

  void SetUp() override {
    audioFeedback = new NullRtpAudioFeedback();
    data_receiver1 = new VerifyingAudioReceiver();
    data_receiver2 = new VerifyingAudioReceiver();
    rtp_callback = new RTPCallback();
    transport1 = new LoopBackTransport();
    transport2 = new LoopBackTransport();

    receive_statistics1_.reset(ReceiveStatistics::Create(&fake_clock));
    receive_statistics2_.reset(ReceiveStatistics::Create(&fake_clock));

    rtp_payload_registry1_.reset(new RTPPayloadRegistry(
        RTPPayloadStrategy::CreateStrategy(true)));
    rtp_payload_registry2_.reset(new RTPPayloadRegistry(
        RTPPayloadStrategy::CreateStrategy(true)));

    RtpRtcp::Configuration configuration;
    configuration.id = test_id;
    configuration.audio = true;
    configuration.clock = &fake_clock;
    configuration.receive_statistics = receive_statistics1_.get();
    configuration.outgoing_transport = transport1;
    configuration.audio_messages = audioFeedback;

    module1 = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_receiver1_.reset(RtpReceiver::CreateAudioReceiver(
        test_id, &fake_clock, audioFeedback, data_receiver1, NULL,
        rtp_payload_registry1_.get()));

    configuration.id = test_id + 1;
    configuration.receive_statistics = receive_statistics2_.get();
    configuration.outgoing_transport = transport2;
    configuration.audio_messages = audioFeedback;

    module2 = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_receiver2_.reset(RtpReceiver::CreateAudioReceiver(
            test_id + 1, &fake_clock, audioFeedback, data_receiver2, NULL,
            rtp_payload_registry2_.get()));

    transport1->SetSendModule(module2, rtp_payload_registry2_.get(),
                              rtp_receiver2_.get(), receive_statistics2_.get());
    transport2->SetSendModule(module1, rtp_payload_registry1_.get(),
                              rtp_receiver1_.get(), receive_statistics1_.get());
  }

  void TearDown() override {
    delete module1;
    delete module2;
    delete transport1;
    delete transport2;
    delete audioFeedback;
    delete data_receiver1;
    delete data_receiver2;
    delete rtp_callback;
  }

  int test_id;
  RtpRtcp* module1;
  RtpRtcp* module2;
  rtc::scoped_ptr<ReceiveStatistics> receive_statistics1_;
  rtc::scoped_ptr<ReceiveStatistics> receive_statistics2_;
  rtc::scoped_ptr<RtpReceiver> rtp_receiver1_;
  rtc::scoped_ptr<RtpReceiver> rtp_receiver2_;
  rtc::scoped_ptr<RTPPayloadRegistry> rtp_payload_registry1_;
  rtc::scoped_ptr<RTPPayloadRegistry> rtp_payload_registry2_;
  VerifyingAudioReceiver* data_receiver1;
  VerifyingAudioReceiver* data_receiver2;
  LoopBackTransport* transport1;
  LoopBackTransport* transport2;
  NullRtpAudioFeedback* audioFeedback;
  RTPCallback* rtp_callback;
  uint32_t test_ssrc;
  uint32_t test_timestamp;
  uint16_t test_sequence_number;
  uint32_t test_CSRC[webrtc::kRtpCsrcSize];
  SimulatedClock fake_clock;
};

TEST_F(RtpRtcpAudioTest, Basic) {
  module1->SetSSRC(test_ssrc);
  module1->SetStartTimestamp(test_timestamp);

  // Test detection at the end of a DTMF tone.
  //EXPECT_EQ(0, module2->SetTelephoneEventForwardToDecoder(true));

  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Start basic RTP test.

  // Send an empty RTP packet.
  // Should fail since we have not registered the payload type.
  EXPECT_EQ(-1, module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                          96, 0, -1, NULL, 0));

  CodecInst voice_codec;
  memset(&voice_codec, 0, sizeof(voice_codec));
  voice_codec.pltype = 96;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "PCMU", 5);

  EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
  EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));
  EXPECT_EQ(0, module2->RegisterSendPayload(voice_codec));
  voice_codec.rate = test_rate;
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));

  const uint8_t test[5] = "test";
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                         0, -1, test, 4));

  EXPECT_EQ(test_ssrc, rtp_receiver2_->SSRC());
  uint32_t timestamp;
  EXPECT_TRUE(rtp_receiver2_->Timestamp(&timestamp));
  EXPECT_EQ(test_timestamp, timestamp);
}

TEST_F(RtpRtcpAudioTest, RED) {
  CodecInst voice_codec;
  memset(&voice_codec, 0, sizeof(voice_codec));
  voice_codec.pltype = 96;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "PCMU", 5);

  EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
  EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));
  EXPECT_EQ(0, module2->RegisterSendPayload(voice_codec));
  voice_codec.rate = test_rate;
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));

  module1->SetSSRC(test_ssrc);
  module1->SetStartTimestamp(test_timestamp);
  EXPECT_EQ(0, module1->SetSendingStatus(true));

  voice_codec.pltype = 127;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "RED", 4);

  EXPECT_EQ(0, module1->SetSendREDPayloadType(voice_codec.pltype));
  int8_t red = 0;
  EXPECT_EQ(0, module1->SendREDPayloadType(red));
  EXPECT_EQ(voice_codec.pltype, red);
  EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));

  RTPFragmentationHeader fragmentation;
  fragmentation.fragmentationVectorSize = 2;
  fragmentation.fragmentationLength = new size_t[2];
  fragmentation.fragmentationLength[0] = 4;
  fragmentation.fragmentationLength[1] = 4;
  fragmentation.fragmentationOffset = new size_t[2];
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationOffset[1] = 4;
  fragmentation.fragmentationTimeDiff = new uint16_t[2];
  fragmentation.fragmentationTimeDiff[0] = 0;
  fragmentation.fragmentationTimeDiff[1] = 0;
  fragmentation.fragmentationPlType = new uint8_t[2];
  fragmentation.fragmentationPlType[0] = 96;
  fragmentation.fragmentationPlType[1] = 96;

  const uint8_t test[5] = "test";
  // Send a RTP packet.
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                         96, 160, -1, test, 4,
                                         &fragmentation));

  EXPECT_EQ(0, module1->SetSendREDPayloadType(-1));
  EXPECT_EQ(-1, module1->SendREDPayloadType(red));
}

TEST_F(RtpRtcpAudioTest, DTMF) {
  CodecInst voice_codec;
  memset(&voice_codec, 0, sizeof(voice_codec));
  voice_codec.pltype = 96;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "PCMU", 5);

  EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
  EXPECT_EQ(0, rtp_receiver1_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));
  EXPECT_EQ(0, module2->RegisterSendPayload(voice_codec));
  voice_codec.rate = test_rate;
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
      voice_codec.plname,
      voice_codec.pltype,
      voice_codec.plfreq,
      voice_codec.channels,
      (voice_codec.rate < 0) ? 0 : voice_codec.rate));

  module1->SetSSRC(test_ssrc);
  module1->SetStartTimestamp(test_timestamp);
  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Prepare for DTMF.
  voice_codec.pltype = 97;
  voice_codec.plfreq = 8000;
  memcpy(voice_codec.plname, "telephone-event", 16);

  EXPECT_EQ(0, module1->RegisterSendPayload(voice_codec));
  EXPECT_EQ(0, rtp_receiver2_->RegisterReceivePayload(
        voice_codec.plname,
        voice_codec.pltype,
        voice_codec.plfreq,
        voice_codec.channels,
        (voice_codec.rate < 0) ? 0 : voice_codec.rate));

  // Start DTMF test.
  uint32_t timeStamp = 160;

  // Send a DTMF tone using RFC 2833 (4733).
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(0, module1->SendTelephoneEventOutband(i, timeStamp, 10));
  }
  timeStamp += 160;  // Prepare for next packet.

  const uint8_t test[9] = "test";

  // Send RTP packets for 16 tones a 160 ms  100ms
  // pause between = 2560ms + 1600ms = 4160ms
  for (;timeStamp <= 250 * 160; timeStamp += 160) {
    EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                           timeStamp, -1, test, 4));
    fake_clock.AdvanceTimeMilliseconds(20);
    module1->Process();
  }
  EXPECT_EQ(0, module1->SendTelephoneEventOutband(32, 9000, 10));

  for (;timeStamp <= 740 * 160; timeStamp += 160) {
    EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                           timeStamp, -1, test, 4));
    fake_clock.AdvanceTimeMilliseconds(20);
    module1->Process();
  }
}
