/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <list>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/mock/mock_rtp_receiver_video.h"
#include "modules/rtp_rtcp/source/receiver_fec.h"

using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

namespace webrtc {

class ReceiverFecTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    fec_ = new ForwardErrorCorrection(0);
    receiver_fec_ = new ReceiverFEC(0, &rtp_receiver_video_);
    generator_ = new FrameGenerator();
    receiver_fec_->SetPayloadTypeFEC(kFecPayloadType);
  }

  virtual void TearDown() {
    delete fec_;
    delete receiver_fec_;
    delete generator_;
  }

  void GenerateFEC(std::list<Packet*>* media_packets,
                   std::list<Packet*>* fec_packets,
                   unsigned int num_fec_packets) {
    EXPECT_EQ(0, fec_->GenerateFEC(
        *media_packets,
        num_fec_packets * 255 / media_packets->size(),
        0,
        false,
        kFecMaskBursty,
        fec_packets));
    ASSERT_EQ(num_fec_packets, fec_packets->size());
  }

  void GenerateFrame(int num_media_packets,
                     int frame_offset,
                     std::list<RtpPacket*>* media_rtp_packets,
                     std::list<Packet*>* media_packets) {
    generator_->NewFrame(num_media_packets);
    for (int i = 0; i < num_media_packets; ++i) {
      media_rtp_packets->push_back(generator_->NextPacket(frame_offset + i,
                                                          kRtpHeaderSize + 10));
      media_packets->push_back(media_rtp_packets->back());
    }
  }

  void VerifyReconstructedMediaPacket(const RtpPacket* packet, int times) {
    // Verify that the content of the reconstructed packet is equal to the
    // content of |packet|, and that the same content is received |times| number
    // of times in a row.
    EXPECT_CALL(rtp_receiver_video_,
                ReceiveRecoveredPacketCallback(_, _,
                                               packet->length - kRtpHeaderSize))
        .With(Args<1, 2>(ElementsAreArray(packet->data + kRtpHeaderSize,
                                          packet->length - kRtpHeaderSize)))
        .Times(times);
  }

  void BuildAndAddRedMediaPacket(RtpPacket* packet) {
    RtpPacket* red_packet = generator_->BuildMediaRedPacket(packet);
    bool is_fec = false;
    EXPECT_EQ(0, receiver_fec_->AddReceivedFECPacket(&red_packet->header,
                                                     red_packet->data,
                                                     red_packet->length -
                                                     kRtpHeaderSize,
                                                     is_fec));
    delete red_packet;
    EXPECT_FALSE(is_fec);
  }

  void BuildAndAddRedFecPacket(Packet* packet) {
    RtpPacket* red_packet = generator_->BuildFecRedPacket(packet);
    bool is_fec = false;
    EXPECT_EQ(0, receiver_fec_->AddReceivedFECPacket(&red_packet->header,
                                                     red_packet->data,
                                                     red_packet->length -
                                                     kRtpHeaderSize,
                                                     is_fec));
    delete red_packet;
    EXPECT_TRUE(is_fec);
  }

  ForwardErrorCorrection* fec_;
  MockRTPReceiverVideo rtp_receiver_video_;
  ReceiverFEC* receiver_fec_;
  FrameGenerator* generator_;
};

void DeletePackets(std::list<Packet*>* packets) {
  while (!packets->empty()) {
    delete packets->front();
    packets->pop_front();
  }
}

TEST_F(ReceiverFecTest, TwoMediaOneFec) {
  const unsigned int kNumFecPackets = 1u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
  std::list<RtpPacket*>::iterator media_it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(*media_it);
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  // Drop one media packet.
  std::list<Packet*>::iterator fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, TwoMediaTwoFec) {
  const unsigned int kNumFecPackets = 2u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(2, 0, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  // Drop both media packets.
  std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
  std::list<Packet*>::iterator fec_it = fec_packets.begin();
  BuildAndAddRedFecPacket(*fec_it);
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  ++fec_it;
  BuildAndAddRedFecPacket(*fec_it);
  ++it;
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, TwoFramesOneFec) {
  const unsigned int kNumFecPackets = 1u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(1, 0, &media_rtp_packets, &media_packets);
  GenerateFrame(1, 1, &media_rtp_packets, &media_packets);
  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(media_rtp_packets.front());
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  // Drop one media packet.
  BuildAndAddRedFecPacket(fec_packets.front());
  ++it;
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, OneCompleteOneUnrecoverableFrame) {
  const unsigned int kNumFecPackets = 1u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  GenerateFrame(1, 0, &media_rtp_packets, &media_packets);
  GenerateFrame(2, 1, &media_rtp_packets, &media_packets);

  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
  BuildAndAddRedMediaPacket(*it);  // First frame: one packet.
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  ++it;
  BuildAndAddRedMediaPacket(*it);  // First packet of second frame.
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, MaxFramesOneFec) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 48u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets; ++i) {
    GenerateFrame(1, i, &media_rtp_packets, &media_packets);
  }
  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets, &fec_packets, kNumFecPackets);

  // Recovery
  std::list<RtpPacket*>::iterator it = media_rtp_packets.begin();
  ++it;  // Drop first packet.
  for (; it != media_rtp_packets.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    VerifyReconstructedMediaPacket(*it, 1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  }
  BuildAndAddRedFecPacket(fec_packets.front());
  it = media_rtp_packets.begin();
  VerifyReconstructedMediaPacket(*it, 1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, TooManyFrames) {
  const unsigned int kNumFecPackets = 1u;
  const unsigned int kNumMediaPackets = 49u;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets; ++i) {
    GenerateFrame(1, i, &media_rtp_packets, &media_packets);
  }
  std::list<Packet*> fec_packets;
  EXPECT_EQ(-1, fec_->GenerateFEC(media_packets,
                                  kNumFecPackets * 255 / kNumMediaPackets,
                                  0,
                                  false,
                                  kFecMaskBursty,
                                  &fec_packets));

  DeletePackets(&media_packets);
}

