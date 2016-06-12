/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_

#include <queue>
#include <string>

#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"

namespace webrtc {

class RtpPacketizerH264 : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded H264 frame.
  RtpPacketizerH264(FrameType frame_type,
                    size_t max_payload_len,
                    uint8_t packetization_mode);

  virtual ~RtpPacketizerH264();

  void SetPayloadData(const uint8_t* payload_data,
                      size_t payload_size,
                      const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with H264 payload header.
  // buffer is a pointer to where the output will be written.
  // bytes_to_send is an output variable that will contain number of bytes
  // written to buffer. The parameter last_packet is true for the last packet of
  // the frame, false otherwise (i.e., call the function again to get the
  // next packet).
  // Returns true on success or false if there was no payload to packetize.
  bool NextPacket(uint8_t* buffer,
                  size_t* bytes_to_send,
                  bool* last_packet) override;

  ProtectionType GetProtectionType() override;

  StorageType GetStorageType(uint32_t retransmission_settings) override;

  std::string ToString() override;

 private:
  struct Packet {
    Packet(size_t offset,
           size_t size,
           bool first_fragment,
           bool last_fragment,
           bool aggregated,
           uint8_t header)
        : offset(offset),
          size(size),
          first_fragment(first_fragment),
          last_fragment(last_fragment),
          aggregated(aggregated),
          header(header) {}

    size_t offset;
    size_t size;
    bool first_fragment;
    bool last_fragment;
    bool aggregated;
    uint8_t header;
  };
  typedef std::queue<Packet> PacketQueue;

  void GeneratePackets();
  void PacketizeFuA(size_t fragment_offset, size_t fragment_length);
  int PacketizeStapA(size_t fragment_index,
                     size_t fragment_offset,
                     size_t fragment_length);
  void NextAggregatePacket(uint8_t* buffer, size_t* bytes_to_send);
  void NextFragmentPacket(uint8_t* buffer, size_t* bytes_to_send);

  const uint8_t* payload_data_;
  size_t payload_size_;
  const size_t max_payload_len_;
  RTPFragmentationHeader fragmentation_;
  PacketQueue packets_;
  FrameType frame_type_;
  uint8_t packetization_mode_;

  DISALLOW_COPY_AND_ASSIGN(RtpPacketizerH264);
};

// Depacketizer for H264.
class RtpDepacketizerH264 : public RtpDepacketizer {
 public:
  virtual ~RtpDepacketizerH264() {}

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
