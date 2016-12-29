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

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/system_wrappers/include/clock.h"
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
    DeliverPacket(MediaType::ANY, data, length, PacketTime());
    delete [] data;
  }

  MOCK_METHOD4(
      DeliverPacket,
      DeliveryStatus(MediaType, const uint8_t*, size_t, const PacketTime&));
};

class FakeNetworkPipeTest : public ::testing::Test {
 public:
  FakeNetworkPipeTest() : fake_clock_(12345) {}

 protected:
  virtual void SetUp() {
    receiver_.reset(new MockReceiver());
    ON_CALL(*receiver_, DeliverPacket(_, _, _, _))
        .WillByDefault(Return(PacketReceiver::DELIVERY_OK));
  }

  virtual void TearDown() {
  }

  void SendPackets(FakeNetworkPipe* pipe, int number_packets, int kPacketSize) {
    rtc::scoped_ptr<uint8_t[]> packet(new uint8_t[kPacketSize]);
    for (int i = 0; i < number_packets; ++i) {
      pipe->SendPacket(packet.get(), kPacketSize);
    }
  }

  int PacketTimeMs(int capacity_kbps, int kPacketSize) const {
    return 8 * kPacketSize / capacity_kbps;
  }

  SimulatedClock fake_clock_;
  rtc::scoped_ptr<MockReceiver> receiver_;
};

void DeleteMemory(uint8_t* data, int length) { delete [] data; }

// Test the capacity link and verify we get as many packets as we expect.
TEST_F(FakeNetworkPipeTest, CapacityTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.link_capacity_kbps = 80;
  rtc::scoped_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, config));
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
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();

  // Advance enough time to release one packet.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
  pipe->Process();

  // Release all but one packet
  fake_clock_.AdvanceTimeMilliseconds(9 * kPacketTimeMs - 1);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(8);
  pipe->Process();

  // And the last one.
  fake_clock_.AdvanceTimeMilliseconds(1);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
  pipe->Process();
}

// Test the extra network delay.
TEST_F(FakeNetworkPipeTest, ExtraDelayTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.queue_delay_ms = 100;
  config.link_capacity_kbps = 80;
  rtc::scoped_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, config));
  pipe->SetReceiver(receiver_.get());

  const int kNumPackets = 2;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets , kPacketSize);

  // Time to get one packet through the link.
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Increase more than kPacketTimeMs, but not more than the extra delay.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();

  // Advance the network delay to get the first packet.
  fake_clock_.AdvanceTimeMilliseconds(config.queue_delay_ms);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
  pipe->Process();

  // Advance one more kPacketTimeMs to get the last packet.
  fake_clock_.AdvanceTimeMilliseconds(kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
  pipe->Process();
}

// Test the number of buffers and packets are dropped when sending too many
// packets too quickly.
TEST_F(FakeNetworkPipeTest, QueueLengthTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 2;
  config.link_capacity_kbps = 80;
  rtc::scoped_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, config));
  pipe->SetReceiver(receiver_.get());

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);

  // Increase time enough to deliver all three packets, verify only two are
  // delivered.
  fake_clock_.AdvanceTimeMilliseconds(3 * kPacketTimeMs);
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(2);
  pipe->Process();
}

// Test we get statistics as expected.
TEST_F(FakeNetworkPipeTest, StatisticsTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 2;
  config.queue_delay_ms = 20;
  config.link_capacity_kbps = 80;
  rtc::scoped_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, config));
  pipe->SetReceiver(receiver_.get());

  const int kPacketSize = 1000;
  const int kPacketTimeMs = PacketTimeMs(config.link_capacity_kbps,
                                         kPacketSize);

  // Send three packets and verify only 2 are delivered.
  SendPackets(pipe.get(), 3, kPacketSize);
  fake_clock_.AdvanceTimeMilliseconds(3 * kPacketTimeMs +
                                      config.queue_delay_ms);

  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(2);
  pipe->Process();

  // Packet 1: kPacketTimeMs + config.queue_delay_ms,
  // packet 2: 2 * kPacketTimeMs + config.queue_delay_ms => 170 ms average.
  EXPECT_EQ(pipe->AverageDelay(), 170);
  EXPECT_EQ(pipe->sent_packets(), 2u);
  EXPECT_EQ(pipe->dropped_packets(), 1u);
  EXPECT_EQ(pipe->PercentageLoss(), 1/3.f);
}

// Change the link capacity half-way through the test and verify that the
// delivery times change accordingly.
TEST_F(FakeNetworkPipeTest, ChangingCapacityWithEmptyPipeTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.link_capacity_kbps = 80;
  rtc::scoped_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, config));
  pipe->SetReceiver(receiver_.get());

  // Add 10 packets of 1000 bytes, = 80 kb, and verify it takes one second to
  // get through the pipe.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  int packet_time_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_ms);
    EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
    pipe->Process();
  }

  // Change the capacity.
  config.link_capacity_kbps /= 2;  // Reduce to 50%.
  pipe->SetConfig(config);

  // Add another 10 packets of 1000 bytes, = 80 kb, and verify it takes two
  // seconds to get them through the pipe.
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link.
  packet_time_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_ms);
    EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
    pipe->Process();
  }

  // Check that all the packets were sent.
  EXPECT_EQ(static_cast<size_t>(2 * kNumPackets), pipe->sent_packets());
  fake_clock_.AdvanceTimeMilliseconds(pipe->TimeUntilNextProcess());
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();
}

// Change the link capacity half-way through the test and verify that the
// delivery times change accordingly.
TEST_F(FakeNetworkPipeTest, ChangingCapacityWithPacketsInPipeTest) {
  FakeNetworkPipe::Config config;
  config.queue_length_packets = 20;
  config.link_capacity_kbps = 80;
  rtc::scoped_ptr<FakeNetworkPipe> pipe(
      new FakeNetworkPipe(&fake_clock_, config));
  pipe->SetReceiver(receiver_.get());

  // Add 10 packets of 1000 bytes, = 80 kb.
  const int kNumPackets = 10;
  const int kPacketSize = 1000;
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link at the initial speed.
  int packet_time_1_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Change the capacity.
  config.link_capacity_kbps *= 2;  // Double the capacity.
  pipe->SetConfig(config);

  // Add another 10 packets of 1000 bytes, = 80 kb, and verify it takes two
  // seconds to get them through the pipe.
  SendPackets(pipe.get(), kNumPackets, kPacketSize);

  // Time to get one packet through the link at the new capacity.
  int packet_time_2_ms = PacketTimeMs(config.link_capacity_kbps, kPacketSize);

  // Time hasn't increased yet, so we souldn't get any packets.
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_1_ms);
    EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
    pipe->Process();
  }

  // Advance time in steps to release one packet at a time.
  for (int i = 0; i < kNumPackets; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(packet_time_2_ms);
    EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(1);
    pipe->Process();
  }

  // Check that all the packets were sent.
  EXPECT_EQ(static_cast<size_t>(2 * kNumPackets), pipe->sent_packets());
  fake_clock_.AdvanceTimeMilliseconds(pipe->TimeUntilNextProcess());
  EXPECT_CALL(*receiver_, DeliverPacket(_, _, _, _)).Times(0);
  pipe->Process();
}
}  // namespace webrtc
