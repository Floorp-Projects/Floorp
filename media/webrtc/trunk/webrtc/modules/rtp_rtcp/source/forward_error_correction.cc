/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/forward_error_correction.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>

#include "modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

// Minimum RTP header size in bytes.
const uint8_t kRtpHeaderSize = 12;

// FEC header size in bytes.
const uint8_t kFecHeaderSize = 10;

// ULP header size in bytes (L bit is set).
const uint8_t kUlpHeaderSizeLBitSet = (2 + kMaskSizeLBitSet);

// ULP header size in bytes (L bit is cleared).
const uint8_t kUlpHeaderSizeLBitClear = (2 + kMaskSizeLBitClear);

// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
const uint8_t kTransportOverhead = 28;

enum { kMaxFecPackets = ForwardErrorCorrection::kMaxMediaPackets };

// Used to link media packets to their protecting FEC packets.
//
// TODO(holmer): Refactor into a proper class.
class ProtectedPacket : public ForwardErrorCorrection::SortablePacket {
 public:
  scoped_refptr<ForwardErrorCorrection::Packet> pkt;
};

typedef std::list<ProtectedPacket*> ProtectedPacketList;

//
// Used for internal storage of FEC packets in a list.
//
// TODO(holmer): Refactor into a proper class.
class FecPacket : public ForwardErrorCorrection::SortablePacket {
 public:
    ProtectedPacketList protectedPktList;
    uint32_t ssrc;  // SSRC of the current frame.
    scoped_refptr<ForwardErrorCorrection::Packet> pkt;
};

bool ForwardErrorCorrection::SortablePacket::LessThan(
    const SortablePacket* first,
    const SortablePacket* second) {
  return IsNewerSequenceNumber(second->seqNum, first->seqNum);
}

ForwardErrorCorrection::ForwardErrorCorrection(int32_t id)
    : _id(id),
      _generatedFecPackets(kMaxMediaPackets),
      _fecPacketReceived(false) {
}

ForwardErrorCorrection::~ForwardErrorCorrection() {
}

