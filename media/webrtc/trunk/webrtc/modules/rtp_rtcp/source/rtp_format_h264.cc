/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {
namespace {

enum Nalu {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kStapA = 24,
  kFuA = 28
};

static const size_t kNalHeaderSize = 1;
static const size_t kFuAHeaderSize = 2;
static const size_t kLengthFieldSize = 2;

// Bit masks for FU (A and B) indicators.
enum NalDefs { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

void ParseSingleNalu(RtpDepacketizer::ParsedPayload* parsed_payload,
                     const uint8_t* payload_data,
                     size_t payload_data_length) {
  parsed_payload->type.Video.width = 0;
  parsed_payload->type.Video.height = 0;
  parsed_payload->type.Video.codec = kRtpVideoH264;
  parsed_payload->type.Video.isFirstPacket = true;
  RTPVideoHeaderH264* h264_header =
      &parsed_payload->type.Video.codecHeader.H264;
  h264_header->single_nalu = true;
  h264_header->stap_a = false;

  uint8_t nal_type = payload_data[0] & kTypeMask;
  if (nal_type == kStapA) {
    nal_type = payload_data[3] & kTypeMask;
    h264_header->stap_a = true;
  }

  switch (nal_type) {
    case kSps:
    case kPps:
    case kIdr:
      parsed_payload->frame_type = kVideoFrameKey;
      break;
    default:
      parsed_payload->frame_type = kVideoFrameDelta;
      break;
  }
}

void ParseFuaNalu(RtpDepacketizer::ParsedPayload* parsed_payload,
                  const uint8_t* payload_data,
                  size_t payload_data_length,
                  size_t* offset) {
  uint8_t fnri = payload_data[0] & (kFBit | kNriMask);
  uint8_t original_nal_type = payload_data[1] & kTypeMask;
  bool first_fragment = (payload_data[1] & kSBit) > 0;

  uint8_t original_nal_header = fnri | original_nal_type;
  if (first_fragment) {
    *offset = kNalHeaderSize;
    uint8_t* payload = const_cast<uint8_t*>(payload_data + *offset);
    payload[0] = original_nal_header;
  } else {
    *offset = kFuAHeaderSize;
  }

  if (original_nal_type == kIdr) {
    parsed_payload->frame_type = kVideoFrameKey;
  } else {
    parsed_payload->frame_type = kVideoFrameDelta;
  }
  parsed_payload->type.Video.width = 0;
  parsed_payload->type.Video.height = 0;
  parsed_payload->type.Video.codec = kRtpVideoH264;
  parsed_payload->type.Video.isFirstPacket = first_fragment;
  RTPVideoHeaderH264* h264_header =
      &parsed_payload->type.Video.codecHeader.H264;
  h264_header->single_nalu = false;
  h264_header->stap_a = false;
}
}  // namespace

RtpPacketizerH264::RtpPacketizerH264(FrameType frame_type,
                                     size_t max_payload_len)
    : payload_data_(NULL),
      payload_size_(0),
      max_payload_len_(max_payload_len),
      frame_type_(frame_type) {
}

RtpPacketizerH264::~RtpPacketizerH264() {
}

void RtpPacketizerH264::SetPayloadData(
    const uint8_t* payload_data,
    size_t payload_size,
    const RTPFragmentationHeader* fragmentation) {
  assert(packets_.empty());
  assert(fragmentation);
  payload_data_ = payload_data;
  payload_size_ = payload_size;
  fragmentation_.CopyFrom(*fragmentation);
  GeneratePackets();
}

void RtpPacketizerH264::GeneratePackets() {
  for (size_t i = 0; i < fragmentation_.fragmentationVectorSize;) {
    size_t fragment_offset = fragmentation_.fragmentationOffset[i];
    size_t fragment_length = fragmentation_.fragmentationLength[i];
    if (fragment_length > max_payload_len_) {
      PacketizeFuA(fragment_offset, fragment_length);
      ++i;
    } else {
      i = PacketizeStapA(i, fragment_offset, fragment_length);
    }
  }
}

void RtpPacketizerH264::PacketizeFuA(size_t fragment_offset,
                                     size_t fragment_length) {
  // Fragment payload into packets (FU-A).
  // Strip out the original header and leave room for the FU-A header.
  fragment_length -= kNalHeaderSize;
  size_t offset = fragment_offset + kNalHeaderSize;
  size_t bytes_available = max_payload_len_ - kFuAHeaderSize;
  size_t fragments =
      (fragment_length + (bytes_available - 1)) / bytes_available;
  size_t avg_size = (fragment_length + fragments - 1) / fragments;
  while (fragment_length > 0) {
    size_t packet_length = avg_size;
    if (fragment_length < avg_size)
      packet_length = fragment_length;
    uint8_t header = payload_data_[fragment_offset];
    packets_.push(Packet(offset,
                         packet_length,
                         offset - kNalHeaderSize == fragment_offset,
                         fragment_length == packet_length,
                         false,
                         header));
    offset += packet_length;
    fragment_length -= packet_length;
  }
}

int RtpPacketizerH264::PacketizeStapA(size_t fragment_index,
                                      size_t fragment_offset,
                                      size_t fragment_length) {
  // Aggregate fragments into one packet (STAP-A).
  size_t payload_size_left = max_payload_len_;
  int aggregated_fragments = 0;
  size_t fragment_headers_length = 0;
  assert(payload_size_left >= fragment_length);
  while (payload_size_left >= fragment_length + fragment_headers_length) {
    assert(fragment_length > 0);
    uint8_t header = payload_data_[fragment_offset];
    packets_.push(Packet(fragment_offset,
                         fragment_length,
                         aggregated_fragments == 0,
                         false,
                         true,
                         header));
    payload_size_left -= fragment_length;
    payload_size_left -= fragment_headers_length;

    // Next fragment.
    ++fragment_index;
    if (fragment_index == fragmentation_.fragmentationVectorSize)
      break;
    fragment_offset = fragmentation_.fragmentationOffset[fragment_index];
    fragment_length = fragmentation_.fragmentationLength[fragment_index];

    fragment_headers_length = kLengthFieldSize;
    // If we are going to try to aggregate more fragments into this packet
    // we need to add the STAP-A NALU header and a length field for the first
    // NALU of this packet.
    if (aggregated_fragments == 0)
      fragment_headers_length += kNalHeaderSize + kLengthFieldSize;
    ++aggregated_fragments;
  }
  packets_.back().last_fragment = true;
  return fragment_index;
}

bool RtpPacketizerH264::NextPacket(uint8_t* buffer,
                                   size_t* bytes_to_send,
                                   bool* last_packet) {
  *bytes_to_send = 0;
  if (packets_.empty()) {
    *bytes_to_send = 0;
    *last_packet = true;
    return false;
  }

  Packet packet = packets_.front();

  if (packet.first_fragment && packet.last_fragment) {
    // Single NAL unit packet.
    *bytes_to_send = packet.size;
    memcpy(buffer, &payload_data_[packet.offset], packet.size);
    packets_.pop();
    assert(*bytes_to_send <= max_payload_len_);
  } else if (packet.aggregated) {
    NextAggregatePacket(buffer, bytes_to_send);
    assert(*bytes_to_send <= max_payload_len_);
  } else {
    NextFragmentPacket(buffer, bytes_to_send);
    assert(*bytes_to_send <= max_payload_len_);
  }
  *last_packet = packets_.empty();
  return true;
}

void RtpPacketizerH264::NextAggregatePacket(uint8_t* buffer,
                                            size_t* bytes_to_send) {
  Packet packet = packets_.front();
  assert(packet.first_fragment);
  // STAP-A NALU header.
  buffer[0] = (packet.header & (kFBit | kNriMask)) | kStapA;
  int index = kNalHeaderSize;
  *bytes_to_send += kNalHeaderSize;
  while (packet.aggregated) {
    // Add NAL unit length field.
    RtpUtility::AssignUWord16ToBuffer(&buffer[index], packet.size);
    index += kLengthFieldSize;
    *bytes_to_send += kLengthFieldSize;
    // Add NAL unit.
    memcpy(&buffer[index], &payload_data_[packet.offset], packet.size);
    index += packet.size;
    *bytes_to_send += packet.size;
    packets_.pop();
    if (packet.last_fragment)
      break;
    packet = packets_.front();
  }
  assert(packet.last_fragment);
}

void RtpPacketizerH264::NextFragmentPacket(uint8_t* buffer,
                                           size_t* bytes_to_send) {
  Packet packet = packets_.front();
  // NAL unit fragmented over multiple packets (FU-A).
  // We do not send original NALU header, so it will be replaced by the
  // FU indicator header of the first packet.
  uint8_t fu_indicator = (packet.header & (kFBit | kNriMask)) | kFuA;
  uint8_t fu_header = 0;

  // S | E | R | 5 bit type.
  fu_header |= (packet.first_fragment ? kSBit : 0);
  fu_header |= (packet.last_fragment ? kEBit : 0);
  uint8_t type = packet.header & kTypeMask;
  fu_header |= type;
  buffer[0] = fu_indicator;
  buffer[1] = fu_header;

  if (packet.last_fragment) {
    *bytes_to_send = packet.size + kFuAHeaderSize;
    memcpy(buffer + kFuAHeaderSize, &payload_data_[packet.offset], packet.size);
  } else {
    *bytes_to_send = packet.size + kFuAHeaderSize;
    memcpy(buffer + kFuAHeaderSize, &payload_data_[packet.offset], packet.size);
  }
  packets_.pop();
}

ProtectionType RtpPacketizerH264::GetProtectionType() {
  return (frame_type_ == kVideoFrameKey) ? kProtectedPacket
                                         : kUnprotectedPacket;
}

StorageType RtpPacketizerH264::GetStorageType(
    uint32_t retransmission_settings) {
  return kAllowRetransmission;
}

std::string RtpPacketizerH264::ToString() {
  return "RtpPacketizerH264";
}

bool RtpDepacketizerH264::Parse(ParsedPayload* parsed_payload,
                                const uint8_t* payload_data,
                                size_t payload_data_length) {
  assert(parsed_payload != NULL);
  uint8_t nal_type = payload_data[0] & kTypeMask;
  size_t offset = 0;
  if (nal_type == kFuA) {
    // Fragmented NAL units (FU-A).
    ParseFuaNalu(parsed_payload, payload_data, payload_data_length, &offset);
  } else {
    // We handle STAP-A and single NALU's the same way here. The jitter buffer
    // will depacketize the STAP-A into NAL units later.
    ParseSingleNalu(parsed_payload, payload_data, payload_data_length);
  }

  parsed_payload->payload = payload_data + offset;
  parsed_payload->payload_length = payload_data_length - offset;
  return true;
}
}  // namespace webrtc
