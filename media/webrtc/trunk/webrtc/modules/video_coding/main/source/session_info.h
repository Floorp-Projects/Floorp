/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_SESSION_INFO_H_
#define WEBRTC_MODULES_VIDEO_CODING_SESSION_INFO_H_

#include <list>

#include "modules/interface/module_common_types.h"
#include "modules/video_coding/main/source/packet.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class VCMSessionInfo {
 public:
  VCMSessionInfo();

  void UpdateDataPointers(const uint8_t* old_base_ptr,
                          const uint8_t* new_base_ptr);
  // NACK - Building the NACK lists.
  // Build hard NACK list: Zero out all entries in list up to and including
  // _lowSeqNum.
  int BuildHardNackList(int* seq_num_list,
                        int seq_num_list_length,
                        int nack_seq_nums_index);

  // Build soft NACK list:  Zero out only a subset of the packets, discard
  // empty packets.
  int BuildSoftNackList(int* seq_num_list,
                        int seq_num_list_length,
                        int nack_seq_nums_index,
                        int rtt_ms);
  void Reset();
  int InsertPacket(const VCMPacket& packet,
                   uint8_t* frame_buffer,
                   bool enable_decodable_state,
                   int rtt_ms);
  bool complete() const;
  bool decodable() const;

  // Builds fragmentation headers for VP8, each fragment being a decodable
  // VP8 partition. Returns the total number of bytes which are decodable. Is
  // used instead of MakeDecodable for VP8.
  int BuildVP8FragmentationHeader(uint8_t* frame_buffer,
                                  int frame_buffer_length,
                                  RTPFragmentationHeader* fragmentation);

  // Makes the frame decodable. I.e., only contain decodable NALUs. All
  // non-decodable NALUs will be deleted and packets will be moved to in
  // memory to remove any empty space.
  // Returns the number of bytes deleted from the session.
  int MakeDecodable();
  int SessionLength() const;
  bool HaveFirstPacket() const;
  bool HaveLastPacket() const;
  bool session_nack() const;
  webrtc::FrameType FrameType() const { return frame_type_; }
  int LowSequenceNumber() const;

  // Returns highest sequence number, media or empty.
  int HighSequenceNumber() const;
  int PictureId() const;
  int TemporalId() const;
  bool LayerSync() const;
  int Tl0PicId() const;
  bool NonReference() const;
  void SetPreviousFrameLoss() { previous_frame_loss_ = true; }
  bool PreviousFrameLoss() const { return previous_frame_loss_; }

  // The number of packets discarded because the decoder can't make use of
  // them.
  int packets_not_decodable() const;

 private:
  enum { kMaxVP8Partitions = 9 };

  typedef std::list<VCMPacket> PacketList;
  typedef PacketList::iterator PacketIterator;
  typedef PacketList::const_iterator PacketIteratorConst;
  typedef PacketList::reverse_iterator ReversePacketIterator;

  void InformOfEmptyPacket(uint16_t seq_num);

  // Finds the packet of the beginning of the next VP8 partition. If
  // none is found the returned iterator points to |packets_.end()|.
  // |it| is expected to point to the last packet of the previous partition,
  // or to the first packet of the frame. |packets_skipped| is incremented
  // for each packet found which doesn't have the beginning bit set.
  PacketIterator FindNextPartitionBeginning(PacketIterator it,
                                            int* packets_skipped) const;

  // Returns an iterator pointing to the last packet of the partition pointed to
  // by |it|.
  PacketIterator FindPartitionEnd(PacketIterator it) const;
  static bool InSequence(const PacketIterator& it,
                         const PacketIterator& prev_it);
  int InsertBuffer(uint8_t* frame_buffer,
                   PacketIterator packetIterator);
  void ShiftSubsequentPackets(PacketIterator it, int steps_to_shift);
  PacketIterator FindNaluEnd(PacketIterator packet_iter) const;
  // Deletes the data of all packets between |start| and |end|, inclusively.
  // Note that this function doesn't delete the actual packets.
  int DeletePacketData(PacketIterator start,
                       PacketIterator end);
  void UpdateCompleteSession();

  // When enabled, determine if session is decodable, i.e. incomplete but
  // would be sent to the decoder.
  void UpdateDecodableSession(int rtt_ms);

  // If this session has been NACKed by the jitter buffer.
  bool session_nack_;
  bool complete_;
  bool decodable_;
  webrtc::FrameType frame_type_;
  bool previous_frame_loss_;
  // Packets in this frame.
  PacketList packets_;
  int empty_seq_num_low_;
  int empty_seq_num_high_;
  // Number of packets discarded because the decoder can't use them.
  int packets_not_decodable_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_SESSION_INFO_H_