// Input packet
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                    RTP Header (12 octets)                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                         RTP Payload                           |
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Output packet
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                    FEC Header (10 octets)                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                      FEC Level 0 Header                       |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                     FEC Level 0 Payload                       |
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
int32_t ForwardErrorCorrection::GenerateFEC(
    const PacketList& mediaPacketList,
    uint8_t protectionFactor,
    int numImportantPackets,
    bool useUnequalProtection,
    FecMaskType fec_mask_type,
    PacketList* fecPacketList) {
  if (mediaPacketList.empty()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s media packet list is empty", __FUNCTION__);
    return -1;
  }
  if (!fecPacketList->empty()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s FEC packet list is not empty", __FUNCTION__);
    return -1;
  }
  const uint16_t numMediaPackets = mediaPacketList.size();
  bool lBit = (numMediaPackets > 8 * kMaskSizeLBitClear);
  int numMaskBytes = lBit ? kMaskSizeLBitSet : kMaskSizeLBitClear;

  if (numMediaPackets > kMaxMediaPackets) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s can only protect %d media packets per frame; %d requested",
                 __FUNCTION__, kMaxMediaPackets, numMediaPackets);
    return -1;
  }

  // Error checking on the number of important packets.
  // Can't have more important packets than media packets.
  if (numImportantPackets > numMediaPackets) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
        "Number of important packets (%d) greater than number of media "
        "packets (%d)", numImportantPackets, numMediaPackets);
    return -1;
  }
  if (numImportantPackets < 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "Number of important packets (%d) less than zero",
                 numImportantPackets);
    return -1;
  }
  // Do some error checking on the media packets.
  PacketList::const_iterator mediaListIt = mediaPacketList.begin();
  while (mediaListIt != mediaPacketList.end()) {
    Packet* mediaPacket = *mediaListIt;
    assert(mediaPacket);

    if (mediaPacket->length < kRtpHeaderSize) {
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                   "%s media packet (%d bytes) is smaller than RTP header",
                   __FUNCTION__, mediaPacket->length);
      return -1;
    }

    // Ensure our FEC packets will fit in a typical MTU.
    if (mediaPacket->length + PacketOverhead() + kTransportOverhead >
        IP_PACKET_SIZE) {
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
          "%s media packet (%d bytes) with overhead is larger than MTU(%d)",
          __FUNCTION__, mediaPacket->length, IP_PACKET_SIZE);
      return -1;
    }
    mediaListIt++;
  }

  int numFecPackets = GetNumberOfFecPackets(numMediaPackets, protectionFactor);
  if (numFecPackets == 0) {
    return 0;
  }

  // Prepare FEC packets by setting them to 0.
  for (int i = 0; i < numFecPackets; i++) {
    memset(_generatedFecPackets[i].data, 0, IP_PACKET_SIZE);
    _generatedFecPackets[i].length = 0;  // Use this as a marker for untouched
    // packets.
    fecPacketList->push_back(&_generatedFecPackets[i]);
  }

  const internal::PacketMaskTable mask_table(fec_mask_type, numMediaPackets);

  // -- Generate packet masks --
  // Always allocate space for a large mask.
  uint8_t* packetMask = new uint8_t[numFecPackets * kMaskSizeLBitSet];
  memset(packetMask, 0, numFecPackets * numMaskBytes);
  internal::GeneratePacketMasks(numMediaPackets, numFecPackets,
                                numImportantPackets, useUnequalProtection,
                                mask_table, packetMask);

  int numMaskBits = InsertZerosInBitMasks(mediaPacketList, packetMask,
                                          numMaskBytes, numFecPackets);

  lBit = (numMaskBits > 8 * kMaskSizeLBitClear);

  if (numMaskBits < 0) {
    delete [] packetMask;
    return -1;
  }
  if (lBit) {
    numMaskBytes = kMaskSizeLBitSet;
  }

  GenerateFecBitStrings(mediaPacketList, packetMask, numFecPackets, lBit);
  GenerateFecUlpHeaders(mediaPacketList, packetMask, lBit, numFecPackets);

  delete [] packetMask;
  return 0;
}

int ForwardErrorCorrection::GetNumberOfFecPackets(int numMediaPackets,
                                                  int protectionFactor) {
  // Result in Q0 with an unsigned round.
  int numFecPackets = (numMediaPackets * protectionFactor + (1 << 7)) >> 8;
  // Generate at least one FEC packet if we need protection.
  if (protectionFactor > 0 && numFecPackets == 0) {
    numFecPackets = 1;
  }
  assert(numFecPackets <= numMediaPackets);
  return numFecPackets;
}

