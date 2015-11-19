/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"

#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

enum { kREDForFECHeaderLength = 1 };
// This controls the maximum amount of excess overhead (actual - target)
// allowed in order to trigger GenerateFEC(), before |params_.max_fec_frames|
// is reached. Overhead here is defined as relative to number of media packets.
enum { kMaxExcessOverhead = 50 };  // Q8.
// This is the minimum number of media packets required (above some protection
// level) in order to trigger GenerateFEC(), before |params_.max_fec_frames| is
// reached.
enum { kMinimumMediaPackets = 4 };
// Threshold on the received FEC protection level, above which we enforce at
// least |kMinimumMediaPackets| packets for the FEC code. Below this
// threshold |kMinimumMediaPackets| is set to default value of 1.
enum { kHighProtectionThreshold = 80 };  // Corresponds to ~30 overhead, range
// is 0 to 255, where 255 corresponds to 100% overhead (relative to number of
// media packets).

struct RtpPacket {
  uint16_t rtpHeaderLength;
  ForwardErrorCorrection::Packet* pkt;
};

RedPacket::RedPacket(size_t length)
    : data_(new uint8_t[length]),
      length_(length),
      header_length_(0) {
}

RedPacket::~RedPacket() {
  delete [] data_;
}

void RedPacket::CreateHeader(const uint8_t* rtp_header, size_t header_length,
                             int red_pl_type, int pl_type) {
  assert(header_length + kREDForFECHeaderLength <= length_);
  memcpy(data_, rtp_header, header_length);
  // Replace payload type.
  data_[1] &= 0x80;
  data_[1] += red_pl_type;
  // Add RED header
  // f-bit always 0
  data_[header_length] = static_cast<uint8_t>(pl_type);
  header_length_ = header_length + kREDForFECHeaderLength;
}

void RedPacket::SetSeqNum(int seq_num) {
  assert(seq_num >= 0 && seq_num < (1<<16));

  ByteWriter<uint16_t>::WriteBigEndian(&data_[2], seq_num);
}

void RedPacket::AssignPayload(const uint8_t* payload, size_t length) {
  assert(header_length_ + length <= length_);
  memcpy(data_ + header_length_, payload, length);
}

void RedPacket::ClearMarkerBit() {
  data_[1] &= 0x7F;
}

uint8_t* RedPacket::data() const {
  return data_;
}

size_t RedPacket::length() const {
  return length_;
}

ProducerFec::ProducerFec(ForwardErrorCorrection* fec)
    : fec_(fec),
      media_packets_fec_(),
      fec_packets_(),
      num_frames_(0),
      incomplete_frame_(false),
      num_first_partition_(0),
      minimum_media_packets_fec_(1),
      params_(),
      new_params_() {
  memset(&params_, 0, sizeof(params_));
  memset(&new_params_, 0, sizeof(new_params_));
}

ProducerFec::~ProducerFec() {
  DeletePackets();
}

void ProducerFec::SetFecParameters(const FecProtectionParams* params,
                                   int num_first_partition) {
  // Number of first partition packets cannot exceed kMaxMediaPackets
  assert(params->fec_rate >= 0 && params->fec_rate < 256);
  if (num_first_partition >
      static_cast<int>(ForwardErrorCorrection::kMaxMediaPackets)) {
      num_first_partition =
          ForwardErrorCorrection::kMaxMediaPackets;
  }
  // Store the new params and apply them for the next set of FEC packets being
  // produced.
  new_params_ = *params;
  num_first_partition_ = num_first_partition;
  if (params->fec_rate > kHighProtectionThreshold) {
    minimum_media_packets_fec_ = kMinimumMediaPackets;
  } else {
    minimum_media_packets_fec_ = 1;
  }
}

RedPacket* ProducerFec::BuildRedPacket(const uint8_t* data_buffer,
                                       size_t payload_length,
                                       size_t rtp_header_length,
                                       int red_pl_type) {
  RedPacket* red_packet = new RedPacket(payload_length +
                                        kREDForFECHeaderLength +
                                        rtp_header_length);
  int pl_type = data_buffer[1] & 0x7f;
  red_packet->CreateHeader(data_buffer, rtp_header_length,
                           red_pl_type, pl_type);
  red_packet->AssignPayload(data_buffer + rtp_header_length, payload_length);
  return red_packet;
}

