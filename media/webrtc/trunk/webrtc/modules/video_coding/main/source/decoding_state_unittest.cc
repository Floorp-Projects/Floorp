/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/main/source/decoding_state.h"
#include "webrtc/modules/video_coding/main/source/frame_buffer.h"
#include "webrtc/modules/video_coding/main/source/jitter_buffer_common.h"
#include "webrtc/modules/video_coding/main/source/packet.h"

namespace webrtc {

TEST(TestDecodingState, Sanity) {
  VCMDecodingState dec_state;
  dec_state.Reset();
  EXPECT_TRUE(dec_state.in_initial_state());
  EXPECT_TRUE(dec_state.full_sync());
}

TEST(TestDecodingState, FrameContinuity) {
  VCMDecodingState dec_state;
  // Check that makes decision based on correct method.
  VCMFrameBuffer frame;
  VCMFrameBuffer frame_key;
  VCMPacket* packet = new VCMPacket();
  packet->isFirstPacket = true;
  packet->timestamp = 1;
  packet->seqNum = 0xffff;
  packet->frameType = kVideoFrameDelta;
  packet->codecSpecificHeader.codec = kRtpVideoVp8;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 0x007F;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  // Always start with a key frame.
  dec_state.Reset();
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  packet->frameType = kVideoFrameKey;
  EXPECT_LE(0, frame_key.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame_key));
  dec_state.SetState(&frame);
  frame.Reset();
  packet->frameType = kVideoFrameDelta;
  // Use pictureId
  packet->isFirstPacket = false;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 0x0002;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  frame.Reset();
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 0;
  packet->seqNum = 10;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));

  // Use sequence numbers.
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = kNoPictureId;
  frame.Reset();
  packet->seqNum = dec_state.sequence_num() - 1u;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  frame.Reset();
  packet->seqNum = dec_state.sequence_num() + 1u;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  // Insert another packet to this frame
  packet->seqNum++;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  // Verify wrap.
  EXPECT_LE(dec_state.sequence_num(), 0xffff);
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);

  // Insert packet with temporal info.
  dec_state.Reset();
  frame.Reset();
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 0;
  packet->seqNum = 1;
  packet->timestamp = 1;
  EXPECT_TRUE(dec_state.full_sync());
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  frame.Reset();
  // 1 layer up - still good.
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 1;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 1;
  packet->seqNum = 2;
  packet->timestamp = 2;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  frame.Reset();
  // Lost non-base layer packet => should update sync parameter.
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 3;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 3;
  packet->seqNum = 4;
  packet->timestamp = 4;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  // Now insert the next non-base layer (belonging to a next tl0PicId).
  frame.Reset();
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 1;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 2;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 4;
  packet->seqNum = 5;
  packet->timestamp = 5;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  // Checking continuity and not updating the state - this should not trigger
  // an update of sync state.
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());
  // Next base layer (dropped interim non-base layers) - should update sync.
  frame.Reset();
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 1;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 5;
  packet->seqNum = 6;
  packet->timestamp = 6;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());

  // Check wrap for temporal layers.
  frame.Reset();
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0x00FF;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 6;
  packet->seqNum = 7;
  packet->timestamp = 7;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());
  frame.Reset();
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0x0000;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 7;
  packet->seqNum = 8;
  packet->timestamp = 8;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  // The current frame is not continuous
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  delete packet;
}

TEST(TestDecodingState, UpdateOldPacket) {
  VCMDecodingState dec_state;
  // Update only if zero size and newer than previous.
  // Should only update if the timeStamp match.
  VCMFrameBuffer frame;
  VCMPacket* packet = new VCMPacket();
  packet->timestamp = 1;
  packet->seqNum = 1;
  packet->frameType = kVideoFrameDelta;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  EXPECT_EQ(dec_state.sequence_num(), 1);
  // Insert an empty packet that does not belong to the same frame.
  // => Sequence num should be the same.
  packet->timestamp = 2;
  dec_state.UpdateOldPacket(packet);
  EXPECT_EQ(dec_state.sequence_num(), 1);
  // Now insert empty packet belonging to the same frame.
  packet->timestamp = 1;
  packet->seqNum = 2;
  packet->frameType = kFrameEmpty;
  packet->sizeBytes = 0;
  dec_state.UpdateOldPacket(packet);
  EXPECT_EQ(dec_state.sequence_num(), 2);
  // Now insert delta packet belonging to the same frame.
  packet->timestamp = 1;
  packet->seqNum = 3;
  packet->frameType = kVideoFrameDelta;
  packet->sizeBytes = 1400;
  dec_state.UpdateOldPacket(packet);
  EXPECT_EQ(dec_state.sequence_num(), 3);
  // Insert a packet belonging to an older timestamp - should not update the
  // sequence number.
  packet->timestamp = 0;
  packet->seqNum = 4;
  packet->frameType = kFrameEmpty;
  packet->sizeBytes = 0;
  dec_state.UpdateOldPacket(packet);
  EXPECT_EQ(dec_state.sequence_num(), 3);

  delete packet;
}

