/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/session_info.h"

#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
namespace {
// Used in determining whether a frame is decodable.
enum {kRttThreshold = 100};  // Not decodable if Rtt is lower than this.

// Do not decode frames if the number of packets is between these two
// thresholds.
static const float kLowPacketPercentageThreshold = 0.2f;
static const float kHighPacketPercentageThreshold = 0.8f;

uint16_t BufferToUWord16(const uint8_t* dataBuffer) {
  return (dataBuffer[0] << 8) | dataBuffer[1];
}
}  // namespace

VCMSessionInfo::VCMSessionInfo()
    : session_nack_(false),
      complete_(false),
      decodable_(false),
      frame_type_(kVideoFrameDelta),
      packets_(),
      empty_seq_num_low_(-1),
      empty_seq_num_high_(-1),
      first_packet_seq_num_(-1),
      last_packet_seq_num_(-1) {
}

void VCMSessionInfo::UpdateDataPointers(const uint8_t* old_base_ptr,
                                        const uint8_t* new_base_ptr) {
  for (PacketIterator it = packets_.begin(); it != packets_.end(); ++it)
    if ((*it).dataPtr != NULL) {
      assert(old_base_ptr != NULL && new_base_ptr != NULL);
      (*it).dataPtr = new_base_ptr + ((*it).dataPtr - old_base_ptr);
    }
}

int VCMSessionInfo::LowSequenceNumber() const {
  if (packets_.empty())
    return empty_seq_num_low_;
  return packets_.front().seqNum;
}

int VCMSessionInfo::HighSequenceNumber() const {
  if (packets_.empty())
    return empty_seq_num_high_;
  if (empty_seq_num_high_ == -1)
    return packets_.back().seqNum;
  return LatestSequenceNumber(packets_.back().seqNum, empty_seq_num_high_);
}

int VCMSessionInfo::PictureId() const {
  if (packets_.empty() ||
      packets_.front().codecSpecificHeader.codec != kRtpVideoVp8)
    return kNoPictureId;
  return packets_.front().codecSpecificHeader.codecHeader.VP8.pictureId;
}

int VCMSessionInfo::TemporalId() const {
  if (packets_.empty() ||
      packets_.front().codecSpecificHeader.codec != kRtpVideoVp8)
    return kNoTemporalIdx;
  return packets_.front().codecSpecificHeader.codecHeader.VP8.temporalIdx;
}

bool VCMSessionInfo::LayerSync() const {
  if (packets_.empty() ||
        packets_.front().codecSpecificHeader.codec != kRtpVideoVp8)
    return false;
  return packets_.front().codecSpecificHeader.codecHeader.VP8.layerSync;
}

int VCMSessionInfo::Tl0PicId() const {
  if (packets_.empty() ||
      packets_.front().codecSpecificHeader.codec != kRtpVideoVp8)
    return kNoTl0PicIdx;
  return packets_.front().codecSpecificHeader.codecHeader.VP8.tl0PicIdx;
}

bool VCMSessionInfo::NonReference() const {
  if (packets_.empty() ||
      packets_.front().codecSpecificHeader.codec != kRtpVideoVp8)
    return false;
  return packets_.front().codecSpecificHeader.codecHeader.VP8.nonReference;
}

void VCMSessionInfo::Reset() {
  session_nack_ = false;
  complete_ = false;
  decodable_ = false;
  frame_type_ = kVideoFrameDelta;
  packets_.clear();
  empty_seq_num_low_ = -1;
  empty_seq_num_high_ = -1;
  first_packet_seq_num_ = -1;
  last_packet_seq_num_ = -1;
}

int VCMSessionInfo::SessionLength() const {
  int length = 0;
  for (PacketIteratorConst it = packets_.begin(); it != packets_.end(); ++it)
    length += (*it).sizeBytes;
  return length;
}

int VCMSessionInfo::NumPackets() const {
  return packets_.size();
}

