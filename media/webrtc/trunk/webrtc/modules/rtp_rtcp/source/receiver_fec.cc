/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/receiver_fec.h"

#include <cassert>

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/trace.h"

// RFC 5109
namespace webrtc {
ReceiverFEC::ReceiverFEC(const int32_t id, RTPReceiverVideo* owner)
    : id_(id),
      owner_(owner),
      fec_(new ForwardErrorCorrection(id)),
      payload_type_fec_(-1) {}

ReceiverFEC::~ReceiverFEC() {
  // Clean up DecodeFEC()
  while (!received_packet_list_.empty()) {
    ForwardErrorCorrection::ReceivedPacket* received_packet =
        received_packet_list_.front();
    delete received_packet;
    received_packet_list_.pop_front();
  }
  assert(received_packet_list_.empty());

  if (fec_ != NULL) {
    fec_->ResetState(&recovered_packet_list_);
    delete fec_;
  }
}

void ReceiverFEC::SetPayloadTypeFEC(const int8_t payload_type) {
  payload_type_fec_ = payload_type;
}
//     0                   1                    2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |F|   block PT  |  timestamp offset         |   block length    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//
// RFC 2198          RTP Payload for Redundant Audio Data    September 1997
//
//    The bits in the header are specified as follows:
//
//    F: 1 bit First bit in header indicates whether another header block
//        follows.  If 1 further header blocks follow, if 0 this is the
//        last header block.
//        If 0 there is only 1 byte RED header
//
//    block PT: 7 bits RTP payload type for this block.
//
//    timestamp offset:  14 bits Unsigned offset of timestamp of this block
//        relative to timestamp given in RTP header.  The use of an unsigned
//        offset implies that redundant data must be sent after the primary
//        data, and is hence a time to be subtracted from the current
//        timestamp to determine the timestamp of the data for which this
//        block is the redundancy.
//
//    block length:  10 bits Length in bytes of the corresponding data
//        block excluding header.

int32_t ReceiverFEC::AddReceivedFECPacket(const WebRtcRTPHeader* rtp_header,
                                          const uint8_t* incoming_rtp_packet,
                                          const uint16_t payload_data_length,
                                          bool& FECpacket) {
  if (payload_type_fec_ == -1) {
    return -1;
  }

  uint8_t REDHeaderLength = 1;

  // Add to list without RED header, aka a virtual RTP packet
  // we remove the RED header

  ForwardErrorCorrection::ReceivedPacket* received_packet =
      new ForwardErrorCorrection::ReceivedPacket;
  received_packet->pkt = new ForwardErrorCorrection::Packet;

  // get payload type from RED header
  uint8_t payload_type =
      incoming_rtp_packet[rtp_header->header.headerLength] & 0x7f;

  // use the payload_type to decide if it's FEC or coded data
  if (payload_type_fec_ == payload_type) {
    received_packet->is_fec = true;
    FECpacket = true;
  } else {
    received_packet->is_fec = false;
    FECpacket = false;
  }
  received_packet->seq_num = rtp_header->header.sequenceNumber;

  uint16_t blockLength = 0;
  if (incoming_rtp_packet[rtp_header->header.headerLength] & 0x80) {
    // f bit set in RED header
    REDHeaderLength = 4;
    uint16_t timestamp_offset =
        (incoming_rtp_packet[rtp_header->header.headerLength + 1]) << 8;
    timestamp_offset +=
        incoming_rtp_packet[rtp_header->header.headerLength + 2];
    timestamp_offset = timestamp_offset >> 2;
    if (timestamp_offset != 0) {
      // |timestampOffset| should be 0. However, it's possible this is the first
      // location a corrupt payload can be caught, so don't assert.
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                   "Corrupt payload found in %s", __FUNCTION__);
      delete received_packet;
      return -1;
    }

    blockLength =
        (0x03 & incoming_rtp_packet[rtp_header->header.headerLength + 2]) << 8;
    blockLength += (incoming_rtp_packet[rtp_header->header.headerLength + 3]);

    // check next RED header
    if (incoming_rtp_packet[rtp_header->header.headerLength + 4] & 0x80) {
      // more than 2 blocks in packet not supported
      delete received_packet;
      assert(false);
      return -1;
    }
    if (blockLength > payload_data_length - REDHeaderLength) {
      // block length longer than packet
      delete received_packet;
      assert(false);
      return -1;
    }
  }