void ForwardErrorCorrection::GenerateFecBitStrings(
    const PacketList& mediaPacketList,
    uint8_t* packetMask,
    int numFecPackets,
    bool lBit) {
  if (mediaPacketList.empty()) {
    return;
  }
  uint8_t mediaPayloadLength[2];
  const int numMaskBytes = lBit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulpHeaderSize = lBit ?
      kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;
  const uint16_t fecRtpOffset = kFecHeaderSize + ulpHeaderSize - kRtpHeaderSize;

  for (int i = 0; i < numFecPackets; i++) {
    PacketList::const_iterator mediaListIt = mediaPacketList.begin();
    uint32_t pktMaskIdx = i * numMaskBytes;
    uint32_t mediaPktIdx = 0;
    uint16_t fecPacketLength = 0;
    uint16_t prevSeqNum = ParseSequenceNumber((*mediaListIt)->data);
    while (mediaListIt != mediaPacketList.end()) {
      // Each FEC packet has a multiple byte mask.
      if (packetMask[pktMaskIdx] & (1 << (7 - mediaPktIdx))) {
        Packet* mediaPacket = *mediaListIt;

        // Assign network-ordered media payload length.
        ModuleRTPUtility::AssignUWord16ToBuffer(
            mediaPayloadLength,
            mediaPacket->length - kRtpHeaderSize);

        fecPacketLength = mediaPacket->length + fecRtpOffset;
        // On the first protected packet, we don't need to XOR.
        if (_generatedFecPackets[i].length == 0) {
          // Copy the first 2 bytes of the RTP header.
          memcpy(_generatedFecPackets[i].data, mediaPacket->data, 2);
          // Copy the 5th to 8th bytes of the RTP header.
          memcpy(&_generatedFecPackets[i].data[4], &mediaPacket->data[4], 4);
          // Copy network-ordered payload size.
          memcpy(&_generatedFecPackets[i].data[8], mediaPayloadLength, 2);

          // Copy RTP payload, leaving room for the ULP header.
          memcpy(&_generatedFecPackets[i].data[kFecHeaderSize + ulpHeaderSize],
                 &mediaPacket->data[kRtpHeaderSize],
                 mediaPacket->length - kRtpHeaderSize);
        } else {
          // XOR with the first 2 bytes of the RTP header.
          _generatedFecPackets[i].data[0] ^= mediaPacket->data[0];
          _generatedFecPackets[i].data[1] ^= mediaPacket->data[1];

          // XOR with the 5th to 8th bytes of the RTP header.
          for (uint32_t j = 4; j < 8; j++) {
            _generatedFecPackets[i].data[j] ^= mediaPacket->data[j];
          }

          // XOR with the network-ordered payload size.
          _generatedFecPackets[i].data[8] ^= mediaPayloadLength[0];
          _generatedFecPackets[i].data[9] ^= mediaPayloadLength[1];

          // XOR with RTP payload, leaving room for the ULP header.
          for (int32_t j = kFecHeaderSize + ulpHeaderSize;
              j < fecPacketLength; j++) {
            _generatedFecPackets[i].data[j] ^=
                mediaPacket->data[j - fecRtpOffset];
          }
        }
        if (fecPacketLength > _generatedFecPackets[i].length) {
          _generatedFecPackets[i].length = fecPacketLength;
        }
      }
      mediaListIt++;
      if (mediaListIt != mediaPacketList.end()) {
        uint16_t seqNum = ParseSequenceNumber((*mediaListIt)->data);
        mediaPktIdx += static_cast<uint16_t>(seqNum - prevSeqNum);
        prevSeqNum = seqNum;
      }
      if (mediaPktIdx == 8) {
        // Switch to the next mask byte.
        mediaPktIdx = 0;
        pktMaskIdx++;
      }
    }
    assert(_generatedFecPackets[i].length);
    //Note: This shouldn't happen: means packet mask is wrong or poorly designed
  }
}

int ForwardErrorCorrection::InsertZerosInBitMasks(
    const PacketList& media_packets,
    uint8_t* packet_mask,
    int num_mask_bytes,
    int num_fec_packets) {
  uint8_t* new_mask = NULL;
  if (media_packets.size() <= 1) {
    return media_packets.size();
  }
  int last_seq_num = ParseSequenceNumber(media_packets.back()->data);
  int first_seq_num = ParseSequenceNumber(media_packets.front()->data);
  int total_missing_seq_nums = static_cast<uint16_t>(last_seq_num -
                                                     first_seq_num) -
                                                     media_packets.size() + 1;
  if (total_missing_seq_nums == 0) {
    // All sequence numbers are covered by the packet mask. No zero insertion
    // required.
    return media_packets.size();
  }
  // Allocate the new mask.
  int new_mask_bytes = kMaskSizeLBitClear;
  if (media_packets.size() + total_missing_seq_nums > 8 * kMaskSizeLBitClear) {
    new_mask_bytes = kMaskSizeLBitSet;
  }
  new_mask = new uint8_t[num_fec_packets * kMaskSizeLBitSet];
  memset(new_mask, 0, num_fec_packets * kMaskSizeLBitSet);

  PacketList::const_iterator it = media_packets.begin();
  uint16_t prev_seq_num = first_seq_num;
  ++it;

  // Insert the first column.
  CopyColumn(new_mask, new_mask_bytes, packet_mask, num_mask_bytes,
             num_fec_packets, 0, 0);
  int new_bit_index = 1;
  int old_bit_index = 1;
  // Insert zeros in the bit mask for every hole in the sequence.
  for (; it != media_packets.end(); ++it) {
    if (new_bit_index == 8 * kMaskSizeLBitSet) {
      // We can only cover up to 48 packets.
      break;
    }
    uint16_t seq_num = ParseSequenceNumber((*it)->data);
    const int zeros_to_insert =
        static_cast<uint16_t>(seq_num - prev_seq_num - 1);
    if (zeros_to_insert > 0) {
      InsertZeroColumns(zeros_to_insert, new_mask, new_mask_bytes,
                        num_fec_packets, new_bit_index);
    }
    new_bit_index += zeros_to_insert;
    CopyColumn(new_mask, new_mask_bytes, packet_mask, num_mask_bytes,
               num_fec_packets, new_bit_index, old_bit_index);
    ++new_bit_index;
    ++old_bit_index;
    prev_seq_num = seq_num;
  }
  if (new_bit_index % 8 != 0) {
    // We didn't fill the last byte. Shift bits to correct position.
    for (uint16_t row = 0; row < num_fec_packets; ++row) {
      int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
      new_mask[new_byte_index] <<= (7 - (new_bit_index % 8));
    }
  }
  // Replace the old mask with the new.
  memcpy(packet_mask, new_mask, kMaskSizeLBitSet * num_fec_packets);
  delete [] new_mask;
  return new_bit_index;
}

