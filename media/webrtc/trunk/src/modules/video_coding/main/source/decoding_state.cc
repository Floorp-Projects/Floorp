/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/main/source/decoding_state.h"

#include "modules/video_coding/main/source/frame_buffer.h"
#include "modules/video_coding/main/source/jitter_buffer_common.h"
#include "modules/video_coding/main/source/packet.h"
#include "modules/interface/module_common_types.h"

namespace webrtc {

VCMDecodingState::VCMDecodingState()
    : sequence_num_(0),
      time_stamp_(0),
      picture_id_(kNoPictureId),
      temporal_id_(kNoTemporalIdx),
      tl0_pic_id_(kNoTl0PicIdx),
      full_sync_(true),
      init_(true) {}

VCMDecodingState::~VCMDecodingState() {}

void VCMDecodingState::Reset() {
  // TODO(mikhal): Verify - not always would want to reset the sync
  sequence_num_ = 0;
  time_stamp_ = 0;
  picture_id_ = kNoPictureId;
  temporal_id_ = kNoTemporalIdx;
  tl0_pic_id_ = kNoTl0PicIdx;
  full_sync_ = true;
  init_ = true;
}

uint32_t VCMDecodingState::time_stamp() const {
  return time_stamp_;
}

uint16_t VCMDecodingState::sequence_num() const {
  return sequence_num_;
}

bool VCMDecodingState::IsOldFrame(const VCMFrameBuffer* frame) const {
  assert(frame != NULL);
  if (init_)
    return false;
  return (LatestTimestamp(time_stamp_, frame->TimeStamp(), NULL)
          == time_stamp_);
}

bool VCMDecodingState::IsOldPacket(const VCMPacket* packet) const {
  assert(packet != NULL);
  if (init_)
    return false;
  return (LatestTimestamp(time_stamp_, packet->timestamp, NULL)
           == time_stamp_);
}

void VCMDecodingState::SetState(const VCMFrameBuffer* frame) {
  assert(frame != NULL && frame->GetHighSeqNum() >= 0);
  UpdateSyncState(frame);
  sequence_num_ = static_cast<uint16_t>(frame->GetHighSeqNum());
  time_stamp_ = frame->TimeStamp();
  picture_id_ = frame->PictureId();
  temporal_id_ = frame->TemporalId();
  tl0_pic_id_ = frame->Tl0PicId();
  init_ = false;
}

void VCMDecodingState::SetStateOneBack(const VCMFrameBuffer* frame) {
  assert(frame != NULL && frame->GetHighSeqNum() >= 0);
  sequence_num_ = static_cast<uint16_t>(frame->GetHighSeqNum()) - 1u;
  time_stamp_ = frame->TimeStamp() - 1u;
  temporal_id_ = frame->TemporalId();
  if (frame->PictureId() != kNoPictureId) {
    if (frame->PictureId() == 0)
      picture_id_ = 0x7FFF;
    else
      picture_id_ =  frame->PictureId() - 1;
  }
  if (frame->Tl0PicId() != kNoTl0PicIdx) {
    if (frame->Tl0PicId() == 0)
      tl0_pic_id_ = 0x00FF;
    else
      tl0_pic_id_ = frame->Tl0PicId() - 1;
  }
  init_ = false;
}

void VCMDecodingState::UpdateOldPacket(const VCMPacket* packet) {
  assert(packet != NULL);
  if (packet->timestamp == time_stamp_) {
    // Late packet belonging to the last decoded frame - make sure we update the
    // last decoded sequence number.
    sequence_num_ = LatestSequenceNumber(packet->seqNum, sequence_num_, NULL);
  }
}

void VCMDecodingState::SetSeqNum(uint16_t new_seq_num) {
  sequence_num_ = new_seq_num;
}

bool VCMDecodingState::init() const {
  return init_;
}

bool VCMDecodingState::full_sync() const {
  return full_sync_;
}

void VCMDecodingState::UpdateSyncState(const VCMFrameBuffer* frame) {
  if (init_)
    return;
  if (frame->TemporalId() == kNoTemporalIdx ||
      frame->Tl0PicId() == kNoTl0PicIdx) {
    full_sync_ = true;
  } else if (frame->FrameType() == kVideoFrameKey || frame->LayerSync()) {
    full_sync_ = true;
  } else if (full_sync_) {
    // Verify that we are still in sync.
    // Sync will be broken if continuity is true for layers but not for the
    // other methods (PictureId and SeqNum).
    if (!ContinuousPictureId(frame->PictureId()) &&
        !ContinuousSeqNum(static_cast<uint16_t>(frame->GetLowSeqNum()))) {
      // Non-layered methods have failed.
      full_sync_ = false;
    }
  }
}

bool VCMDecodingState::ContinuousFrame(const VCMFrameBuffer* frame) const {
  // Check continuity based on the following hierarchy:
  // - Temporal layers (stop here if out of sync).
  // - Picture Id when available.
  // - Sequence numbers.
  // Return true when in initial state.
  // Note that when a method is not applicable it will return false.
  assert(frame != NULL);
  if (init_)
    return true;

  if (!ContinuousLayer(frame->TemporalId(), frame->Tl0PicId())) {
    // Base layers are not continuous or temporal layers are inactive.
    // In the presence of temporal layers, check for Picture ID/sequence number
    // continuity if sync can be restored by this frame.
    if (!full_sync_ && !frame->LayerSync())
      return false;
    else if (!ContinuousPictureId(frame->PictureId()))
      return ContinuousSeqNum(static_cast<uint16_t>(frame->GetLowSeqNum()));
  }
  return true;
}

bool VCMDecodingState::ContinuousPictureId(int picture_id) const {
  // First, check if applicable.
  if (picture_id == kNoPictureId || picture_id_ == kNoPictureId)
    return false;

  int next_picture_id = picture_id_ + 1;
  if (picture_id < picture_id_) {
    // Wrap
    if (picture_id_ >= 0x80) {
      // 15 bits used for picture id
      return ((next_picture_id & 0x7FFF) == picture_id);
    } else {
      // 7 bits used for picture id
      return ((next_picture_id & 0x7F) == picture_id);
    }
  }
  // No wrap
  return (next_picture_id == picture_id);
}

bool VCMDecodingState::ContinuousSeqNum(uint16_t seq_num) const {
  return (seq_num == static_cast<uint16_t>(sequence_num_ + 1));
}

bool VCMDecodingState::ContinuousLayer(int temporal_id,
                                       int tl0_pic_id) const {
  // First, check if applicable.
  if (temporal_id == kNoTemporalIdx || tl0_pic_id == kNoTl0PicIdx)
    return false;
  // If this is the first frame to use temporal layers, make sure we start
  // from base.
  else if (tl0_pic_id_ == kNoTl0PicIdx && temporal_id_ == kNoTemporalIdx &&
           temporal_id == 0)
    return true;

  // Current implementation: Look for base layer continuity.
  if (temporal_id != 0)
    return false;
  return (static_cast<uint8_t>(tl0_pic_id_ + 1) == tl0_pic_id);
}

}  // namespace webrtc
