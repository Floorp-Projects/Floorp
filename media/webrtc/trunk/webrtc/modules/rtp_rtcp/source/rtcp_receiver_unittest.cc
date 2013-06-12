/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes unit tests for the RTCPReceiver.
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Note: This file has no directory. Lint warning must be ignored.
#include "common_types.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "modules/rtp_rtcp/source/rtcp_sender.h"
#include "modules/rtp_rtcp/source/rtcp_receiver.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl.h"

namespace webrtc {

namespace {  // Anonymous namespace; hide utility functions and classes.

// A very simple packet builder class for building RTCP packets.
class PacketBuilder {
 public:
  static const int kMaxPacketSize = 1024;

  PacketBuilder()
      : pos_(0),
        pos_of_len_(0) {
  }


  void Add8(uint8_t byte) {
    EXPECT_LT(pos_, kMaxPacketSize - 1);
    buffer_[pos_] = byte;
    ++ pos_;
  }

  void Add16(uint16_t word) {
    Add8(word >> 8);
    Add8(word & 0xFF);
  }

  void Add32(uint32_t word) {
    Add8(word >> 24);
    Add8((word >> 16) & 0xFF);
    Add8((word >> 8) & 0xFF);
    Add8(word & 0xFF);
  }

  void Add64(uint32_t upper_half, uint32_t lower_half) {
    Add32(upper_half);
    Add32(lower_half);
  }

  // Set the 5-bit value in the 1st byte of the header
  // and the payload type. Set aside room for the length field,
  // and make provision for backpatching it.
  // Note: No way to set the padding bit.
  void AddRtcpHeader(int payload, int format_or_count) {
    PatchLengthField();
    Add8(0x80 | (format_or_count & 0x1F));
    Add8(payload);
    pos_of_len_ = pos_;
    Add16(0xDEAD);  // Initialize length to "clearly illegal".
  }

  void AddTmmbrBandwidth(int mantissa, int exponent, int overhead) {
    // 6 bits exponent, 17 bits mantissa, 9 bits overhead.
    uint32_t word = 0;
    word |= (exponent << 26);
    word |= ((mantissa & 0x1FFFF) << 9);
    word |= (overhead & 0x1FF);
    Add32(word);
  }

  void AddSrPacket(uint32_t sender_ssrc) {
    AddRtcpHeader(200, 0);
    Add32(sender_ssrc);
    Add64(0x10203, 0x4050607);  // NTP timestamp
    Add32(0x10203);  // RTP timestamp
    Add32(0);  // Sender's packet count
    Add32(0);  // Sender's octet count
  }

  void AddRrPacket(uint32_t sender_ssrc, uint32_t rtp_ssrc,
                   uint32_t extended_max) {
    AddRtcpHeader(201, 1);
    Add32(sender_ssrc);
    Add32(rtp_ssrc);
    Add32(0);  // No loss.
    Add32(extended_max);
    Add32(0);  // Jitter.
    Add32(0);  // Last SR.
    Add32(0);  // Delay since last SR.
  }

  const uint8_t* packet() {
    PatchLengthField();
    return buffer_;
  }

  unsigned int length() {
    return pos_;
  }
 private:
  void PatchLengthField() {
    if (pos_of_len_ > 0) {
      // Backpatch the packet length. The client must have taken
      // care of proper padding to 32-bit words.
      int this_packet_length = (pos_ - pos_of_len_ - 2);
      ASSERT_EQ(0, this_packet_length % 4)
          << "Packets must be a multiple of 32 bits long"
          << " pos " << pos_ << " pos_of_len " << pos_of_len_;
      buffer_[pos_of_len_] = this_packet_length >> 10;
      buffer_[pos_of_len_+1] = (this_packet_length >> 2) & 0xFF;
      pos_of_len_ = 0;
    }
  }