TEST(TestDecodingState, MultiLayerBehavior) {
  // Identify sync/non-sync when more than one layer.
  VCMDecodingState dec_state;
  // Identify packets belonging to old frames/packets.
  // Set state for current frames.
  // tl0PicIdx 0, temporal id 0.
  VCMFrameBuffer frame;
  VCMPacket* packet = new VCMPacket();
  packet->frameType = kVideoFrameDelta;
  packet->codecSpecificHeader.codec = kRtpVideoVp8;
  packet->timestamp = 0;
  packet->seqNum = 0;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 0;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  // tl0PicIdx 0, temporal id 1.
  frame.Reset();
  packet->timestamp = 1;
  packet->seqNum = 1;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 1;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 1;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  // Lost tl0PicIdx 0, temporal id 2.
  // Insert tl0PicIdx 0, temporal id 3.
  frame.Reset();
  packet->timestamp = 3;
  packet->seqNum = 3;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 3;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 3;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());
  // Insert next base layer
  frame.Reset();
  packet->timestamp = 4;
  packet->seqNum = 4;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 1;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 4;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());
  // Insert key frame - should update sync value.
  // A key frame is always a base layer.
  frame.Reset();
  packet->frameType = kVideoFrameKey;
  packet->isFirstPacket = 1;
  packet->timestamp = 5;
  packet->seqNum = 5;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 2;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 5;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  // After sync, a continuous PictureId is required
  // (continuous base layer is not enough )
  frame.Reset();
  packet->frameType = kVideoFrameDelta;
  packet->timestamp = 6;
  packet->seqNum = 6;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 3;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 6;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());
  frame.Reset();
  packet->frameType = kVideoFrameDelta;
  packet->isFirstPacket = 1;
  packet->timestamp = 8;
  packet->seqNum = 8;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 4;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 8;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());

  // Insert a non-ref frame - should update sync value.
  frame.Reset();
  packet->frameType = kVideoFrameDelta;
  packet->isFirstPacket = 1;
  packet->timestamp = 9;
  packet->seqNum = 9;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 4;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 2;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 9;
  packet->codecSpecificHeader.codecHeader.VP8.layerSync = true;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  // The following test will verify the sync flag behavior after a loss.
  // Create the following pattern:
  // Update base layer, lose packet 1 (sync flag on, layer 2), insert packet 3
  // (sync flag on, layer 2) check continuity and sync flag after inserting
  // packet 2 (sync flag on, layer 1).
  // Base layer.
  frame.Reset();
  dec_state.Reset();
  packet->frameType = kVideoFrameDelta;
  packet->isFirstPacket = 1;
  packet->markerBit = 1;
  packet->timestamp = 0;
  packet->seqNum = 0;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 0;
  packet->codecSpecificHeader.codecHeader.VP8.layerSync = false;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());
  // Layer 2 - 2 packets (insert one, lose one).
  frame.Reset();
  packet->frameType = kVideoFrameDelta;
  packet->isFirstPacket = 1;
  packet->markerBit = 0;
  packet->timestamp = 1;
  packet->seqNum = 1;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 2;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 1;
  packet->codecSpecificHeader.codecHeader.VP8.layerSync = true;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_TRUE(dec_state.ContinuousFrame(&frame));
  // Layer 1
  frame.Reset();
  packet->frameType = kVideoFrameDelta;
  packet->isFirstPacket = 1;
  packet->markerBit = 1;
  packet->timestamp = 2;
  packet->seqNum = 3;
  packet->codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet->codecSpecificHeader.codecHeader.VP8.temporalIdx = 1;
  packet->codecSpecificHeader.codecHeader.VP8.pictureId = 2;
  packet->codecSpecificHeader.codecHeader.VP8.layerSync = true;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  EXPECT_TRUE(dec_state.full_sync());

  delete packet;
}

TEST(TestDecodingState, DiscontinuousPicIdContinuousSeqNum) {
  VCMDecodingState dec_state;
  VCMFrameBuffer frame;
  VCMPacket packet;
  frame.Reset();
  packet.frameType = kVideoFrameKey;
  packet.codecSpecificHeader.codec = kRtpVideoVp8;
  packet.timestamp = 0;
  packet.seqNum = 0;
  packet.codecSpecificHeader.codecHeader.VP8.tl0PicIdx = 0;
  packet.codecSpecificHeader.codecHeader.VP8.temporalIdx = 0;
  packet.codecSpecificHeader.codecHeader.VP8.pictureId = 0;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  EXPECT_TRUE(dec_state.full_sync());

  // Continuous sequence number but discontinuous picture id. This implies a
  // a loss and we have to fall back to only decoding the base layer.
  frame.Reset();
  packet.frameType = kVideoFrameDelta;
  packet.timestamp += 3000;
  ++packet.seqNum;
  packet.codecSpecificHeader.codecHeader.VP8.temporalIdx = 1;
  packet.codecSpecificHeader.codecHeader.VP8.pictureId = 2;
  EXPECT_LE(0, frame.InsertPacket(packet, 0, kNoErrors, frame_data));
  EXPECT_FALSE(dec_state.ContinuousFrame(&frame));
  dec_state.SetState(&frame);
  EXPECT_FALSE(dec_state.full_sync());
}

TEST(TestDecodingState, OldInput) {
  VCMDecodingState dec_state;
  // Identify packets belonging to old frames/packets.
  // Set state for current frames.
  VCMFrameBuffer frame;
  VCMPacket* packet = new VCMPacket();
  packet->timestamp = 10;
  packet->seqNum = 1;
  FrameData frame_data;
  frame_data.rtt_ms = 0;
  frame_data.rolling_average_packets_per_frame = -1;
  EXPECT_LE(0, frame.InsertPacket(*packet, 0, kNoErrors, frame_data));
  dec_state.SetState(&frame);
  packet->timestamp = 9;
  EXPECT_TRUE(dec_state.IsOldPacket(packet));
  // Check for old frame
  frame.Reset();
  frame.InsertPacket(*packet, 0, kNoErrors, frame_data);
  EXPECT_TRUE(dec_state.IsOldFrame(&frame));


  delete packet;
}
}  // namespace webrtc