int VCMSessionInfo::InsertBuffer(uint8_t* frame_buffer,
                                 PacketIterator packet_it) {
  VCMPacket& packet = *packet_it;
  PacketIterator it;

  // Calculate the offset into the frame buffer for this packet.
  int offset = 0;
  for (it = packets_.begin(); it != packet_it; ++it)
    offset += (*it).sizeBytes;

  // Set the data pointer to pointing to the start of this packet in the
  // frame buffer.
  const uint8_t* packet_buffer = packet.dataPtr;
  packet.dataPtr = frame_buffer + offset;

  // We handle H.264 STAP-A packets in a special way as we need to remove the
  // two length bytes between each NAL unit, and potentially add start codes.
  const size_t kH264NALHeaderLengthInBytes = 1;
  const size_t kLengthFieldLength = 2;
  if (packet.codecSpecificHeader.codec == kRtpVideoH264 &&
      packet.codecSpecificHeader.codecHeader.H264.stap_a) {
    size_t required_length = 0;
    const uint8_t* nalu_ptr = packet_buffer + kH264NALHeaderLengthInBytes;
    while (nalu_ptr < packet_buffer + packet.sizeBytes) {
      uint32_t length = BufferToUWord16(nalu_ptr);
      required_length +=
          length + (packet.insertStartCode ? kH264StartCodeLengthBytes : 0);
      nalu_ptr += kLengthFieldLength + length;
    }
    ShiftSubsequentPackets(packet_it, required_length);
    nalu_ptr = packet_buffer + kH264NALHeaderLengthInBytes;
    uint8_t* frame_buffer_ptr = frame_buffer + offset;
    while (nalu_ptr < packet_buffer + packet.sizeBytes) {
      uint32_t length = BufferToUWord16(nalu_ptr);
      nalu_ptr += kLengthFieldLength;
      frame_buffer_ptr += Insert(nalu_ptr,
                                 length,
                                 packet.insertStartCode,
                                 const_cast<uint8_t*>(frame_buffer_ptr));
      nalu_ptr += length;
    }
    packet.sizeBytes = required_length;
    return packet.sizeBytes;
  }
  ShiftSubsequentPackets(
      packet_it,
      packet.sizeBytes +
          (packet.insertStartCode ? kH264StartCodeLengthBytes : 0));

  packet.sizeBytes = Insert(packet_buffer,
                            packet.sizeBytes,
                            packet.insertStartCode,
                            const_cast<uint8_t*>(packet.dataPtr));
  return packet.sizeBytes;
}

size_t VCMSessionInfo::Insert(const uint8_t* buffer,
                              size_t length,
                              bool insert_start_code,
                              uint8_t* frame_buffer) {
  if (insert_start_code) {
    const unsigned char startCode[] = {0, 0, 0, 1};
    memcpy(frame_buffer, startCode, kH264StartCodeLengthBytes);
  }
  memcpy(frame_buffer + (insert_start_code ? kH264StartCodeLengthBytes : 0),
         buffer,
         length);
  length += (insert_start_code ? kH264StartCodeLengthBytes : 0);

  return length;
}

void VCMSessionInfo::ShiftSubsequentPackets(PacketIterator it,
                                            int steps_to_shift) {
  ++it;
  if (it == packets_.end())
    return;
  uint8_t* first_packet_ptr = const_cast<uint8_t*>((*it).dataPtr);
  int shift_length = 0;
  // Calculate the total move length and move the data pointers in advance.
  for (; it != packets_.end(); ++it) {
    shift_length += (*it).sizeBytes;
    if ((*it).dataPtr != NULL)
      (*it).dataPtr += steps_to_shift;
  }
  memmove(first_packet_ptr + steps_to_shift, first_packet_ptr, shift_length);
}

void VCMSessionInfo::UpdateCompleteSession() {
  if (HaveFirstPacket() && HaveLastPacket()) {
    // Do we have all the packets in this session?
    bool complete_session = true;
    PacketIterator it = packets_.begin();
    PacketIterator prev_it = it;
    ++it;
    for (; it != packets_.end(); ++it) {
      if (!InSequence(it, prev_it)) {
        complete_session = false;
        break;
      }
      prev_it = it;
    }
    complete_ = complete_session;
  }
}