  int pos_;
  // Where the length field of the current packet is.
  // Note that 0 is not a legal value, so is used for "uninitialized".
  int pos_of_len_;
  uint8_t buffer_[kMaxPacketSize];
};

// This test transport verifies that no functions get called.
class TestTransport : public Transport,
                      public RtpData {
 public:
  explicit TestTransport()
      : rtcp_receiver_(NULL) {
  }
  void SetRTCPReceiver(RTCPReceiver* rtcp_receiver) {
    rtcp_receiver_ = rtcp_receiver;
  }
  virtual int SendPacket(int /*ch*/, const void* /*data*/, int /*len*/) {
    ADD_FAILURE();  // FAIL() gives a compile error.
    return -1;
  }

  // Injects an RTCP packet into the receiver.
  virtual int SendRTCPPacket(int /* ch */, const void *packet, int packet_len) {
    ADD_FAILURE();
    return 0;
  }

  virtual int OnReceivedPayloadData(const uint8_t* payloadData,
                                    const uint16_t payloadSize,
                                    const WebRtcRTPHeader* rtpHeader) {
    ADD_FAILURE();
    return 0;
  }
  RTCPReceiver* rtcp_receiver_;
};

class RtcpReceiverTest : public ::testing::Test {
 protected:
  RtcpReceiverTest()
      : over_use_detector_options_(),
        system_clock_(1335900000),
        remote_bitrate_observer_(),
        remote_bitrate_estimator_(
            RemoteBitrateEstimator::Create(
                over_use_detector_options_,
                RemoteBitrateEstimator::kSingleStreamEstimation,
                &remote_bitrate_observer_,
                &system_clock_)) {
    test_transport_ = new TestTransport();

    RtpRtcp::Configuration configuration;
    configuration.id = 0;
    configuration.audio = false;
    configuration.clock = &system_clock_;
    configuration.outgoing_transport = test_transport_;
    configuration.remote_bitrate_estimator = remote_bitrate_estimator_.get();
    rtp_rtcp_impl_ = new ModuleRtpRtcpImpl(configuration);
    rtcp_receiver_ = new RTCPReceiver(0, &system_clock_, rtp_rtcp_impl_);
    test_transport_->SetRTCPReceiver(rtcp_receiver_);
  }
  ~RtcpReceiverTest() {
    delete rtcp_receiver_;
    delete rtp_rtcp_impl_;
    delete test_transport_;
  }

  // Injects an RTCP packet into the receiver.
  // Returns 0 for OK, non-0 for failure.
  int InjectRtcpPacket(const uint8_t* packet,
                        uint16_t packet_len) {
    RTCPUtility::RTCPParserV2 rtcpParser(packet,
                                         packet_len,
                                         true);  // Allow non-compound RTCP

    RTCPHelp::RTCPPacketInformation rtcpPacketInformation;
    int result = rtcp_receiver_->IncomingRTCPPacket(rtcpPacketInformation,
                                                    &rtcpParser);
    // The NACK list is on purpose not copied below as it isn't needed by the
    // test.
    rtcp_packet_info_.rtcpPacketTypeFlags =
        rtcpPacketInformation.rtcpPacketTypeFlags;
    rtcp_packet_info_.remoteSSRC = rtcpPacketInformation.remoteSSRC;
    rtcp_packet_info_.applicationSubType =
        rtcpPacketInformation.applicationSubType;
    rtcp_packet_info_.applicationName = rtcpPacketInformation.applicationName;
    rtcp_packet_info_.reportBlock = rtcpPacketInformation.reportBlock;
    rtcp_packet_info_.fractionLost = rtcpPacketInformation.fractionLost;
    rtcp_packet_info_.roundTripTime = rtcpPacketInformation.roundTripTime;
    rtcp_packet_info_.lastReceivedExtendedHighSeqNum =
        rtcpPacketInformation.lastReceivedExtendedHighSeqNum;
    rtcp_packet_info_.jitter = rtcpPacketInformation.jitter;
    rtcp_packet_info_.interArrivalJitter =
        rtcpPacketInformation.interArrivalJitter;
    rtcp_packet_info_.sliPictureId = rtcpPacketInformation.sliPictureId;
    rtcp_packet_info_.rpsiPictureId = rtcpPacketInformation.rpsiPictureId;
    rtcp_packet_info_.receiverEstimatedMaxBitrate =
        rtcpPacketInformation.receiverEstimatedMaxBitrate;
    rtcp_packet_info_.ntp_secs = rtcpPacketInformation.ntp_secs;
    rtcp_packet_info_.ntp_frac = rtcpPacketInformation.ntp_frac;
    rtcp_packet_info_.rtp_timestamp = rtcpPacketInformation.rtp_timestamp;
    return result;
  }

