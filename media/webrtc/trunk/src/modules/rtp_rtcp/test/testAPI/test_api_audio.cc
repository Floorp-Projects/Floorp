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
  VerifyingAudioReceiver(RtpRtcp* rtpRtcpModule) {}

  virtual WebRtc_Word32 OnReceivedPayloadData(
      const WebRtc_UWord8* payloadData,
      const WebRtc_UWord16 payloadSize,
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
      } else if (rtpHeader->type.Audio.channel == 2) {
        if (payloadData[0] == 0x0) {
          // All our test vectors for payload type 100, 101 and 102 have the
          // second channel data being equal to 0x00.
          return 0;
        }
      } else if (rtpHeader->type.Audio.channel == 3) {
        // All our test vectors for payload type 100, 101 and 102 have the
        // third channel data being equal to 0xaa.
        if (payloadData[0] == 0xaa) {
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
  virtual WebRtc_Word32 OnInitializeDecoder(
      const WebRtc_Word32 id,
      const WebRtc_Word8 payloadType,
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate) {
    if (payloadType == 96) {
      EXPECT_EQ(test_rate, rate) <<
          "The rate should be 64K for this payloadType";
    }
    return 0;
  }
  virtual void OnPacketTimeout(const WebRtc_Word32 id) {
  }
  virtual void OnReceivedPacket(const WebRtc_Word32 id,
                                const RtpRtcpPacketType packetType) {
  }
  virtual void OnPeriodicDeadOrAlive(const WebRtc_Word32 id,
                                     const RTPAliveType alive) {
  }
  virtual void OnIncomingSSRCChanged(const WebRtc_Word32 id,
                                     const WebRtc_UWord32 SSRC) {
  }
  virtual void OnIncomingCSRCChanged(const WebRtc_Word32 id,
                                     const WebRtc_UWord32 CSRC,
                                     const bool added) {
  }
};

class AudioFeedback : public RtpAudioFeedback {
  virtual void OnReceivedTelephoneEvent(const WebRtc_Word32 id,
                                        const WebRtc_UWord8 event,
                                        const bool end) {
    static WebRtc_UWord8 expectedEvent = 0;

    if (end) {
      WebRtc_UWord8 oldEvent = expectedEvent-1;
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
  virtual void OnPlayTelephoneEvent(const WebRtc_Word32 id,
                                    const WebRtc_UWord8 event,
                                    const WebRtc_UWord16 lengthMs,
                                    const WebRtc_UWord8 volume) {
  };
};

class RtpRtcpAudioTest : public ::testing::Test {
 protected:
  RtpRtcpAudioTest() {
    test_CSRC[0] = 1234;
    test_CSRC[2] = 2345;
    test_id = 123;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpAudioTest() {}

  virtual void SetUp() {
    module1 = RtpRtcp::CreateRtpRtcp(test_id, true, &fake_clock);
    module2 = RtpRtcp::CreateRtpRtcp(test_id+1, true, &fake_clock);

    EXPECT_EQ(0, module1->InitReceiver());
    EXPECT_EQ(0, module1->InitSender());
    EXPECT_EQ(0, module2->InitReceiver());
    EXPECT_EQ(0, module2->InitSender());
    data_receiver1 = new VerifyingAudioReceiver(module1);
    EXPECT_EQ(0, module1->RegisterIncomingDataCallback(data_receiver1));
    data_receiver2 = new VerifyingAudioReceiver(module2);
    EXPECT_EQ(0, module2->RegisterIncomingDataCallback(data_receiver2));
    transport1 = new LoopBackTransport(module2);
    EXPECT_EQ(0, module1->RegisterSendTransport(transport1));
    transport2 = new LoopBackTransport(module1);
    EXPECT_EQ(0, module2->RegisterSendTransport(transport2));
    rtp_callback = new RTPCallback();
    EXPECT_EQ(0, module2->RegisterIncomingRTPCallback(rtp_callback));
  }

  virtual void TearDown() {
    RtpRtcp::DestroyRtpRtcp(module1);
    RtpRtcp::DestroyRtpRtcp(module2);
    delete transport1;
    delete transport2;
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
  RTPCallback* rtp_callback;
  WebRtc_UWord32 test_ssrc;
  WebRtc_UWord32 test_timestamp;
  WebRtc_UWord16 test_sequence_number;
  WebRtc_UWord32 test_CSRC[webrtc::kRtpCsrcSize];
  FakeRtpRtcpClock fake_clock;
};

TEST_F(RtpRtcpAudioTest, Basic) {
  EXPECT_EQ(0, module1->SetSSRC(test_ssrc));
  EXPECT_EQ(0, module1->SetStartTimestamp(test_timestamp));

  EXPECT_FALSE(module1->TelephoneEvent());

  // Test detection at the end of a DTMF tone.
  EXPECT_EQ(0, module2->SetTelephoneEventStatus(true, true, true));
  EXPECT_EQ(true, module2->TelephoneEvent());

  EXPECT_EQ(0, module1->SetSendingStatus(true));

  // Start basic RTP test.

  // Send an empty RTP packet.
  // Should fail since we have not registerd the payload type.
  EXPECT_EQ(-1, module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                          96, 0, NULL, 0));

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

  const WebRtc_UWord8 test[5] = "test";
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                         0, test, 4));

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
  WebRtc_Word8 red = 0;
  EXPECT_EQ(0, module1->SendREDPayloadType(red));
  EXPECT_EQ(voiceCodec.pltype, red);
  EXPECT_EQ(0, module1->RegisterReceivePayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  RTPFragmentationHeader fragmentation;
  fragmentation.fragmentationVectorSize = 2;
  fragmentation.fragmentationLength = new WebRtc_UWord32[2];
  fragmentation.fragmentationLength[0] = 4;
  fragmentation.fragmentationLength[1] = 4;
  fragmentation.fragmentationOffset = new WebRtc_UWord32[2];
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationOffset[1] = 4;
  fragmentation.fragmentationTimeDiff = new WebRtc_UWord16[2];
  fragmentation.fragmentationTimeDiff[0] = 0;
  fragmentation.fragmentationTimeDiff[1] = 0;
  fragmentation.fragmentationPlType = new WebRtc_UWord8[2];
  fragmentation.fragmentationPlType[0] = 96;
  fragmentation.fragmentationPlType[1] = 96;

  const WebRtc_UWord8 test[5] = "test";
  // Send a RTP packet.
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech,
                                         96, 160, test, 4,
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

  AudioFeedback* audioFeedback = new AudioFeedback();
  EXPECT_EQ(0, module2->RegisterAudioCallback(audioFeedback));

  // Prepare for DTMF.
  voiceCodec.pltype = 97;
  voiceCodec.plfreq = 8000;
  memcpy(voiceCodec.plname, "telephone-event", 16);

  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  // Start DTMF test.
  WebRtc_UWord32 timeStamp = 160;

  // Send a DTMF tone using RFC 2833 (4733).
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(0, module1->SendTelephoneEventOutband(i, timeStamp, 10));
  }
  timeStamp += 160;  // Prepare for next packet.

  const WebRtc_UWord8 test[9] = "test";

  // Send RTP packets for 16 tones a 160 ms  100ms
  // pause between = 2560ms + 1600ms = 4160ms
  for (;timeStamp <= 250 * 160; timeStamp += 160) {
    EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                           timeStamp, test, 4));
    fake_clock.IncrementTime(20);
    module1->Process();
  }
  EXPECT_EQ(0, module1->SendTelephoneEventOutband(32, 9000, 10));

  for (;timeStamp <= 740 * 160; timeStamp += 160) {
    EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 96,
                                           timeStamp, test, 4));
    fake_clock.IncrementTime(20);
    module1->Process();
  }
  delete audioFeedback;
}