void VCMSessionInfo::UpdateDecodableSession(const FrameData& frame_data) {
  // Irrelevant if session is already complete or decodable
  if (complete_ || decodable_)
    return;
  // TODO(agalusza): Account for bursty loss.
  // TODO(agalusza): Refine these values to better approximate optimal ones.
  if (frame_data.rtt_ms < kRttThreshold
      || frame_type_ == kVideoFrameKey
      || !HaveFirstPacket()
      || (NumPackets() <= kHighPacketPercentageThreshold
                          * frame_data.rolling_average_packets_per_frame
          && NumPackets() > kLowPacketPercentageThreshold
                            * frame_data.rolling_average_packets_per_frame))
    return;

  decodable_ = true;
}

bool VCMSessionInfo::complete() const {
  return complete_;
}

bool VCMSessionInfo::decodable() const {
  return decodable_;
}

// Find the end of the NAL unit which the packet pointed to by |packet_it|
// belongs to. Returns an iterator to the last packet of the frame if the end
// of the NAL unit wasn't found.
VCMSessionInfo::PacketIterator VCMSessionInfo::FindNaluEnd(
    PacketIterator packet_it) const {
  if ((*packet_it).completeNALU == kNaluEnd ||
      (*packet_it).completeNALU == kNaluComplete) {
    return packet_it;
  }
  // Find the end of the NAL unit.
  for (; packet_it != packets_.end(); ++packet_it) {
    if (((*packet_it).completeNALU == kNaluComplete &&
        (*packet_it).sizeBytes > 0) ||
        // Found next NALU.
        (*packet_it).completeNALU == kNaluStart)
      return --packet_it;
    if ((*packet_it).completeNALU == kNaluEnd)
      return packet_it;
  }
  // The end wasn't found.
  return --packet_it;
}

int VCMSessionInfo::DeletePacketData(PacketIterator start,
                                     PacketIterator end) {
  int bytes_to_delete = 0;  // The number of bytes to delete.
  PacketIterator packet_after_end = end;
  ++packet_after_end;

  // Get the number of bytes to delete.
  // Clear the size of these packets.
  for (PacketIterator it = start; it != packet_after_end; ++it) {
    bytes_to_delete += (*it).sizeBytes;
    (*it).sizeBytes = 0;
    (*it).dataPtr = NULL;
  }
  if (bytes_to_delete > 0)
    ShiftSubsequentPackets(end, -bytes_to_delete);
  return bytes_to_delete;
}

int VCMSessionInfo::BuildVP8FragmentationHeader(
    uint8_t* frame_buffer,
    int frame_buffer_length,
    RTPFragmentationHeader* fragmentation) {
  int new_length = 0;
  // Allocate space for max number of partitions
  fragmentation->VerifyAndAllocateFragmentationHeader(kMaxVP8Partitions);
  fragmentation->fragmentationVectorSize = 0;
  memset(fragmentation->fragmentationLength, 0,
         kMaxVP8Partitions * sizeof(uint32_t));
  if (packets_.empty())
      return new_length;
  PacketIterator it = FindNextPartitionBeginning(packets_.begin());
  while (it != packets_.end()) {
    const int partition_id =
        (*it).codecSpecificHeader.codecHeader.VP8.partitionId;
    PacketIterator partition_end = FindPartitionEnd(it);
    fragmentation->fragmentationOffset[partition_id] =
        (*it).dataPtr - frame_buffer;
    assert(fragmentation->fragmentationOffset[partition_id] <
           static_cast<uint32_t>(frame_buffer_length));
    fragmentation->fragmentationLength[partition_id] =
        (*partition_end).dataPtr + (*partition_end).sizeBytes - (*it).dataPtr;
    assert(fragmentation->fragmentationLength[partition_id] <=
           static_cast<uint32_t>(frame_buffer_length));
    new_length += fragmentation->fragmentationLength[partition_id];
    ++partition_end;
    it = FindNextPartitionBeginning(partition_end);
    if (partition_id + 1 > fragmentation->fragmentationVectorSize)
      fragmentation->fragmentationVectorSize = partition_id + 1;
  }
  // Set all empty fragments to start where the previous fragment ends,
  // and have zero length.
  if (fragmentation->fragmentationLength[0] == 0)
      fragmentation->fragmentationOffset[0] = 0;
  for (int i = 1; i < fragmentation->fragmentationVectorSize; ++i) {
    if (fragmentation->fragmentationLength[i] == 0)
      fragmentation->fragmentationOffset[i] =
          fragmentation->fragmentationOffset[i - 1] +
          fragmentation->fragmentationLength[i - 1];
    assert(i == 0 ||
           fragmentation->fragmentationOffset[i] >=
           fragmentation->fragmentationOffset[i - 1]);
  }
  assert(new_length <= frame_buffer_length);
  return new_length;
}

