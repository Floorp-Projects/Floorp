/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/fileutils.h"
#include "voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "voice_engine/test/auto_test/voe_standard_test.h"

class TestRtpObserver : public webrtc::VoERTPObserver {
 public:
  TestRtpObserver();
  virtual ~TestRtpObserver();
  virtual void OnIncomingCSRCChanged(const int channel,
                                     const unsigned int CSRC,
                                     const bool added);
  virtual void OnIncomingSSRCChanged(const int channel,
                                     const unsigned int SSRC);
  void Reset();
 public:
  unsigned int ssrc_[2];
  unsigned int csrc_[2][2];  // Stores 2 CSRCs for each channel.
  bool added_[2][2];
  int size_[2];
};

TestRtpObserver::TestRtpObserver() {
  Reset();
}

TestRtpObserver::~TestRtpObserver() {
}

void TestRtpObserver::Reset() {
  for (int i = 0; i < 2; i++) {
    ssrc_[i] = 0;
    csrc_[i][0] = 0;
    csrc_[i][1] = 0;
    added_[i][0] = false;
    added_[i][1] = false;
    size_[i] = 0;
  }
}

void TestRtpObserver::OnIncomingCSRCChanged(const int channel,
                                            const unsigned int CSRC,
                                            const bool added) {
  char msg[128];
  sprintf(msg, "=> OnIncomingCSRCChanged(channel=%d, CSRC=%u, added=%d)\n",
          channel, CSRC, added);
  TEST_LOG("%s", msg);

  if (channel > 1)
    return;  // Not enough memory.

  csrc_[channel][size_[channel]] = CSRC;
  added_[channel][size_[channel]] = added;

  size_[channel]++;
  if (size_[channel] == 2)
    size_[channel] = 0;
}

void TestRtpObserver::OnIncomingSSRCChanged(const int channel,
                                            const unsigned int SSRC) {
  char msg[128];
  sprintf(msg, "\n=> OnIncomingSSRCChanged(channel=%d, SSRC=%u)\n", channel,
          SSRC);
  TEST_LOG("%s", msg);

  ssrc_[channel] = SSRC;
}

class RtcpAppHandler : public webrtc::VoERTCPObserver {
 public:
  void OnApplicationDataReceived(const int channel,
                                 const unsigned char sub_type,
                                 const unsigned int name,
                                 const unsigned char* data,
                                 const unsigned short length_in_bytes);
  void Reset();
  ~RtcpAppHandler() {}
  unsigned short length_in_bytes_;
  unsigned char data_[256];
  unsigned char sub_type_;
  unsigned int name_;
};


static const char* const RTCP_CNAME = "Whatever";

class RtpRtcpTest : public AfterStreamingFixture {
 protected:
  void SetUp() {
    // We need a second channel for this test, so set it up.
    second_channel_ = voe_base_->CreateChannel();
    EXPECT_GE(second_channel_, 0);

    EXPECT_EQ(0, voe_base_->SetSendDestination(
        second_channel_, 8002, "127.0.0.1"));
    EXPECT_EQ(0, voe_base_->SetLocalReceiver(
        second_channel_, 8002));
    EXPECT_EQ(0, voe_base_->StartReceive(second_channel_));
    EXPECT_EQ(0, voe_base_->StartPlayout(second_channel_));
    EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(second_channel_, 5678));
    EXPECT_EQ(0, voe_base_->StartSend(second_channel_));

    // We'll set up the RTCP CNAME and SSRC to something arbitrary here.
    voe_rtp_rtcp_->SetRTCP_CNAME(channel_, RTCP_CNAME);
  }

  void TearDown() {
    voe_base_->DeleteChannel(second_channel_);
  }

  int second_channel_;
};

void RtcpAppHandler::OnApplicationDataReceived(
    const int /*channel*/, const unsigned char sub_type,
    const unsigned int name, const unsigned char* data,
    const unsigned short length_in_bytes) {
  length_in_bytes_ = length_in_bytes;
  memcpy(data_, &data[0], length_in_bytes);
  sub_type_ = sub_type;
  name_ = name;
}

void RtcpAppHandler::Reset() {
  length_in_bytes_ = 0;
  memset(data_, 0, sizeof(data_));
  sub_type_ = 0;
  name_ = 0;
}

TEST_F(RtpRtcpTest, RemoteRtcpCnameHasPropagatedToRemoteSide) {
  if (!FLAGS_include_timing_dependent_tests) {
    TEST_LOG("Skipping test - running in slow execution environment...\n");
    return;
  }

  // We need to sleep a bit here for the name to propagate. For instance,
  // 200 milliseconds is not enough, so we'll go with one second here.
  Sleep(1000);

  char char_buffer[256];
  voe_rtp_rtcp_->GetRemoteRTCP_CNAME(channel_, char_buffer);
  EXPECT_STREQ(RTCP_CNAME, char_buffer);
}

TEST_F(RtpRtcpTest, SSRCPropagatesCorrectly) {
  unsigned int local_ssrc = 1234;
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(channel_, local_ssrc));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  Sleep(1000);

  unsigned int ssrc;
  EXPECT_EQ(0, voe_rtp_rtcp_->GetLocalSSRC(channel_, ssrc));
  EXPECT_EQ(local_ssrc, ssrc);

  EXPECT_EQ(0, voe_rtp_rtcp_->GetRemoteSSRC(channel_, ssrc));
  EXPECT_EQ(local_ssrc, ssrc);
}

