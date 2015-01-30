/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/fec_receiver_impl.h"

#include <assert.h>

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/logging.h"

// RFC 5109
namespace webrtc {

FecReceiver* FecReceiver::Create(RtpData* callback) {
  return new FecReceiverImpl(callback);
}

FecReceiverImpl::FecReceiverImpl(RtpData* callback)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      recovered_packet_callback_(callback),
      fec_(new ForwardErrorCorrection()) {}

FecReceiverImpl::~FecReceiverImpl() {
  while (!received_packet_list_.empty()) {
    delete received_packet_list_.front();
    received_packet_list_.pop_front();
  }
  if (fec_ != NULL) {
    fec_->ResetState(&recovered_packet_list_);
    delete fec_;
  }
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

int32_t FecReceiverImpl::AddReceivedRedPacket(
    const RTPHeader& header, const uint8_t* incoming_rtp_packet,
    int packet_length, uint8_t ulpfec_payload_type) {
  CriticalSectionScoped cs(crit_sect_.get());
  uint8_t REDHeaderLength = 1;
  uint16_t payload_data_length = packet_length - header.headerLength;

  // Add to list without RED header, aka a virtual RTP packet
  // we remove the RED header

  ForwardErrorCorrection::ReceivedPacket* received_packet =
      new ForwardErrorCorrection::ReceivedPacket;
  received_packet->pkt = new ForwardErrorCorrection::Packet;

  // get payload type from RED header
  uint8_t payload_type =
      incoming_rtp_packet[header.headerLength] & 0x7f;

  received_packet->is_fec = payload_type == ulpfec_payload_type;
  received_packet->seq_num = header.sequenceNumber;

  uint16_t blockLength = 0;
  if (incoming_rtp_packet[header.headerLength] & 0x80) {
    // f bit set in RED header
    REDHeaderLength = 4;
    uint16_t timestamp_offset =
        (incoming_rtp_packet[header.headerLength + 1]) << 8;
    timestamp_offset +=
        incoming_rtp_packet[header.headerLength + 2];
    timestamp_offset = timestamp_offset >> 2;
    if (timestamp_offset != 0) {
      // |timestampOffset| should be 0. However, it's possible this is the first
      // location a corrupt payload can be caught, so don't assert.
      LOG(LS_WARNING) << "Corrupt payload found.";
      delete received_packet;
      return -1;
    }

    blockLength =
        (0x03 & incoming_rtp_packet[header.headerLength + 2]) << 8;
    blockLength += (incoming_rtp_packet[header.headerLength + 3]);

    // check next RED header
    if (incoming_rtp_packet[header.headerLength + 4] & 0x80) {
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
           header.headerLength);

    // replace the RED payload type
    received_packet->pkt->data[1] &= 0x80;  // reset the payload
    received_packet->pkt->data[1] +=
        payload_type;                       // set the media payload type

    // copy the payload data
    memcpy(
        received_packet->pkt->data + header.headerLength,
        incoming_rtp_packet + header.headerLength + REDHeaderLength,
        blockLength);

    received_packet->pkt->length = blockLength;

    second_received_packet = new ForwardErrorCorrection::ReceivedPacket;
    second_received_packet->pkt = new ForwardErrorCorrection::Packet;

    second_received_packet->is_fec = true;
    second_received_packet->seq_num = header.sequenceNumber;

    // copy the FEC payload data
    memcpy(second_received_packet->pkt->data,
           incoming_rtp_packet + header.headerLength +
               REDHeaderLength + blockLength,
           payload_data_length - REDHeaderLength - blockLength);

    second_received_packet->pkt->length =
        payload_data_length - REDHeaderLength - blockLength;

  } else if (received_packet->is_fec) {
    // everything behind the RED header
    memcpy(
        received_packet->pkt->data,
        incoming_rtp_packet + header.headerLength + REDHeaderLength,
        payload_data_length - REDHeaderLength);
    received_packet->pkt->length = payload_data_length - REDHeaderLength;
    received_packet->ssrc =
        RtpUtility::BufferToUWord32(&incoming_rtp_packet[8]);

  } else {
    // copy the RTP header
    memcpy(received_packet->pkt->data, incoming_rtp_packet,
           header.headerLength);

    // replace the RED payload type
    received_packet->pkt->data[1] &= 0x80;  // reset the payload
    received_packet->pkt->data[1] +=
        payload_type;                       // set the media payload type

    // copy the media payload data
    memcpy(
        received_packet->pkt->data + header.headerLength,
        incoming_rtp_packet + header.headerLength + REDHeaderLength,
        payload_data_length - REDHeaderLength);

    received_packet->pkt->length =
        header.headerLength + payload_data_length - REDHeaderLength;
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

int32_t FecReceiverImpl::ProcessReceivedFec() {
  crit_sect_->Enter();
  if (!received_packet_list_.empty()) {
    // Send received media packet to VCM.
    if (!received_packet_list_.front()->is_fec) {
      ForwardErrorCorrection::Packet* packet =
          received_packet_list_.front()->pkt;
      crit_sect_->Leave();
      if (!recovered_packet_callback_->OnRecoveredPacket(packet->data,
                                                         packet->length)) {
        return -1;
      }
      crit_sect_->Enter();
    }
    if (fec_->DecodeFEC(&received_packet_list_, &recovered_packet_list_) != 0) {
      crit_sect_->Leave();
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
    ForwardErrorCorrection::Packet* packet = (*it)->pkt;
    crit_sect_->Leave();
    if (!recovered_packet_callback_->OnRecoveredPacket(packet->data,
                                                       packet->length)) {
      return -1;
    }
    crit_sect_->Enter();
    (*it)->returned = true;
  }
  crit_sect_->Leave();
  return 0;
}

}  // namespace webrtc