void ForwardErrorCorrection::InsertZeroColumns(int num_zeros,
                                               uint8_t* new_mask,
                                               int new_mask_bytes,
                                               int num_fec_packets,
                                               int new_bit_index) {
  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    const int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    const int max_shifts = (7 - (new_bit_index % 8));
    new_mask[new_byte_index] <<= std::min(num_zeros, max_shifts);
  }
}

void ForwardErrorCorrection::CopyColumn(uint8_t* new_mask,
                                        int new_mask_bytes,
                                        uint8_t* old_mask,
                                        int old_mask_bytes,
                                        int num_fec_packets,
                                        int new_bit_index,
                                        int old_bit_index) {
  // Copy column from the old mask to the beginning of the new mask and shift it
  // out from the old mask.
  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    int old_byte_index = row * old_mask_bytes + old_bit_index / 8;
    new_mask[new_byte_index] |= ((old_mask[old_byte_index] & 0x80) >> 7);
    if (new_bit_index % 8 != 7) {
      new_mask[new_byte_index] <<= 1;
    }
    old_mask[old_byte_index] <<= 1;
  }
}

void ForwardErrorCorrection::GenerateFecUlpHeaders(
    const PacketList& mediaPacketList,
    uint8_t* packetMask,
    bool lBit,
    int numFecPackets) {
  // -- Generate FEC and ULP headers --
  //
  // FEC Header, 10 bytes
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |E|L|P|X|  CC   |M| PT recovery |            SN base            |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                          TS recovery                          |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |        length recovery        |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  // ULP Header, 4 bytes (for L = 0)
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |       Protection Length       |             mask              |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |              mask cont. (present only when L = 1)             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  PacketList::const_iterator mediaListIt = mediaPacketList.begin();
  Packet* mediaPacket = *mediaListIt;
  assert(mediaPacket != NULL);
  int numMaskBytes = lBit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulpHeaderSize = lBit ?
      kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;

  for (int i = 0; i < numFecPackets; i++) {
    // -- FEC header --
    _generatedFecPackets[i].data[0] &= 0x7f; // Set E to zero.
    if (lBit == 0) {
      _generatedFecPackets[i].data[0] &= 0xbf; // Clear the L bit.
    } else {
      _generatedFecPackets[i].data[0] |= 0x40; // Set the L bit.
    }
    // Two byte sequence number from first RTP packet to SN base.
    // We use the same sequence number base for every FEC packet,
    // but that's not required in general.
    memcpy(&_generatedFecPackets[i].data[2], &mediaPacket->data[2], 2);

    // -- ULP header --
    // Copy the payload size to the protection length field.
    // (We protect the entire packet.)
    ModuleRTPUtility::AssignUWord16ToBuffer(&_generatedFecPackets[i].data[10],
        _generatedFecPackets[i].length - kFecHeaderSize - ulpHeaderSize);

    // Copy the packet mask.
    memcpy(&_generatedFecPackets[i].data[12], &packetMask[i * numMaskBytes],
           numMaskBytes);
  }
}

