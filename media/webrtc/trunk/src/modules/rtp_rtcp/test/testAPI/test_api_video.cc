/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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

class RtpRtcpVideoTest : public ::testing::Test {
 protected:
  RtpRtcpVideoTest() {
    test_id = 123;
    test_ssrc = 3456;
    test_timestamp = 4567;
    test_sequence_number = 2345;
  }
  ~RtpRtcpVideoTest() {}

  virtual void SetUp() {
    video_module = RtpRtcp::CreateRtpRtcp(test_id, false, &fake_clock);
    EXPECT_EQ(0, video_module->InitReceiver());
    EXPECT_EQ(0, video_module->InitSender());
    EXPECT_EQ(0, video_module->SetRTCPStatus(kRtcpCompound));
    EXPECT_EQ(0, video_module->SetSSRC(test_ssrc));
    EXPECT_EQ(0, video_module->SetNACKStatus(kNackRtcp));
    EXPECT_EQ(0, video_module->SetStorePacketsStatus(true));
    EXPECT_EQ(0, video_module->SetSendingStatus(true));

    transport = new LoopBackTransport(video_module);
    EXPECT_EQ(0, video_module->RegisterSendTransport(transport));

    receiver = new RtpReceiver();
    EXPECT_EQ(0, video_module->RegisterIncomingDataCallback(receiver));

    VideoCodec video_codec;
    memset(&video_codec, 0, sizeof(video_codec));
    video_codec.plType = 123;
    memcpy(video_codec.plName, "I420", 5);

    EXPECT_EQ(0, video_module->RegisterSendPayload(video_codec));
    EXPECT_EQ(0, video_module->RegisterReceivePayload(video_codec));

    payload_data_length = sizeof(payload_data);

    for (int n = 0; n < payload_data_length; n++) {
      payload_data[n] = n%10;
    }
  }

  virtual void TearDown() {
    RtpRtcp::DestroyRtpRtcp(video_module);
    delete transport;
    delete receiver;
  }

  int test_id;
  RtpRtcp* video_module;
  LoopBackTransport* transport;
  RtpReceiver* receiver;
  WebRtc_UWord32 test_ssrc;
  WebRtc_UWord32 test_timestamp;
  WebRtc_UWord16 test_sequence_number;
  WebRtc_UWord8  payload_data[65000];
  int payload_data_length;
  FakeRtpRtcpClock fake_clock;
};

TEST_F(RtpRtcpVideoTest, BasicVideo) {
  WebRtc_UWord32 timestamp = 3000;
  EXPECT_EQ(0, video_module->SendOutgoingData(webrtc::kVideoFrameDelta, 123,
                                             timestamp,
                                             payload_data,
                                             payload_data_length));

}