VCMSessionInfo::PacketIterator VCMSessionInfo::FindNextPartitionBeginning(
    PacketIterator it) const {
  while (it != packets_.end()) {
    if ((*it).codecSpecificHeader.codecHeader.VP8.beginningOfPartition) {
      return it;
    }
    ++it;
  }
  return it;
}

VCMSessionInfo::PacketIterator VCMSessionInfo::FindPartitionEnd(
    PacketIterator it) const {
  assert((*it).codec == kVideoCodecVP8);
  PacketIterator prev_it = it;
  const int partition_id =
      (*it).codecSpecificHeader.codecHeader.VP8.partitionId;
  while (it != packets_.end()) {
    bool beginning =
        (*it).codecSpecificHeader.codecHeader.VP8.beginningOfPartition;
    int current_partition_id =
        (*it).codecSpecificHeader.codecHeader.VP8.partitionId;
    bool packet_loss_found = (!beginning && !InSequence(it, prev_it));
    if (packet_loss_found ||
        (beginning && current_partition_id != partition_id)) {
      // Missing packet, the previous packet was the last in sequence.
      return prev_it;
    }
    prev_it = it;
    ++it;
  }
  return prev_it;
}

bool VCMSessionInfo::InSequence(const PacketIterator& packet_it,
                                const PacketIterator& prev_packet_it) {
  // If the two iterators are pointing to the same packet they are considered
  // to be in sequence.
  return (packet_it == prev_packet_it ||
      (static_cast<uint16_t>((*prev_packet_it).seqNum + 1) ==
          (*packet_it).seqNum));
}

int VCMSessionInfo::MakeDecodable() {
  int return_length = 0;
  if (packets_.empty()) {
    return 0;
  }
  PacketIterator it = packets_.begin();
  // Make sure we remove the first NAL unit if it's not decodable.
  if ((*it).completeNALU == kNaluIncomplete ||
      (*it).completeNALU == kNaluEnd) {
    PacketIterator nalu_end = FindNaluEnd(it);
    return_length += DeletePacketData(it, nalu_end);
    it = nalu_end;
  }
  PacketIterator prev_it = it;
  // Take care of the rest of the NAL units.
  for (; it != packets_.end(); ++it) {
    bool start_of_nalu = ((*it).completeNALU == kNaluStart ||
        (*it).completeNALU == kNaluComplete);
    if (!start_of_nalu && !InSequence(it, prev_it)) {
      // Found a sequence number gap due to packet loss.
      PacketIterator nalu_end = FindNaluEnd(it);
      return_length += DeletePacketData(it, nalu_end);
      it = nalu_end;
    }
    prev_it = it;
  }
  return return_length;
}

void VCMSessionInfo::SetNotDecodableIfIncomplete() {
  // We don't need to check for completeness first because the two are
  // orthogonal. If complete_ is true, decodable_ is irrelevant.
  decodable_ = false;
}

bool
VCMSessionInfo::HaveFirstPacket() const {
  return !packets_.empty() && (first_packet_seq_num_ != -1);
}

bool
VCMSessionInfo::HaveLastPacket() const {
  return !packets_.empty() && (last_packet_seq_num_ != -1);
}

bool
VCMSessionInfo::session_nack() const {
  return session_nack_;
}