  ForwardErrorCorrection::ReceivedPacket* second_received_packet = NULL;
  if (blockLength > 0) {
    // handle block length, split into 2 packets
    REDHeaderLength = 5;

    // copy the RTP header
    memcpy(received_packet->pkt->data, incoming_rtp_packet,
           rtp_header->header.headerLength);

    // replace the RED payload type
    received_packet->pkt->data[1] &= 0x80;  // reset the payload
    received_packet->pkt->data[1] +=
        payload_type;                       // set the media payload type

    // copy the payload data
    memcpy(
        received_packet->pkt->data + rtp_header->header.headerLength,
        incoming_rtp_packet + rtp_header->header.headerLength + REDHeaderLength,
        blockLength);

    received_packet->pkt->length = blockLength;

    second_received_packet = new ForwardErrorCorrection::ReceivedPacket;
    second_received_packet->pkt = new ForwardErrorCorrection::Packet;

    second_received_packet->is_fec = true;
    second_received_packet->seq_num = rtp_header->header.sequenceNumber;

    // copy the FEC payload data
    memcpy(second_received_packet->pkt->data,
           incoming_rtp_packet + rtp_header->header.headerLength +
               REDHeaderLength + blockLength,
           payload_data_length - REDHeaderLength - blockLength);

    second_received_packet->pkt->length =
        payload_data_length - REDHeaderLength - blockLength;

  } else if (received_packet->is_fec) {
    // everything behind the RED header
    memcpy(
        received_packet->pkt->data,
        incoming_rtp_packet + rtp_header->header.headerLength + REDHeaderLength,
        payload_data_length - REDHeaderLength);
    received_packet->pkt->length = payload_data_length - REDHeaderLength;
    received_packet->ssrc =
        ModuleRTPUtility::BufferToUWord32(&incoming_rtp_packet[8]);

  } else {
    // copy the RTP header
    memcpy(received_packet->pkt->data, incoming_rtp_packet,
           rtp_header->header.headerLength);

    // replace the RED payload type
    received_packet->pkt->data[1] &= 0x80;  // reset the payload
    received_packet->pkt->data[1] +=
        payload_type;                       // set the media payload type

    // copy the media payload data
    memcpy(
        received_packet->pkt->data + rtp_header->header.headerLength,
        incoming_rtp_packet + rtp_header->header.headerLength + REDHeaderLength,
        payload_data_length - REDHeaderLength);

    received_packet->pkt->length =
        rtp_header->header.headerLength + payload_data_length - REDHeaderLength;
  }

  if (received_packet->pkt->length == 0) {
    delete second_received_packet;
    delete received_packet;
    return 0;
  }

  received_packet_list_.push_back(received_packet);
  if (second_received_packet) {
    received_packet_list_.push_back(second_received_packet);
  }
  return 0;
}

int32_t ReceiverFEC::ProcessReceivedFEC() {
  if (!received_packet_list_.empty()) {
    // Send received media packet to VCM.
    if (!received_packet_list_.front()->is_fec) {
      if (ParseAndReceivePacket(received_packet_list_.front()->pkt) != 0) {
        return -1;
      }
    }
    if (fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_) != 0) {
      return -1;
    }
    assert(received_packet_list_.empty());
  }
  // Send any recovered media packets to VCM.
  ForwardErrorCorrection::RecoveredPacketList::iterator it =
      recovered_packet_list_.begin();
  for (; it != recovered_packet_list_.end(); ++it) {
    if ((*it)->returned)  // Already sent to the VCM and the jitter buffer.
      continue;
    if (ParseAndReceivePacket((*it)->pkt) != 0) {
      return -1;
    }
    (*it)->returned = true;
  }
  return 0;
}

int ReceiverFEC::ParseAndReceivePacket(
    const ForwardErrorCorrection::Packet* packet) {
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  ModuleRTPUtility::RTPHeaderParser parser(packet->data, packet->length);
  if (!parser.Parse(header.header)) {
    return -1;
  }
  if (owner_->ReceiveRecoveredPacketCallback(
          &header, &packet->data[header.header.headerLength],
          packet->length - header.header.headerLength) != 0) {
    return -1;
  }
  return 0;
}

}  // namespace webrtc