void ForwardErrorCorrection::ResetState(
    RecoveredPacketList* recoveredPacketList) {
  _fecPacketReceived = false;

  // Free the memory for any existing recovered packets, if the user hasn't.
  while (!recoveredPacketList->empty()) {
    delete recoveredPacketList->front();
    recoveredPacketList->pop_front();
  }
  assert(recoveredPacketList->empty());

  // Free the FEC packet list.
  while (!_fecPacketList.empty()) {
    FecPacketList::iterator fecPacketListIt = _fecPacketList.begin();
    FecPacket* fecPacket = *fecPacketListIt;
    ProtectedPacketList::iterator protectedPacketListIt;
    protectedPacketListIt = fecPacket->protectedPktList.begin();
    while (protectedPacketListIt != fecPacket->protectedPktList.end()) {
      delete *protectedPacketListIt;
      protectedPacketListIt =
          fecPacket->protectedPktList.erase(protectedPacketListIt);
    }
    assert(fecPacket->protectedPktList.empty());
    delete fecPacket;
    _fecPacketList.pop_front();
  }
  assert(_fecPacketList.empty());
}

void ForwardErrorCorrection::InsertMediaPacket(
    ReceivedPacket* rxPacket,
    RecoveredPacketList* recoveredPacketList) {
  RecoveredPacketList::iterator recoveredPacketListIt =
      recoveredPacketList->begin();

  // Search for duplicate packets.
  while (recoveredPacketListIt != recoveredPacketList->end()) {
    if (rxPacket->seqNum == (*recoveredPacketListIt)->seqNum) {
      // Duplicate packet, no need to add to list.
      // Delete duplicate media packet data.
      rxPacket->pkt = NULL;
      return;
    }
    recoveredPacketListIt++;
  }
  RecoveredPacket* recoverdPacketToInsert = new RecoveredPacket;
  recoverdPacketToInsert->wasRecovered = false;
  // Inserted Media packet is already sent to VCM.
  recoverdPacketToInsert->returned = true;
  recoverdPacketToInsert->seqNum = rxPacket->seqNum;
  recoverdPacketToInsert->pkt = rxPacket->pkt;
  recoverdPacketToInsert->pkt->length = rxPacket->pkt->length;

  // TODO(holmer): Consider replacing this with a binary search for the right
  // position, and then just insert the new packet. Would get rid of the sort.
  recoveredPacketList->push_back(recoverdPacketToInsert);
  recoveredPacketList->sort(SortablePacket::LessThan);
  UpdateCoveringFECPackets(recoverdPacketToInsert);
}

void ForwardErrorCorrection::UpdateCoveringFECPackets(RecoveredPacket* packet) {
  for (FecPacketList::iterator it = _fecPacketList.begin();
      it != _fecPacketList.end(); ++it) {
    // Is this FEC packet protecting the media packet |packet|?
    ProtectedPacketList::iterator protected_it = std::lower_bound(
        (*it)->protectedPktList.begin(),
        (*it)->protectedPktList.end(),
        packet,
        SortablePacket::LessThan);
    if (protected_it != (*it)->protectedPktList.end() &&
        (*protected_it)->seqNum == packet->seqNum) {
      // Found an FEC packet which is protecting |packet|.
      (*protected_it)->pkt = packet->pkt;
    }
  }
}