int VCMSessionInfo::InsertPacket(const VCMPacket& packet,
                                 uint8_t* frame_buffer,
                                 VCMDecodeErrorMode decode_error_mode,
                                 const FrameData& frame_data) {
  if (packet.frameType == kFrameEmpty) {
    // Update sequence number of an empty packet.
    // Only media packets are inserted into the packet list.
    InformOfEmptyPacket(packet.seqNum);
    return 0;
  }

  if (packets_.size() == kMaxPacketsInSession) {
    LOG(LS_ERROR) << "Max number of packets per frame has been reached.";
    return -1;
  }

  // Find the position of this packet in the packet list in sequence number
  // order and insert it. Loop over the list in reverse order.
  ReversePacketIterator rit = packets_.rbegin();
  for (; rit != packets_.rend(); ++rit)
    if (LatestSequenceNumber(packet.seqNum, (*rit).seqNum) == packet.seqNum)
      break;

  // Check for duplicate packets.
  if (rit != packets_.rend() &&
      (*rit).seqNum == packet.seqNum && (*rit).sizeBytes > 0)
    return -2;

  if (packet.codec == kVideoCodecH264) {
    frame_type_ = packet.frameType;
    if (packet.isFirstPacket &&
        (first_packet_seq_num_ == -1 ||
         IsNewerSequenceNumber(first_packet_seq_num_, packet.seqNum))) {
      first_packet_seq_num_ = packet.seqNum;
    }
    if (packet.markerBit &&
        (last_packet_seq_num_ == -1 ||
         IsNewerSequenceNumber(packet.seqNum, last_packet_seq_num_))) {
      last_packet_seq_num_ = packet.seqNum;
    }
  } else {
    // Only insert media packets between first and last packets (when
    // available).
    // Placing check here, as to properly account for duplicate packets.
    // Check if this is first packet (only valid for some codecs)
    // Should only be set for one packet per session.
    if (packet.isFirstPacket && first_packet_seq_num_ == -1) {
      // The first packet in a frame signals the frame type.
      frame_type_ = packet.frameType;
      // Store the sequence number for the first packet.
      first_packet_seq_num_ = static_cast<int>(packet.seqNum);
    } else if (first_packet_seq_num_ != -1 &&
               !IsNewerSequenceNumber(packet.seqNum, first_packet_seq_num_)) {
      LOG(LS_WARNING) << "Received packet with a sequence number which is out "
                         "of frame boundaries";
      return -3;
    } else if (frame_type_ == kFrameEmpty && packet.frameType != kFrameEmpty) {
      // Update the frame type with the type of the first media packet.
      // TODO(mikhal): Can this trigger?
      frame_type_ = packet.frameType;
    }

    // Track the marker bit, should only be set for one packet per session.
    if (packet.markerBit && last_packet_seq_num_ == -1) {
      last_packet_seq_num_ = static_cast<int>(packet.seqNum);
    } else if (last_packet_seq_num_ != -1 &&
               IsNewerSequenceNumber(packet.seqNum, last_packet_seq_num_)) {
      LOG(LS_WARNING) << "Received packet with a sequence number which is out "
                         "of frame boundaries";
      return -3;
    }
  }

  // The insert operation invalidates the iterator |rit|.
  PacketIterator packet_list_it = packets_.insert(rit.base(), packet);

  int returnLength = InsertBuffer(frame_buffer, packet_list_it);
  UpdateCompleteSession();
  if (decode_error_mode == kWithErrors)
    decodable_ = true;
  else if (decode_error_mode == kSelectiveErrors)
    UpdateDecodableSession(frame_data);
  return returnLength;
}

void VCMSessionInfo::InformOfEmptyPacket(uint16_t seq_num) {
  // Empty packets may be FEC or filler packets. They are sequential and
  // follow the data packets, therefore, we should only keep track of the high
  // and low sequence numbers and may assume that the packets in between are
  // empty packets belonging to the same frame (timestamp).
  if (empty_seq_num_high_ == -1)
    empty_seq_num_high_ = seq_num;
  else
    empty_seq_num_high_ = LatestSequenceNumber(seq_num, empty_seq_num_high_);
  if (empty_seq_num_low_ == -1 || IsNewerSequenceNumber(empty_seq_num_low_,
                                                        seq_num))
    empty_seq_num_low_ = seq_num;
}

}  // namespace webrtc