  OverUseDetectorOptions over_use_detector_options_;
  SimulatedClock system_clock_;
  ModuleRtpRtcpImpl* rtp_rtcp_impl_;
  RTCPReceiver* rtcp_receiver_;
  TestTransport* test_transport_;
  RTCPHelp::RTCPPacketInformation rtcp_packet_info_;
  MockRemoteBitrateObserver remote_bitrate_observer_;
  scoped_ptr<RemoteBitrateEstimator> remote_bitrate_estimator_;
};


TEST_F(RtcpReceiverTest, BrokenPacketIsIgnored) {
  const uint8_t bad_packet[] = {0, 0, 0, 0};
  EXPECT_EQ(0, InjectRtcpPacket(bad_packet, sizeof(bad_packet)));
  EXPECT_EQ(0U, rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, InjectSrPacket) {
  const uint32_t kSenderSsrc = 0x10203;
  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  // The parser will note the remote SSRC on a SR from other than his
  // expected peer, but will not flag that he's gotten a packet.
  EXPECT_EQ(kSenderSsrc, rtcp_packet_info_.remoteSSRC);
  EXPECT_EQ(0U,
            kRtcpSr & rtcp_packet_info_.rtcpPacketTypeFlags);
}

TEST_F(RtcpReceiverTest, ReceiveReportTimeout) {
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kSourceSsrc = 0x40506;
  const int64_t kRtcpIntervalMs = 1000;

  rtcp_receiver_->SetSSRC(kSourceSsrc);

  uint32_t sequence_number = 1234;
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);

  // No RR received, shouldn't trigger a timeout.
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a RR and advance the clock just enough to not trigger a timeout.
  PacketBuilder p1;
  p1.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number);
  EXPECT_EQ(0, InjectRtcpPacket(p1.packet(), p1.length()));
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs - 1);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a RR with the same extended max as the previous RR to trigger a
  // sequence number timeout, but not a RR timeout.
  PacketBuilder p2;
  p2.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number);
  EXPECT_EQ(0, InjectRtcpPacket(p2.packet(), p2.length()));
  system_clock_.AdvanceTimeMilliseconds(2);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_TRUE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Advance clock enough to trigger an RR timeout too.
  system_clock_.AdvanceTimeMilliseconds(3 * kRtcpIntervalMs);
  EXPECT_TRUE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));

  // We should only get one timeout even though we still haven't received a new
  // RR.
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Add a new RR with increase sequence number to reset timers.
  PacketBuilder p3;
  sequence_number++;
  p2.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number);
  EXPECT_EQ(0, InjectRtcpPacket(p2.packet(), p2.length()));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_FALSE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));

  // Verify we can get a timeout again once we've received new RR.
  system_clock_.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  PacketBuilder p4;
  p4.AddRrPacket(kSenderSsrc, kSourceSsrc, sequence_number);
  EXPECT_EQ(0, InjectRtcpPacket(p4.packet(), p4.length()));
  system_clock_.AdvanceTimeMilliseconds(kRtcpIntervalMs + 1);
  EXPECT_FALSE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
  EXPECT_TRUE(rtcp_receiver_->RtcpRrSequenceNumberTimeout(kRtcpIntervalMs));
  system_clock_.AdvanceTimeMilliseconds(2 * kRtcpIntervalMs);
  EXPECT_TRUE(rtcp_receiver_->RtcpRrTimeout(kRtcpIntervalMs));
}