void ForwardErrorCorrection::InsertFECPacket(
    ReceivedPacket* rxPacket,
    const RecoveredPacketList* recoveredPacketList) {
  _fecPacketReceived = true;

  // Check for duplicate.
  FecPacketList::iterator fecPacketListIt = _fecPacketList.begin();
  while (fecPacketListIt != _fecPacketList.end()) {
    if (rxPacket->seqNum == (*fecPacketListIt)->seqNum) {
      // Delete duplicate FEC packet data.
      rxPacket->pkt = NULL;
      return;
    }
    fecPacketListIt++;
  }
  FecPacket* fecPacket = new FecPacket;
  fecPacket->pkt = rxPacket->pkt;
  fecPacket->seqNum = rxPacket->seqNum;
  fecPacket->ssrc = rxPacket->ssrc;

  const uint16_t seqNumBase = ModuleRTPUtility::BufferToUWord16(
      &fecPacket->pkt->data[2]);
  const uint16_t maskSizeBytes = (fecPacket->pkt->data[0] & 0x40) ?
      kMaskSizeLBitSet : kMaskSizeLBitClear;  // L bit set?

  for (uint16_t byteIdx = 0; byteIdx < maskSizeBytes; byteIdx++) {
    uint8_t packetMask = fecPacket->pkt->data[12 + byteIdx];
    for (uint16_t bitIdx = 0; bitIdx < 8; bitIdx++) {
      if (packetMask & (1 << (7 - bitIdx))) {
        ProtectedPacket* protectedPacket = new ProtectedPacket;
        fecPacket->protectedPktList.push_back(protectedPacket);
        // This wraps naturally with the sequence number.
        protectedPacket->seqNum = static_cast<uint16_t>(seqNumBase +
            (byteIdx << 3) + bitIdx);
        protectedPacket->pkt = NULL;
      }
    }
  }
  if (fecPacket->protectedPktList.empty()) {
    // All-zero packet mask; we can discard this FEC packet.
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "FEC packet %u has an all-zero packet mask.",
                 fecPacket->seqNum, __FUNCTION__);
    delete fecPacket;
  } else {
    AssignRecoveredPackets(fecPacket,
                           recoveredPacketList);
    // TODO(holmer): Consider replacing this with a binary search for the right
    // position, and then just insert the new packet. Would get rid of the sort.
    _fecPacketList.push_back(fecPacket);
    _fecPacketList.sort(SortablePacket::LessThan);
    if (_fecPacketList.size() > kMaxFecPackets) {
      DiscardFECPacket(_fecPacketList.front());
      _fecPacketList.pop_front();
    }
    assert(_fecPacketList.size() <= kMaxFecPackets);
  }
}

void ForwardErrorCorrection::AssignRecoveredPackets(
    FecPacket* fec_packet,
    const RecoveredPacketList* recovered_packets) {
  // Search for missing packets which have arrived or have been recovered by
  // another FEC packet.
  ProtectedPacketList* not_recovered = &fec_packet->protectedPktList;
  RecoveredPacketList already_recovered;
  std::set_intersection(
      recovered_packets->begin(), recovered_packets->end(),
      not_recovered->begin(), not_recovered->end(),
      std::inserter(already_recovered, already_recovered.end()),
      SortablePacket::LessThan);
  // Set the FEC pointers to all recovered packets so that we don't have to
  // search for them when we are doing recovery.
  ProtectedPacketList::iterator not_recovered_it = not_recovered->begin();
  for (RecoveredPacketList::iterator it = already_recovered.begin();
      it != already_recovered.end(); ++it) {
    // Search for the next recovered packet in |not_recovered|.
    while ((*not_recovered_it)->seqNum != (*it)->seqNum)
      ++not_recovered_it;
    (*not_recovered_it)->pkt = (*it)->pkt;
  }
}

void ForwardErrorCorrection::InsertPackets(
    ReceivedPacketList* receivedPacketList,
    RecoveredPacketList* recoveredPacketList) {

  while (!receivedPacketList->empty()) {
    ReceivedPacket* rxPacket = receivedPacketList->front();

    if (rxPacket->isFec) {
      InsertFECPacket(rxPacket, recoveredPacketList);
    } else {
      // Insert packet at the end of |recoveredPacketList|.
      InsertMediaPacket(rxPacket, recoveredPacketList);
    }
    // Delete the received packet "wrapper", but not the packet data.
    delete rxPacket;
    receivedPacketList->pop_front();
  }
  assert(receivedPacketList->empty());
  DiscardOldPackets(recoveredPacketList);
}

