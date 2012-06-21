/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/producer_fec.h"

#include "modules/rtp_rtcp/source/forward_error_correction.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"

namespace webrtc {

// Minimum RTP header size in bytes.
enum { kRtpHeaderSize = 12 };
enum { kREDForFECHeaderLength = 1 };
enum { kMaxOverhead = 60 };  // Q8.

struct RtpPacket {
  WebRtc_UWord16 rtpHeaderLength;
  ForwardErrorCorrection::Packet* pkt;
};

RedPacket::RedPacket(int length)
    : data_(new uint8_t[length]),
      length_(length),
      header_length_(0) {
}

RedPacket::~RedPacket() {
  delete [] data_;
}

void RedPacket::CreateHeader(const uint8_t* rtp_header, int header_length,
                             int red_pl_type, int pl_type) {
  assert(header_length + kREDForFECHeaderLength <= length_);
  memcpy(data_, rtp_header, header_length);
  // Replace payload type.
  data_[1] &= 0x80;
  data_[1] += red_pl_type;
  // Add RED header
  // f-bit always 0
  data_[header_length] = pl_type;
  header_length_ = header_length + kREDForFECHeaderLength;
}

void RedPacket::SetSeqNum(int seq_num) {
  assert(seq_num >= 0 && seq_num < (1<<16));
  ModuleRTPUtility::AssignUWord16ToBuffer(&data_[2], seq_num);
}

void RedPacket::AssignPayload(const uint8_t* payload, int length) {
  assert(header_length_ + length <= length_);
  memcpy(data_ + header_length_, payload, length);
}

void RedPacket::ClearMarkerBit() {
  data_[1] &= 0x7F;
}

uint8_t* RedPacket::data() const {
  return data_;
}

int RedPacket::length() const {
  return length_;
}

ProducerFec::ProducerFec(ForwardErrorCorrection* fec)
    : fec_(fec),
      media_packets_fec_(),
      fec_packets_(),
      num_frames_(0),
      incomplete_frame_(false),
      num_first_partition_(0),
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
}

RedPacket* ProducerFec::BuildRedPacket(const uint8_t* data_buffer,
                                       int payload_length,
                                       int rtp_header_length,
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
                                            int payload_length,
                                            int rtp_header_length) {
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
  // Produce FEC over at most |params_.max_fec_frames| frames, or as soon as
  // the wasted overhead (actual overhead - requested protection) is less than
  // |kMaxOverhead|.
  if (!incomplete_frame_ &&
      (num_frames_ == params_.max_fec_frames ||
          (Overhead() - params_.fec_rate) < kMaxOverhead)) {
    assert(num_first_partition_ <=
           static_cast<int>(ForwardErrorCorrection::kMaxMediaPackets));
    int ret = fec_->GenerateFEC(media_packets_fec_,
                                params_.fec_rate,
                                num_first_partition_,
                                params_.use_uep_protection,
                                &fec_packets_);
    if (fec_packets_.empty()) {
      num_frames_ = 0;
      DeletePackets();
    }
    return ret;
  }
  return 0;
}

bool ProducerFec::FecAvailable() const {
  return (fec_packets_.size() > 0);
}

RedPacket* ProducerFec::GetFecPacket(int red_pl_type, int fec_pl_type,
                                     uint16_t seq_num) {
  if (fec_packets_.empty())
    return NULL;
  // Build FEC packet. The FEC packets in |fec_packets_| doesn't
  // have RTP headers, so we're reusing the header from the last
  // media packet.
  ForwardErrorCorrection::Packet* packet_to_send = fec_packets_.front();
  ForwardErrorCorrection::Packet* last_media_packet = media_packets_fec_.back();
  RedPacket* return_packet = new RedPacket(packet_to_send->length +
                                           kREDForFECHeaderLength +
                                           kRtpHeaderSize);
  return_packet->CreateHeader(last_media_packet->data,
                              kRtpHeaderSize,
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
  int num_fec_packets = params_.fec_rate * media_packets_fec_.size();
  // Ceil.
  int rounding = (num_fec_packets % (1 << 8) > 0) ? (1 << 8) : 0;
  num_fec_packets = (num_fec_packets + rounding) >> 8;
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
