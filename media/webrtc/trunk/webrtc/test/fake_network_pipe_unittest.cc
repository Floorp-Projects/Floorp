/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/call.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/fake_network_pipe.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::Invoke;

namespace webrtc {

class MockReceiver : public PacketReceiver {
 public:
  MockReceiver() {}
  virtual ~MockReceiver() {}

  void IncomingPacket(const uint8_t* data, size_t length) {
    DeliverPacket(data, length);
    delete [] data;
  }

  MOCK_METHOD2(DeliverPacket, bool(const uint8_t*, size_t));
};

class FakeNetworkPipeTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    TickTime::UseFakeClock(12345);
    receiver_.reset(new MockReceiver());
  }

  virtual void TearDown() {
  }

  void SendPackets(FakeNetworkPipe* pipe, int number_packets, int kPacketSize) {
    scoped_array<uint8_t> packet(new uint8_t[kPacketSize]);
    for (int i = 0; i < number_packets; ++i) {
      pipe->SendPacket(packet.get(), kPacketSize);
    }
  }

  int PacketTimeMs(int capacity_kbps, int kPacketSize) {
    return 8 * kPacketSize / capacity_kbps;
  }

  scoped_ptr<MockReceiver> receiver_;
};

void DeleteMemory(uint8_t* data, int length) { delete [] data; }

// Test the capacity link and verify we get as many packets as we expect.
TEST_F(FakeNetworkPipeTest, CapacityTest) {
  FakeNetworkPipe::Config config;
  config.queue_length = 20;
  config.link_capacity_kbps = 80;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(config));
  pipe->SetReceiver(receiver_.get());

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets , kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Time haven't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(0);
  pipe->Process();

  // Advance enough time to release one packet.
  TickTime::AdvanceFakeClock(kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(1);
  pipe->Process();

  // Release all but one packet
  TickTime::AdvanceFakeClock(9 * kPacketTimeMs - 1);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(8);
  pipe->Process();

  // And the last one.
  TickTime::AdvanceFakeClock(1);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(1);
  pipe->Process();
}

// Test the extra network delay.
TEST_F(FakeNetworkPipeTest, ExtraDelayTest) {
  FakeNetworkPipe::Config config;
  config.queue_length = 20;
  config.queue_delay_ms = 100;
  config.link_capacity_kbps = 80;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(config));
  pipe->SetReceiver(receiver_.get());

  const int kNumPackets = 2;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets , kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Increase more than kPacketTimeMs, but not more than the extra delay.
  TickTime::AdvanceFakeClock(kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(0);
  pipe->Process();

  // Advance the network delay to get the first packet.
  TickTime::AdvanceFakeClock(config.queue_delay_ms);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(1);
  pipe->Process();

  // Advance one more kPacketTimeMs to get the last packet.
  TickTime::AdvanceFakeClock(kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(1);
  pipe->Process();
}

// Test the number of buffers and packets are dropped when sending too many
// packets too quickly.
TEST_F(FakeNetworkPipeTest, QueueLengthTest) {
  FakeNetworkPipe::Config config;
  config.queue_length = 2;
  config.link_capacity_kbps = 80;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(config));
  pipe->SetReceiver(receiver_.get());

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);

  // Increase time enough to deliver all three packets, verify only two are
  // delivered.
  TickTime::AdvanceFakeClock(3 * kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(2);
  pipe->Process();
}

// Test we get statistics as expected.
TEST_F(FakeNetworkPipeTest, StatisticsTest) {
  FakeNetworkPipe::Config config;
  config.queue_length = 2;
  config.queue_delay_ms = 20;
  config.link_capacity_kbps = 80;
  scoped_ptr<FakeNetworkPipe> pipe(new FakeNetworkPipe(config));
  pipe->SetReceiver(receiver_.get());

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);
  TickTime::AdvanceFakeClock(3 * kPacketTimeMs + config.queue_delay_ms);

  EXPECT_CALL(*receiver_, DeliverPacket(_, _))
      .Times(2);
  pipe->Process();

  // Packet 1: kPacketTimeMs + config.queue_delay_ms,
  // packet 2: 2 * kPacketTimeMs + config.queue_delay_ms => 170 ms average.
  EXPECT_EQ(pipe->AverageDelay(), 170);
  EXPECT_EQ(pipe->sent_packets(), 2u);
  EXPECT_EQ(pipe->dropped_packets(), 1u);
  EXPECT_EQ(pipe->PercentageLoss(), 1/3.f);
}

}  // namespace webrtc