void ForwardErrorCorrection::InitRecovery(
    const FecPacket* fec_packet,
    RecoveredPacket* recovered) {
  // This is the first packet which we try to recover with.
  const uint16_t ulpHeaderSize = fec_packet->pkt->data[0] & 0x40 ?
      kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;  // L bit set?
  recovered->pkt = new Packet;
  memset(recovered->pkt->data, 0, IP_PACKET_SIZE);
  recovered->returned = false;
  recovered->wasRecovered = true;
  uint8_t protectionLength[2];
  // Copy the protection length from the ULP header.
  memcpy(protectionLength, &fec_packet->pkt->data[10], 2);
  // Copy FEC payload, skipping the ULP header.
  memcpy(&recovered->pkt->data[kRtpHeaderSize],
         &fec_packet->pkt->data[kFecHeaderSize + ulpHeaderSize],
         ModuleRTPUtility::BufferToUWord16(protectionLength));
  // Copy the length recovery field.
  memcpy(recovered->length_recovery, &fec_packet->pkt->data[8], 2);
  // Copy the first 2 bytes of the FEC header.
  memcpy(recovered->pkt->data, fec_packet->pkt->data, 2);
  // Copy the 5th to 8th bytes of the FEC header.
  memcpy(&recovered->pkt->data[4], &fec_packet->pkt->data[4], 4);
  // Set the SSRC field.
  ModuleRTPUtility::AssignUWord32ToBuffer(&recovered->pkt->data[8],
                                          fec_packet->ssrc);
}

void ForwardErrorCorrection::FinishRecovery(RecoveredPacket* recovered) {
  // Set the RTP version to 2.
  recovered->pkt->data[0] |= 0x80;  // Set the 1st bit.
  recovered->pkt->data[0] &= 0xbf;  // Clear the 2nd bit.

  // Set the SN field.
  ModuleRTPUtility::AssignUWord16ToBuffer(&recovered->pkt->data[2],
                                          recovered->seqNum);
  // Recover the packet length.
  recovered->pkt->length = ModuleRTPUtility::BufferToUWord16(
      recovered->length_recovery) + kRtpHeaderSize;
}

void ForwardErrorCorrection::XorPackets(const Packet* src_packet,
                                        RecoveredPacket* dst_packet) {
  // XOR with the first 2 bytes of the RTP header.
  for (uint32_t i = 0; i < 2; i++) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
  // XOR with the 5th to 8th bytes of the RTP header.
  for (uint32_t i = 4; i < 8; i++) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
  // XOR with the network-ordered payload size.
  uint8_t mediaPayloadLength[2];
  ModuleRTPUtility::AssignUWord16ToBuffer(
      mediaPayloadLength,
      src_packet->length - kRtpHeaderSize);
  dst_packet->length_recovery[0] ^= mediaPayloadLength[0];
  dst_packet->length_recovery[1] ^= mediaPayloadLength[1];

  // XOR with RTP payload.
  // TODO(marpan/ajm): Are we doing more XORs than required here?
  for (int32_t i = kRtpHeaderSize; i < src_packet->length; i++) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
}

void ForwardErrorCorrection::RecoverPacket(
    const FecPacket* fecPacket,
    RecoveredPacket* recPacketToInsert) {
  InitRecovery(fecPacket, recPacketToInsert);
  ProtectedPacketList::const_iterator protected_it =
      fecPacket->protectedPktList.begin();
  while (protected_it != fecPacket->protectedPktList.end()) {
    if ((*protected_it)->pkt == NULL) {
      // This is the packet we're recovering.
      recPacketToInsert->seqNum = (*protected_it)->seqNum;
    } else {
      XorPackets((*protected_it)->pkt, recPacketToInsert);
    }
    ++protected_it;
  }
  FinishRecovery(recPacketToInsert);
}