TEST_F(RtpRtcpAudioTest, Stereo) {
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

  // Prepare for 3 channel audio 8 bits per sample.
  voiceCodec.pltype = 98;
  voiceCodec.channels = 3;
  memcpy(voiceCodec.plname, "PCMA", 5);
  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  // Prepare for 3 channel audio 16 bits per sample.
  voiceCodec.pltype = 99;
  memcpy(voiceCodec.plname, "L16", 4);
  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  // Prepare for 3 channel audio 5 bits per sample.
  voiceCodec.pltype = 100;
  memcpy(voiceCodec.plname, "G726-40",8);
  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  // Prepare for 3 channel audio 3 bits per sample.
  voiceCodec.pltype = 101;
  memcpy(voiceCodec.plname, "G726-24",8);
  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  // Prepare for 3 channel audio 2 bits per sample.
  voiceCodec.pltype = 102;
  memcpy(voiceCodec.plname, "G726-16",8);
  EXPECT_EQ(0, module1->RegisterSendPayload(voiceCodec));
  EXPECT_EQ(0, module2->RegisterReceivePayload(voiceCodec));

  // Test sample based multi channel codec, 3 channels 8 bits.
  WebRtc_UWord8 test3channels[15] = "ttteeesssttt";
  WebRtc_UWord32 timeStamp = 160;
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 98,
                                         timeStamp, test3channels, 12));
  fake_clock.IncrementTime(20);
  module1->Process();
  timeStamp += 160;  // Prepare for next packet.

  // Test sample based multi channel codec, 3 channels 16 bits.
  const WebRtc_UWord8 test3channels16[13] = "teteteststst";
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 99,
                                         timeStamp, test3channels16, 12));
  fake_clock.IncrementTime(20);
  module1->Process();
  timeStamp += 160;  // Prepare for next packet.

  // Test sample based multi channel codec, 3 channels 5 bits.
  test3channels[0] = 0xf8;  // 5 ones 3 zeros.
  test3channels[1] = 0x2b;  // 2 zeros 5 10 1 one.
  test3channels[2] = 0xf0;  // 4 ones 4 zeros.
  test3channels[3] = 0x2b;  // 1 zero 5 01 2 ones.
  test3channels[4] = 0xe0;  // 3 ones 5 zeros.
  test3channels[5] = 0x0;
  test3channels[6] = 0x0;
  test3channels[7] = 0x0;
  test3channels[8] = 0x0;
  test3channels[9] = 0x0;
  test3channels[10] = 0x0;
  test3channels[11] = 0x0;
  test3channels[12] = 0x0;
  test3channels[13] = 0x0;
  test3channels[14] = 0x0;

  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 100,
                                         timeStamp, test3channels, 15));
  fake_clock.IncrementTime(20);
  module1->Process();
  timeStamp += 160;  // Prepare for next packet.

  // Test sample based multi channel codec, 3 channels 3 bits.
  test3channels[0] = 0xe2;  // 3 ones    3 zeros     2 10
  test3channels[1] = 0xf0;  // 1 1       3 ones      3 zeros     1 0
  test3channels[2] = 0xb8;  // 2 10      3 ones      3 zeros
  test3channels[3] = 0xa0;  // 3 101     5 zeros
  test3channels[4] = 0x0;
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 101,
                                         timeStamp, test3channels, 15));
  fake_clock.IncrementTime(20);
  module1->Process();
  timeStamp += 160;  // Prepare for next packet.

  // Test sample based multi channel codec, 3 channels 2 bits.
  test3channels[0] = 0xcb;  // 2 ones    2 zeros     2 10        2 ones
  test3channels[1] = 0x2c;  // 2 zeros   2 10        2 ones      2 zeros
  test3channels[2] = 0xb2;  // 2 10      2 ones      2 zeros     2 10
  test3channels[3] = 0xcb;  // 2 ones    2 zeros     2 10        2 ones
  test3channels[4] = 0x2c;  // 2 zeros   2 10        2 ones      2 zeros
  EXPECT_EQ(0, module1->SendOutgoingData(webrtc::kAudioFrameSpeech, 102,
                                         timeStamp, test3channels, 15));
}
