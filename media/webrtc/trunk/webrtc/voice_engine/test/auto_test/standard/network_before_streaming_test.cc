/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "after_initialization_fixture.h"

class NetworkBeforeStreamingTest : public AfterInitializationFixture {
 protected:
  void SetUp() {
    channel_ = voe_base_->CreateChannel();
  }

  void TearDown() {
    voe_base_->DeleteChannel(channel_);
  }

  int channel_;
};

TEST_F(NetworkBeforeStreamingTest,
    GetSourceInfoReturnsEmptyValuesForUnconfiguredChannel) {
  char src_ip[32] = "0.0.0.0";
  int src_rtp_port = 1234;
  int src_rtcp_port = 1235;

  EXPECT_EQ(0, voe_network_->GetSourceInfo(
      channel_, src_rtp_port, src_rtcp_port, src_ip));
  EXPECT_EQ(0, src_rtp_port);
  EXPECT_EQ(0, src_rtcp_port);
  EXPECT_STRCASEEQ("", src_ip);
}

TEST_F(NetworkBeforeStreamingTest,
    GetSourceFilterReturnsEmptyValuesForUnconfiguredChannel) {
  int filter_port = -1;
  int filter_port_rtcp = -1;
  char filter_ip[32] = "0.0.0.0";

  EXPECT_EQ(0, voe_network_->GetSourceFilter(
      channel_, filter_port, filter_port_rtcp, filter_ip));

  EXPECT_EQ(0, filter_port);
  EXPECT_EQ(0, filter_port_rtcp);
  EXPECT_STRCASEEQ("", filter_ip);
}

TEST_F(NetworkBeforeStreamingTest, SetSourceFilterSucceeds) {
  EXPECT_EQ(0, voe_network_->SetSourceFilter(channel_, 0));
}