int ProducerFec::AddRtpPacketAndGenerateFec(const uint8_t* data_buffer,
                                            size_t payload_length,
                                            size_t rtp_header_length) {
  assert(fec_packets_.empty());
  if (media_packets_fec_.empty()) {
    params_ = new_params_;
  }
  incomplete_frame_ = true;
  const bool marker_bit = (data_buffer[1] & kRtpMarkerBitMask) ? true : false;
  if (media_packets_fec_.size() < ForwardErrorCorrection::kMaxMediaPackets) {
    // Generic FEC can only protect up to kMaxMediaPackets packets.
    ForwardErrorCorrection::Packet* packet = new ForwardErrorCorrection::Packet;
    packet->length = payload_length + rtp_header_length;
    memcpy(packet->data, data_buffer, packet->length);
    media_packets_fec_.push_back(packet);
  }
  if (marker_bit) {
    ++num_frames_;
    incomplete_frame_ = false;
  }
  // Produce FEC over at most |params_.max_fec_frames| frames, or as soon as:
  // (1) the excess overhead (actual overhead - requested/target overhead) is
  // less than |kMaxExcessOverhead|, and
  // (2) at least |minimum_media_packets_fec_| media packets is reached.
  if (!incomplete_frame_ &&
      (num_frames_ == params_.max_fec_frames ||
          (ExcessOverheadBelowMax() && MinimumMediaPacketsReached()))) {
    assert(num_first_partition_ <=
           static_cast<int>(ForwardErrorCorrection::kMaxMediaPackets));
    int ret = fec_->GenerateFEC(media_packets_fec_,
                                params_.fec_rate,
                                num_first_partition_,
                                params_.use_uep_protection,
                                params_.fec_mask_type,
                                &fec_packets_);
    if (fec_packets_.empty()) {
      num_frames_ = 0;
      DeletePackets();
    }
    return ret;
  }
  return 0;
}

// Returns true if the excess overhead (actual - target) for the FEC is below
// the amount |kMaxExcessOverhead|. This effects the lower protection level
// cases and low number of media packets/frame. The target overhead is given by
// |params_.fec_rate|, and is only achievable in the limit of large number of
// media packets.
bool ProducerFec::ExcessOverheadBelowMax() {
  return ((Overhead() - params_.fec_rate) < kMaxExcessOverhead);
}

// Returns true if the media packet list for the FEC is at least
// |minimum_media_packets_fec_|. This condition tries to capture the effect
// that, for the same amount of protection/overhead, longer codes
// (e.g. (2k,2m) vs (k,m)) are generally more effective at recovering losses.
bool ProducerFec::MinimumMediaPacketsReached() {
  float avg_num_packets_frame = static_cast<float>(media_packets_fec_.size()) /
                                num_frames_;
  if (avg_num_packets_frame < 2.0f) {
  return (static_cast<int>(media_packets_fec_.size()) >=
      minimum_media_packets_fec_);
  } else {
    // For larger rates (more packets/frame), increase the threshold.
    return (static_cast<int>(media_packets_fec_.size()) >=
        minimum_media_packets_fec_ + 1);
  }
}

bool ProducerFec::FecAvailable() const {
  return (fec_packets_.size() > 0);
}

RedPacket* ProducerFec::GetFecPacket(int red_pl_type,
                                     int fec_pl_type,
                                     uint16_t seq_num,
                                     size_t rtp_header_length) {
  if (fec_packets_.empty())
    return NULL;
  // Build FEC packet. The FEC packets in |fec_packets_| doesn't
  // have RTP headers, so we're reusing the header from the last
  // media packet.
  ForwardErrorCorrection::Packet* packet_to_send = fec_packets_.front();
  ForwardErrorCorrection::Packet* last_media_packet = media_packets_fec_.back();
  RedPacket* return_packet = new RedPacket(packet_to_send->length +
                                           kREDForFECHeaderLength +
                                           rtp_header_length);
  return_packet->CreateHeader(last_media_packet->data,
                              rtp_header_length,
                              red_pl_type,
                              fec_pl_type);
  return_packet->SetSeqNum(seq_num);
  return_packet->ClearMarkerBit();
  return_packet->AssignPayload(packet_to_send->data, packet_to_send->length);
  fec_packets_.pop_front();
  if (fec_packets_.empty()) {
    // Done with all the FEC packets. Reset for next run.
    DeletePackets();
    num_frames_ = 0;
  }
  return return_packet;
}

int ProducerFec::Overhead() const {
  // Overhead is defined as relative to the number of media packets, and not
  // relative to total number of packets. This definition is inhereted from the
  // protection factor produced by video_coding module and how the FEC
  // generation is implemented.
  assert(!media_packets_fec_.empty());
  int num_fec_packets = fec_->GetNumberOfFecPackets(media_packets_fec_.size(),
                                                    params_.fec_rate);
  // Return the overhead in Q8.
  return (num_fec_packets << 8) / media_packets_fec_.size();
}

void ProducerFec::DeletePackets() {
  while (!media_packets_fec_.empty()) {
    delete media_packets_fec_.front();
    media_packets_fec_.pop_front();
  }
  assert(media_packets_fec_.empty());
}

}  // namespace webrtc
