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
#include <gtest/gtest.h>

#include "test_api.h"

#include "common_types.h"
#include "rtp_rtcp.h"
#include "rtp_rtcp_defines.h"

using namespace webrtc;

#define test_rate 64000u

class VerifyingAudioReceiver : public RtpData {
 public:
  virtual int32_t OnReceivedPayloadData(
      const uint8_t* payloadData,
      const uint16_t payloadSize,
      const webrtc::WebRtcRTPHeader* rtpHeader) {
    if (rtpHeader->header.payloadType == 98 ||
        rtpHeader->header.payloadType == 99) {
      EXPECT_EQ(4, payloadSize);
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

class RTPCallback : public RtpFeedback {
 public:
  virtual int32_t OnInitializeDecoder(
      const int32_t id,
      const int8_t payloadType,
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const uint8_t channels,
      const uint32_t rate) {
    if (payloadType == 96) {
      EXPECT_EQ(test_rate, rate) <<
          "The rate should be 64K for this payloadType";
    }
    return 0;
  }
  virtual void OnPacketTimeout(const int32_t id) {
  }
  virtual void OnReceivedPacket(const int32_t id,
                                const RtpRtcpPacketType packetType) {
  }
  virtual void OnPeriodicDeadOrAlive(const int32_t id,
                                     const RTPAliveType alive) {
  }
  virtual void OnIncomingSSRCChanged(const int32_t id,
                                     const uint32_t SSRC) {
  }
  virtual void OnIncomingCSRCChanged(const int32_t id,
                                     const uint32_t CSRC,
                                     const bool added) {
  }
};

class AudioFeedback : public RtpAudioFeedback {
  virtual void OnReceivedTelephoneEvent(const int32_t id,
                                        const uint8_t event,
                                        const bool end) {
    static uint8_t expectedEvent = 0;

    if (end) {
      uint8_t oldEvent = expectedEvent-1;
      if (expectedEvent == 32) {
        oldEvent = 15;
      }
      EXPECT_EQ(oldEvent, event);
    } else {
      EXPECT_EQ(expectedEvent, event);
      expectedEvent++;
    }
    if (expectedEvent == 16) {
      expectedEvent = 32;
    }
  }
  virtual void OnPlayTelephoneEvent(const int32_t id,
                                    const uint8_t event,
                                    const uint16_t lengthMs,
                                    const uint8_t volume) {
  };
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

  virtual void SetUp() {
    audioFeedback = new AudioFeedback();
    data_receiver1 = new VerifyingAudioReceiver();
    data_receiver2 = new VerifyingAudioReceiver();
    rtp_callback = new RTPCallback();
    transport1 = new LoopBackTransport();
    transport2 = new LoopBackTransport();

    RtpRtcp::Configuration configuration;
    configuration.id = test_id;
    configuration.audio = true;
    configuration.clock = &fake_clock;
    configuration.incoming_data = data_receiver1;
    configuration.outgoing_transport = transport1;
    configuration.audio_messages = audioFeedback;

    module1 = RtpRtcp::CreateRtpRtcp(configuration);

    configuration.id = test_id + 1;
    configuration.incoming_data = data_receiver2;
    configuration.incoming_messages = rtp_callback;
    configuration.outgoing_transport = transport2;
    configuration.audio_messages = audioFeedback;

    module2 = RtpRtcp::CreateRtpRtcp(configuration);

    transport1->SetSendModule(module2);
    transport2->SetSendModule(module1);
  }

  virtual void TearDown() {
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
  VerifyingAudioReceiver* data_receiver1;
  VerifyingAudioReceiver* data_receiver2;
  LoopBackTransport* transport1;
  LoopBackTransport* transport2;
  AudioFeedback* audioFeedback;
  RTPCallback* rtp_callback;
  uint32_t test_ssrc;
  uint32_t test_timestamp;
  uint16_t test_sequence_number;
  uint32_t test_CSRC[webrtc::kRtpCsrcSize];
  SimulatedClock fake_clock;
};

TEST_F(RtpRtcpAudioTest, Basic) {
  EXPECT_EQ(0, module1->SetSSRC(test_ssrc));
  EXPECT_EQ(0, module1->SetStartTimestamp(test_timestamp));

  // Test detection at the end of a DTMF tone.
  EXPECT_EQ(0, module2->SetTelephoneEventForwardToDecoder(true));

  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Start basic RTP test.

  // Send an empty RTP packet.
  // Should fail since we have not registerd the payload type.
  EXPECT_EQ(-1, module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                          96, 0, -1, NULL, 0));

  CodecInst voiceCodec;
  voiceCodec.pltype = 96;
  voiceCodec.plfreq = 8000;
  memcpy(voiceCodec.plname, "PCMU", 5);

  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module1->RegisterReceivePayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterSendPayload(voiceCodec));
  voiceCodec.rate = test_rate;
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));
  printf("4\n");

  const uint8_t test[5] = "test";
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                         0, -1, test, 4));

  EXPECT_EQ(test_ssrc, module2->RemoteSSRC());
  EXPECT_EQ(test_timestamp, module2->RemoteTimestamp());
}

TEST_F(RtpRtcpAudioTest, RED) {
  CodecInst voiceCodec;
  voiceCodec.pltype = 96;
  voiceCodec.plfreq = 8000;
  memcpy(voiceCodec.plname, "PCMU", 5);

  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module1->RegisterReceivePayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterSendPayload(voiceCodec));
  voiceCodec.rate = test_rate;
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  EXPECT_EQ(0, module1->SetSSRC(test_ssrc));
  EXPECT_EQ(0, module1->SetStartTimestamp(test_timestamp));
  EXPECT_EQ(0, module1->SetSendingStatus(true));

  voiceCodec.pltype = 127;
  voiceCodec.plfreq = 8000;
  memcpy(voiceCodec.plname, "RED", 4);

  EXPECT_EQ(0, module1->SetSendREDPayloadType(voiceCodec.pltype));
  int8_t red = 0;
  EXPECT_EQ(0, module1->SendREDPayloadType(red));
  EXPECT_EQ(voiceCodec.pltype, red);
  EXPECT_EQ(0, module1->RegisterReceivePayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  RTPFragmentationHeader fragmentation;
  fragmentation.fragmentationVectorSize = 2;
  fragmentation.fragmentationLength = new uint32_t[2];
  fragmentation.fragmentationLength[0] = 4;
  fragmentation.fragmentationLength[1] = 4;
  fragmentation.fragmentationOffset = new uint32_t[2];
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
  CodecInst voiceCodec;
  voiceCodec.pltype = 96;
  voiceCodec.plfreq = 8000;
  memcpy(voiceCodec.plname, "PCMU", 5);

  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module1->RegisterReceivePayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterSendPayload(voiceCodec));
  voiceCodec.rate = test_rate;
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  EXPECT_EQ(0, module1->SetSSRC(test_ssrc));
  EXPECT_EQ(0, module1->SetStartTimestamp(test_timestamp));
  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Prepare for DTMF.
  voiceCodec.pltype = 97;
  voiceCodec.plfreq = 8000;
  memcpy(voiceCodec.plname, "telephone-event", 16);

  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

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