TEST_F(RtpRtcpTest, RtcpApplicationDefinedPacketsCanBeSentAndReceived) {
  RtcpAppHandler rtcp_app_handler;
  EXPECT_EQ(0, voe_rtp_rtcp_->RegisterRTCPObserver(
      channel_, rtcp_app_handler));

  // Send data aligned to 32 bytes.
  const char* data = "application-dependent data------";
  unsigned short data_length = strlen(data);
  unsigned int data_name = 0x41424344;  // 'ABCD' in ascii
  unsigned char data_subtype = 1;

  EXPECT_EQ(0, voe_rtp_rtcp_->SendApplicationDefinedRTCPPacket(
      channel_, data_subtype, data_name, data, data_length));

  // Ensure the RTP-RTCP process gets scheduled.
  Sleep(1000);

  // Ensure we received the data in the callback.
  EXPECT_EQ(data_length, rtcp_app_handler.length_in_bytes_);
  EXPECT_EQ(0, memcmp(data, rtcp_app_handler.data_, data_length));
  EXPECT_EQ(data_name, rtcp_app_handler.name_);
  EXPECT_EQ(data_subtype, rtcp_app_handler.sub_type_);

  EXPECT_EQ(0, voe_rtp_rtcp_->DeRegisterRTCPObserver(channel_));
}

TEST_F(RtpRtcpTest, DisabledRtcpObserverDoesNotReceiveData) {
  RtcpAppHandler rtcp_app_handler;
  EXPECT_EQ(0, voe_rtp_rtcp_->RegisterRTCPObserver(
      channel_, rtcp_app_handler));

  // Put observer in a known state before de-registering.
  rtcp_app_handler.Reset();

  EXPECT_EQ(0, voe_rtp_rtcp_->DeRegisterRTCPObserver(channel_));

  const char* data = "whatever";
  EXPECT_EQ(0, voe_rtp_rtcp_->SendApplicationDefinedRTCPPacket(
      channel_, 1, 0x41424344, data, strlen(data)));

  // Ensure the RTP-RTCP process gets scheduled.
  Sleep(1000);

  // Ensure we received no data.
  EXPECT_EQ(0u, rtcp_app_handler.name_);
  EXPECT_EQ(0u, rtcp_app_handler.sub_type_);
}

TEST_F(RtpRtcpTest, InsertExtraRTPPacketDealsWithInvalidArguments) {
  const char payload_data[8] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };

  EXPECT_EQ(-1, voe_rtp_rtcp_->InsertExtraRTPPacket(
      -1, 0, false, payload_data, 8)) <<
          "Should reject: invalid channel.";
  EXPECT_EQ(-1, voe_rtp_rtcp_->InsertExtraRTPPacket(
      channel_, -1, false, payload_data, 8)) <<
          "Should reject: invalid payload type.";
  EXPECT_EQ(-1, voe_rtp_rtcp_->InsertExtraRTPPacket(
      channel_, 128, false, payload_data, 8)) <<
          "Should reject: invalid payload type.";
  EXPECT_EQ(-1, voe_rtp_rtcp_->InsertExtraRTPPacket(
        channel_, 99, false, NULL, 8)) <<
            "Should reject: bad pointer.";
  EXPECT_EQ(-1, voe_rtp_rtcp_->InsertExtraRTPPacket(
        channel_, 99, false, payload_data, 1500 - 28 + 1)) <<
            "Should reject: invalid size.";
}

TEST_F(RtpRtcpTest, CanTransmitExtraRtpPacketsWithoutError) {
  const char payload_data[8] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H' };

  for (int i = 0; i < 128; ++i) {
    // Try both with and without the marker bit set
    EXPECT_EQ(0, voe_rtp_rtcp_->InsertExtraRTPPacket(
        channel_, i, false, payload_data, 8));
    EXPECT_EQ(0, voe_rtp_rtcp_->InsertExtraRTPPacket(
        channel_, i, true, payload_data, 8));
  }
}

// TODO(xians, phoglund): Re-enable when issue 372 is resolved.
TEST_F(RtpRtcpTest, DISABLED_CanCreateRtpDumpFilesWithoutError) {
  // Create two RTP dump files (3 seconds long). You can verify these after
  // the test using rtpplay or NetEqRTPplay if you like.
  std::string output_path = webrtc::test::OutputPath();
  std::string incoming_filename = output_path + "dump_in_3sec.rtp";
  std::string outgoing_filename = output_path + "dump_out_3sec.rtp";

  EXPECT_EQ(0, voe_rtp_rtcp_->StartRTPDump(
      channel_, incoming_filename.c_str(), webrtc::kRtpIncoming));
  EXPECT_EQ(0, voe_rtp_rtcp_->StartRTPDump(
      channel_, outgoing_filename.c_str(), webrtc::kRtpOutgoing));

  Sleep(3000);

  EXPECT_EQ(0, voe_rtp_rtcp_->StopRTPDump(channel_, webrtc::kRtpIncoming));
  EXPECT_EQ(0, voe_rtp_rtcp_->StopRTPDump(channel_, webrtc::kRtpOutgoing));
}

TEST_F(RtpRtcpTest, ObserverGetsNotifiedOnSsrcChange) {
  TestRtpObserver rtcp_observer;
  EXPECT_EQ(0, voe_rtp_rtcp_->RegisterRTPObserver(
      channel_, rtcp_observer));
  rtcp_observer.Reset();

  unsigned int new_ssrc = 7777;
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(channel_, new_ssrc));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  Sleep(500);

  // Verify we got the new SSRC.
  EXPECT_EQ(new_ssrc, rtcp_observer.ssrc_[0]);

  // Now try another SSRC.
  unsigned int newer_ssrc = 1717;
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetLocalSSRC(channel_, newer_ssrc));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  Sleep(500);

  EXPECT_EQ(newer_ssrc, rtcp_observer.ssrc_[0]);

  EXPECT_EQ(0, voe_rtp_rtcp_->DeRegisterRTPObserver(channel_));
}