void ForwardErrorCorrection::AttemptRecover(
    RecoveredPacketList* recoveredPacketList) {
  FecPacketList::iterator fecPacketListIt = _fecPacketList.begin();
  while (fecPacketListIt != _fecPacketList.end()) {
    // Search for each FEC packet's protected media packets.
    int packets_missing = NumCoveredPacketsMissing(*fecPacketListIt);

    // We can only recover one packet with an FEC packet.
   if (packets_missing == 1) {
      // Recovery possible.
      RecoveredPacket* packetToInsert = new RecoveredPacket;
      packetToInsert->pkt = NULL;
      RecoverPacket(*fecPacketListIt, packetToInsert);

      // Add recovered packet to the list of recovered packets and update any
      // FEC packets covering this packet with a pointer to the data.
      // TODO(holmer): Consider replacing this with a binary search for the
      // right position, and then just insert the new packet. Would get rid of
      // the sort.
      recoveredPacketList->push_back(packetToInsert);
      recoveredPacketList->sort(SortablePacket::LessThan);
      UpdateCoveringFECPackets(packetToInsert);
      DiscardOldPackets(recoveredPacketList);
      DiscardFECPacket(*fecPacketListIt);
      fecPacketListIt = _fecPacketList.erase(fecPacketListIt);

      // A packet has been recovered. We need to check the FEC list again, as
      // this may allow additional packets to be recovered.
      // Restart for first FEC packet.
      fecPacketListIt = _fecPacketList.begin();
    } else if (packets_missing == 0) {
        // Either all protected packets arrived or have been recovered. We can
        // discard this FEC packet.
        DiscardFECPacket(*fecPacketListIt);
        fecPacketListIt = _fecPacketList.erase(fecPacketListIt);
    } else {
      fecPacketListIt++;
    }
  }
}

int ForwardErrorCorrection::NumCoveredPacketsMissing(
    const FecPacket* fec_packet) {
  int packets_missing = 0;
  ProtectedPacketList::const_iterator it = fec_packet->protectedPktList.begin();
  for (; it != fec_packet->protectedPktList.end(); ++it) {
    if ((*it)->pkt == NULL) {
      ++packets_missing;
      if (packets_missing > 1) {
        break;  // We can't recover more than one packet.
      }
    }
  }
  return packets_missing;
}

void ForwardErrorCorrection::DiscardFECPacket(FecPacket* fec_packet) {
  while (!fec_packet->protectedPktList.empty()) {
    delete fec_packet->protectedPktList.front();
    fec_packet->protectedPktList.pop_front();
  }
  assert(fec_packet->protectedPktList.empty());
  delete fec_packet;
}

void ForwardErrorCorrection::DiscardOldPackets(
    RecoveredPacketList* recoveredPacketList) {
  while (recoveredPacketList->size() > kMaxMediaPackets) {
    ForwardErrorCorrection::RecoveredPacket* packet =
        recoveredPacketList->front();
    delete packet;
    recoveredPacketList->pop_front();
  }
  assert(recoveredPacketList->size() <= kMaxMediaPackets);
}

uint16_t ForwardErrorCorrection::ParseSequenceNumber(uint8_t* packet) {
  return (packet[2] << 8) + packet[3];
}

int32_t ForwardErrorCorrection::DecodeFEC(
    ReceivedPacketList* receivedPacketList,
    RecoveredPacketList* recoveredPacketList) {
  // TODO(marpan/ajm): can we check for multiple ULP headers, and return an
  // error?
  if (recoveredPacketList->size() == kMaxMediaPackets) {
    const unsigned int seq_num_diff = abs(
        static_cast<int>(receivedPacketList->front()->seqNum)  -
        static_cast<int>(recoveredPacketList->back()->seqNum));
    if (seq_num_diff > kMaxMediaPackets) {
      // A big gap in sequence numbers. The old recovered packets
      // are now useless, so it's safe to do a reset.
      ResetState(recoveredPacketList);
    }
  }
  InsertPackets(receivedPacketList, recoveredPacketList);
  AttemptRecover(recoveredPacketList);
  return 0;
}

uint16_t ForwardErrorCorrection::PacketOverhead() {
  return kFecHeaderSize + kUlpHeaderSizeLBitSet;
}
} // namespace webrtc