TEST_F(RtcpReceiverTest, TmmbrReceivedWithNoIncomingPacket) {
  // This call is expected to fail because no data has arrived.
  EXPECT_EQ(-1, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrPacketAccepted) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  rtcp_receiver_->SetSSRC(kMediaFlowSsrc);  // Matches "media source" above.

  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  // TMMBR packet.
  p.AddRtcpHeader(205, 3);
  p.Add32(kSenderSsrc);
  p.Add32(kMediaRecipientSsrc);
  p.Add32(kMediaFlowSsrc);
  p.AddTmmbrBandwidth(30000, 0, 0);  // 30 Kbits/sec bandwidth, no overhead.

  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(1);
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(1, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.Tmmbr(0));
  EXPECT_EQ(kMediaRecipientSsrc, candidate_set.Ssrc(0));
}

TEST_F(RtcpReceiverTest, TmmbrPacketNotForUsIgnored) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  const uint32_t kOtherMediaFlowSsrc = 0x9999;

  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  // TMMBR packet.
  p.AddRtcpHeader(205, 3);
  p.Add32(kSenderSsrc);
  p.Add32(kMediaRecipientSsrc);
  p.Add32(kOtherMediaFlowSsrc);  // This SSRC is not what we're sending.
  p.AddTmmbrBandwidth(30000, 0, 0);

  rtcp_receiver_->SetSSRC(kMediaFlowSsrc);
  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrPacketZeroRateIgnored) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  rtcp_receiver_->SetSSRC(kMediaFlowSsrc);  // Matches "media source" above.

  PacketBuilder p;
  p.AddSrPacket(kSenderSsrc);
  // TMMBR packet.
  p.AddRtcpHeader(205, 3);
  p.Add32(kSenderSsrc);
  p.Add32(kMediaRecipientSsrc);
  p.Add32(kMediaFlowSsrc);
  p.AddTmmbrBandwidth(0, 0, 0);  // Rate zero.

  EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
  EXPECT_EQ(0, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
}

TEST_F(RtcpReceiverTest, TmmbrThreeConstraintsTimeOut) {
  const uint32_t kMediaFlowSsrc = 0x2040608;
  const uint32_t kSenderSsrc = 0x10203;
  const uint32_t kMediaRecipientSsrc = 0x101;
  rtcp_receiver_->SetSSRC(kMediaFlowSsrc);  // Matches "media source" above.

  // Inject 3 packets "from" kMediaRecipientSsrc, Ssrc+1, Ssrc+2.
  // The times of arrival are starttime + 0, starttime + 5 and starttime + 10.
  for (uint32_t ssrc = kMediaRecipientSsrc;
       ssrc < kMediaRecipientSsrc+3; ++ssrc) {
    PacketBuilder p;
    p.AddSrPacket(kSenderSsrc);
    // TMMBR packet.
    p.AddRtcpHeader(205, 3);
    p.Add32(kSenderSsrc);
    p.Add32(ssrc);
    p.Add32(kMediaFlowSsrc);
    p.AddTmmbrBandwidth(30000, 0, 0);  // 30 Kbits/sec bandwidth, no overhead.

    EXPECT_EQ(0, InjectRtcpPacket(p.packet(), p.length()));
    // 5 seconds between each packet.
    system_clock_.AdvanceTimeMilliseconds(5000);
  }
  // It is now starttime+15.
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  TMMBRSet candidate_set;
  candidate_set.VerifyAndAllocateSet(3);
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(3, 0, &candidate_set));
  EXPECT_LT(0U, candidate_set.Tmmbr(0));
  // We expect the timeout to be 25 seconds. Advance the clock by 12
  // seconds, timing out the first packet.
  system_clock_.AdvanceTimeMilliseconds(12000);
  // Odd behaviour: Just counting them does not trigger the timeout.
  EXPECT_EQ(3, rtcp_receiver_->TMMBRReceived(0, 0, NULL));
  // Odd behaviour: There's only one left after timeout, not 2.
  EXPECT_EQ(1, rtcp_receiver_->TMMBRReceived(3, 0, &candidate_set));
  EXPECT_EQ(kMediaRecipientSsrc + 2, candidate_set.Ssrc(0));
}


}  // Anonymous namespace

}  // namespace webrtc