TEST_F(ReceiverFecTest, PacketNotDroppedTooEarly) {
  // 1 frame with 2 media packets and one FEC packet. One media packet missing.
  // Delay the FEC packet.
  Packet* delayed_fec = NULL;
  const unsigned int kNumFecPacketsBatch1 = 1u;
  const unsigned int kNumMediaPacketsBatch1 = 2u;
  std::list<RtpPacket*> media_rtp_packets_batch1;
  std::list<Packet*> media_packets_batch1;
  GenerateFrame(kNumMediaPacketsBatch1, 0, &media_rtp_packets_batch1,
                &media_packets_batch1);
  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets_batch1, &fec_packets, kNumFecPacketsBatch1);

  BuildAndAddRedMediaPacket(media_rtp_packets_batch1.front());
  EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
      .Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  delayed_fec = fec_packets.front();

  // Fill the FEC decoder. No packets should be dropped.
  const unsigned int kNumMediaPacketsBatch2 = 46u;
  std::list<RtpPacket*> media_rtp_packets_batch2;
  std::list<Packet*> media_packets_batch2;
  for (unsigned int i = 0; i < kNumMediaPacketsBatch2; ++i) {
    GenerateFrame(1, i, &media_rtp_packets_batch2, &media_packets_batch2);
  }
  for (std::list<RtpPacket*>::iterator it = media_rtp_packets_batch2.begin();
      it != media_rtp_packets_batch2.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
        .Times(1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  }

  // Add the delayed FEC packet. One packet should be reconstructed.
  BuildAndAddRedFecPacket(delayed_fec);
  EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
      .Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets_batch1);
  DeletePackets(&media_packets_batch2);
}

TEST_F(ReceiverFecTest, PacketDroppedWhenTooOld) {
  // 1 frame with 2 media packets and one FEC packet. One media packet missing.
  // Delay the FEC packet.
  Packet* delayed_fec = NULL;
  const unsigned int kNumFecPacketsBatch1 = 1u;
  const unsigned int kNumMediaPacketsBatch1 = 2u;
  std::list<RtpPacket*> media_rtp_packets_batch1;
  std::list<Packet*> media_packets_batch1;
  GenerateFrame(kNumMediaPacketsBatch1, 0, &media_rtp_packets_batch1,
                &media_packets_batch1);
  std::list<Packet*> fec_packets;
  GenerateFEC(&media_packets_batch1, &fec_packets, kNumFecPacketsBatch1);

  BuildAndAddRedMediaPacket(media_rtp_packets_batch1.front());
  EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
      .Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  delayed_fec = fec_packets.front();

  // Fill the FEC decoder and force the last packet to be dropped.
  const unsigned int kNumMediaPacketsBatch2 = 48u;
  std::list<RtpPacket*> media_rtp_packets_batch2;
  std::list<Packet*> media_packets_batch2;
  for (unsigned int i = 0; i < kNumMediaPacketsBatch2; ++i) {
    GenerateFrame(1, i, &media_rtp_packets_batch2, &media_packets_batch2);
  }
  for (std::list<RtpPacket*>::iterator it = media_rtp_packets_batch2.begin();
      it != media_rtp_packets_batch2.end(); ++it) {
    BuildAndAddRedMediaPacket(*it);
    EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
        .Times(1);
    EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
  }

  // Add the delayed FEC packet. No packet should be reconstructed since the
  // first media packet of that frame has been dropped due to being too old.
  BuildAndAddRedFecPacket(delayed_fec);
  EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
      .Times(0);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets_batch1);
  DeletePackets(&media_packets_batch2);
}

TEST_F(ReceiverFecTest, OldFecPacketDropped) {
  // 49 frames with 2 media packets and one FEC packet. All media packets
  // missing.
  const unsigned int kNumMediaPackets = 49 * 2;
  std::list<RtpPacket*> media_rtp_packets;
  std::list<Packet*> media_packets;
  for (unsigned int i = 0; i < kNumMediaPackets / 2; ++i) {
    std::list<RtpPacket*> frame_media_rtp_packets;
    std::list<Packet*> frame_media_packets;
    std::list<Packet*> fec_packets;
    GenerateFrame(2, 0, &frame_media_rtp_packets, &frame_media_packets);
    GenerateFEC(&frame_media_packets, &fec_packets, 1);
    for (std::list<Packet*>::iterator it = fec_packets.begin();
        it != fec_packets.end(); ++it) {
      // Only FEC packets inserted. No packets recoverable at this time.
      BuildAndAddRedFecPacket(*it);
      EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
          .Times(0);
      EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());
    }
    media_packets.insert(media_packets.end(),
                         frame_media_packets.begin(),
                         frame_media_packets.end());
    media_rtp_packets.insert(media_rtp_packets.end(),
                             frame_media_rtp_packets.begin(),
                             frame_media_rtp_packets.end());
  }
  // Insert the oldest media packet. The corresponding FEC packet is too old
  // and should've been dropped. Only the media packet we inserted will be
  // returned.
  BuildAndAddRedMediaPacket(media_rtp_packets.front());
  EXPECT_CALL(rtp_receiver_video_, ReceiveRecoveredPacketCallback(_,_,_))
      .Times(1);
  EXPECT_EQ(0, receiver_fec_->ProcessReceivedFEC());

  DeletePackets(&media_packets);
}

}  // namespace webrtc
