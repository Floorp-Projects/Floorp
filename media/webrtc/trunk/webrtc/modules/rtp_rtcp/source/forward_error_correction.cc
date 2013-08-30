/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>

#include "webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/trace.h"

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

enum {
  kMaxFecPackets = ForwardErrorCorrection::kMaxMediaPackets
};

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
  ProtectedPacketList protected_pkt_list;
  uint32_t ssrc;  // SSRC of the current frame.
  scoped_refptr<ForwardErrorCorrection::Packet> pkt;
};

bool ForwardErrorCorrection::SortablePacket::LessThan(
    const SortablePacket* first, const SortablePacket* second) {
  return IsNewerSequenceNumber(second->seq_num, first->seq_num);
}

ForwardErrorCorrection::ForwardErrorCorrection(int32_t id)
    : id_(id),
      generated_fec_packets_(kMaxMediaPackets),
      fec_packet_received_(false) {}

ForwardErrorCorrection::~ForwardErrorCorrection() {}

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
int32_t ForwardErrorCorrection::GenerateFEC(const PacketList& media_packet_list,
                                            uint8_t protection_factor,
                                            int num_important_packets,
                                            bool use_unequal_protection,
                                            FecMaskType fec_mask_type,
                                            PacketList* fec_packet_list) {
  if (media_packet_list.empty()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s media packet list is empty", __FUNCTION__);
    return -1;
  }
  if (!fec_packet_list->empty()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s FEC packet list is not empty", __FUNCTION__);
    return -1;
  }
  const uint16_t num_media_packets = media_packet_list.size();
  bool l_bit = (num_media_packets > 8 * kMaskSizeLBitClear);
  int num_maskBytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;

  if (num_media_packets > kMaxMediaPackets) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "%s can only protect %d media packets per frame; %d requested",
                 __FUNCTION__, kMaxMediaPackets, num_media_packets);
    return -1;
  }

  // Error checking on the number of important packets.
  // Can't have more important packets than media packets.
  if (num_important_packets > num_media_packets) {
    WEBRTC_TRACE(
        kTraceError, kTraceRtpRtcp, id_,
        "Number of important packets (%d) greater than number of media "
        "packets (%d)",
        num_important_packets, num_media_packets);
    return -1;
  }
  if (num_important_packets < 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                 "Number of important packets (%d) less than zero",
                 num_important_packets);
    return -1;
  }
  // Do some error checking on the media packets.
  PacketList::const_iterator media_list_it = media_packet_list.begin();
  while (media_list_it != media_packet_list.end()) {
    Packet* media_packet = *media_list_it;
    assert(media_packet);

    if (media_packet->length < kRtpHeaderSize) {
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id_,
                   "%s media packet (%d bytes) is smaller than RTP header",
                   __FUNCTION__, media_packet->length);
      return -1;
    }

    // Ensure our FEC packets will fit in a typical MTU.
    if (media_packet->length + PacketOverhead() + kTransportOverhead >
        IP_PACKET_SIZE) {
      WEBRTC_TRACE(
          kTraceError, kTraceRtpRtcp, id_,
          "%s media packet (%d bytes) with overhead is larger than MTU(%d)",
          __FUNCTION__, media_packet->length, IP_PACKET_SIZE);
      return -1;
    }
    media_list_it++;
  }

  int num_fec_packets =
      GetNumberOfFecPackets(num_media_packets, protection_factor);
  if (num_fec_packets == 0) {
    return 0;
  }

  // Prepare FEC packets by setting them to 0.
  for (int i = 0; i < num_fec_packets; ++i) {
    memset(generated_fec_packets_[i].data, 0, IP_PACKET_SIZE);
    generated_fec_packets_[i].length = 0;  // Use this as a marker for untouched
                                           // packets.
    fec_packet_list->push_back(&generated_fec_packets_[i]);
  }

  const internal::PacketMaskTable mask_table(fec_mask_type, num_media_packets);

  // -- Generate packet masks --
  // Always allocate space for a large mask.
  uint8_t* packet_mask = new uint8_t[num_fec_packets * kMaskSizeLBitSet];
  memset(packet_mask, 0, num_fec_packets * num_maskBytes);
  internal::GeneratePacketMasks(num_media_packets, num_fec_packets,
                                num_important_packets, use_unequal_protection,
                                mask_table, packet_mask);

  int num_maskBits = InsertZerosInBitMasks(media_packet_list, packet_mask,
                                           num_maskBytes, num_fec_packets);

  l_bit = (num_maskBits > 8 * kMaskSizeLBitClear);

  if (num_maskBits < 0) {
    delete[] packet_mask;
    return -1;
  }
  if (l_bit) {
    num_maskBytes = kMaskSizeLBitSet;
  }

  GenerateFecBitStrings(media_packet_list, packet_mask, num_fec_packets, l_bit);
  GenerateFecUlpHeaders(media_packet_list, packet_mask, l_bit, num_fec_packets);

  delete[] packet_mask;
  return 0;
}

int ForwardErrorCorrection::GetNumberOfFecPackets(int num_media_packets,
                                                  int protection_factor) {
  // Result in Q0 with an unsigned round.
  int num_fec_packets = (num_media_packets * protection_factor + (1 << 7)) >> 8;
  // Generate at least one FEC packet if we need protection.
  if (protection_factor > 0 && num_fec_packets == 0) {
    num_fec_packets = 1;
  }
  assert(num_fec_packets <= num_media_packets);
  return num_fec_packets;
}

void ForwardErrorCorrection::GenerateFecBitStrings(
    const PacketList& media_packet_list, uint8_t* packet_mask,
    int num_fec_packets, bool l_bit) {
  if (media_packet_list.empty()) {
    return;
  }
  uint8_t media_payload_length[2];
  const int num_maskBytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulp_header_size =
      l_bit ? kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;
  const uint16_t fec_rtp_offset =
      kFecHeaderSize + ulp_header_size - kRtpHeaderSize;

  for (int i = 0; i < num_fec_packets; ++i) {
    PacketList::const_iterator media_list_it = media_packet_list.begin();
    uint32_t pkt_mask_idx = i * num_maskBytes;
    uint32_t media_pkt_idx = 0;
    uint16_t fec_packet_length = 0;
    uint16_t prev_seq_num = ParseSequenceNumber((*media_list_it)->data);
    while (media_list_it != media_packet_list.end()) {
      // Each FEC packet has a multiple byte mask.
      if (packet_mask[pkt_mask_idx] & (1 << (7 - media_pkt_idx))) {
        Packet* media_packet = *media_list_it;

        // Assign network-ordered media payload length.
        ModuleRTPUtility::AssignUWord16ToBuffer(
            media_payload_length, media_packet->length - kRtpHeaderSize);

        fec_packet_length = media_packet->length + fec_rtp_offset;
        // On the first protected packet, we don't need to XOR.
        if (generated_fec_packets_[i].length == 0) {
          // Copy the first 2 bytes of the RTP header.
          memcpy(generated_fec_packets_[i].data, media_packet->data, 2);
          // Copy the 5th to 8th bytes of the RTP header.
          memcpy(&generated_fec_packets_[i].data[4], &media_packet->data[4], 4);
          // Copy network-ordered payload size.
          memcpy(&generated_fec_packets_[i].data[8], media_payload_length, 2);

          // Copy RTP payload, leaving room for the ULP header.
          memcpy(
              &generated_fec_packets_[i].data[kFecHeaderSize + ulp_header_size],
              &media_packet->data[kRtpHeaderSize],
              media_packet->length - kRtpHeaderSize);
        } else {
          // XOR with the first 2 bytes of the RTP header.
          generated_fec_packets_[i].data[0] ^= media_packet->data[0];
          generated_fec_packets_[i].data[1] ^= media_packet->data[1];

          // XOR with the 5th to 8th bytes of the RTP header.
          for (uint32_t j = 4; j < 8; ++j) {
            generated_fec_packets_[i].data[j] ^= media_packet->data[j];
          }

          // XOR with the network-ordered payload size.
          generated_fec_packets_[i].data[8] ^= media_payload_length[0];
          generated_fec_packets_[i].data[9] ^= media_payload_length[1];

          // XOR with RTP payload, leaving room for the ULP header.
          for (int32_t j = kFecHeaderSize + ulp_header_size;
               j < fec_packet_length; j++) {
            generated_fec_packets_[i].data[j] ^=
                media_packet->data[j - fec_rtp_offset];
          }
        }
        if (fec_packet_length > generated_fec_packets_[i].length) {
          generated_fec_packets_[i].length = fec_packet_length;
        }
      }
      media_list_it++;
      if (media_list_it != media_packet_list.end()) {
        uint16_t seq_num = ParseSequenceNumber((*media_list_it)->data);
        media_pkt_idx += static_cast<uint16_t>(seq_num - prev_seq_num);
        prev_seq_num = seq_num;
      }
      if (media_pkt_idx == 8) {
        // Switch to the next mask byte.
        media_pkt_idx = 0;
        pkt_mask_idx++;
      }
    }
    assert(generated_fec_packets_[i].length);
    //Note: This shouldn't happen: means packet mask is wrong or poorly designed
  }
}

int ForwardErrorCorrection::InsertZerosInBitMasks(
    const PacketList& media_packets, uint8_t* packet_mask, int num_mask_bytes,
    int num_fec_packets) {
  uint8_t* new_mask = NULL;
  if (media_packets.size() <= 1) {
    return media_packets.size();
  }
  int last_seq_num = ParseSequenceNumber(media_packets.back()->data);
  int first_seq_num = ParseSequenceNumber(media_packets.front()->data);
  int total_missing_seq_nums =
      static_cast<uint16_t>(last_seq_num - first_seq_num) -
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
  delete[] new_mask;
  return new_bit_index;
}

void ForwardErrorCorrection::InsertZeroColumns(int num_zeros, uint8_t* new_mask,
                                               int new_mask_bytes,
                                               int num_fec_packets,
                                               int new_bit_index) {
  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    const int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    const int max_shifts = (7 - (new_bit_index % 8));
    new_mask[new_byte_index] <<= std::min(num_zeros, max_shifts);
  }
}

void ForwardErrorCorrection::CopyColumn(uint8_t* new_mask, int new_mask_bytes,
                                        uint8_t* old_mask, int old_mask_bytes,
                                        int num_fec_packets, int new_bit_index,
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
    const PacketList& media_packet_list, uint8_t* packet_mask, bool l_bit,
    int num_fec_packets) {
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
  PacketList::const_iterator media_list_it = media_packet_list.begin();
  Packet* media_packet = *media_list_it;
  assert(media_packet != NULL);
  int num_maskBytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulp_header_size =
      l_bit ? kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;

  for (int i = 0; i < num_fec_packets; ++i) {
    // -- FEC header --
    generated_fec_packets_[i].data[0] &= 0x7f;  // Set E to zero.
    if (l_bit == 0) {
      generated_fec_packets_[i].data[0] &= 0xbf;  // Clear the L bit.
    } else {
      generated_fec_packets_[i].data[0] |= 0x40;  // Set the L bit.
    }
    // Two byte sequence number from first RTP packet to SN base.
    // We use the same sequence number base for every FEC packet,
    // but that's not required in general.
    memcpy(&generated_fec_packets_[i].data[2], &media_packet->data[2], 2);

    // -- ULP header --
    // Copy the payload size to the protection length field.
    // (We protect the entire packet.)
    ModuleRTPUtility::AssignUWord16ToBuffer(
        &generated_fec_packets_[i].data[10],
        generated_fec_packets_[i].length - kFecHeaderSize - ulp_header_size);

    // Copy the packet mask.
    memcpy(&generated_fec_packets_[i].data[12], &packet_mask[i * num_maskBytes],
           num_maskBytes);
  }
}

void ForwardErrorCorrection::ResetState(
    RecoveredPacketList* recovered_packet_list) {
  fec_packet_received_ = false;

  // Free the memory for any existing recovered packets, if the user hasn't.
  while (!recovered_packet_list->empty()) {
    delete recovered_packet_list->front();
    recovered_packet_list->pop_front();
  }
  assert(recovered_packet_list->empty());

  // Free the FEC packet list.
  while (!fec_packet_list_.empty()) {
    FecPacketList::iterator fec_packet_list_it = fec_packet_list_.begin();
    FecPacket* fec_packet = *fec_packet_list_it;
    ProtectedPacketList::iterator protected_packet_list_it;
    protected_packet_list_it = fec_packet->protected_pkt_list.begin();
    while (protected_packet_list_it != fec_packet->protected_pkt_list.end()) {
      delete* protected_packet_list_it;
      protected_packet_list_it =
          fec_packet->protected_pkt_list.erase(protected_packet_list_it);
    }
    assert(fec_packet->protected_pkt_list.empty());
    delete fec_packet;
    fec_packet_list_.pop_front();
  }
  assert(fec_packet_list_.empty());
}

void ForwardErrorCorrection::InsertMediaPacket(
    ReceivedPacket* rx_packet, RecoveredPacketList* recovered_packet_list) {
  RecoveredPacketList::iterator recovered_packet_list_it =
      recovered_packet_list->begin();

  // Search for duplicate packets.
  while (recovered_packet_list_it != recovered_packet_list->end()) {
    if (rx_packet->seq_num == (*recovered_packet_list_it)->seq_num) {
      // Duplicate packet, no need to add to list.
      // Delete duplicate media packet data.
      rx_packet->pkt = NULL;
      return;
    }
    recovered_packet_list_it++;
  }
  RecoveredPacket* recoverd_packet_to_insert = new RecoveredPacket;
  recoverd_packet_to_insert->was_recovered = false;
  // Inserted Media packet is already sent to VCM.
  recoverd_packet_to_insert->returned = true;
  recoverd_packet_to_insert->seq_num = rx_packet->seq_num;
  recoverd_packet_to_insert->pkt = rx_packet->pkt;
  recoverd_packet_to_insert->pkt->length = rx_packet->pkt->length;

  // TODO(holmer): Consider replacing this with a binary search for the right
  // position, and then just insert the new packet. Would get rid of the sort.
  recovered_packet_list->push_back(recoverd_packet_to_insert);
  recovered_packet_list->sort(SortablePacket::LessThan);
  UpdateCoveringFECPackets(recoverd_packet_to_insert);
}

void ForwardErrorCorrection::UpdateCoveringFECPackets(RecoveredPacket* packet) {
  for (FecPacketList::iterator it = fec_packet_list_.begin();
       it != fec_packet_list_.end(); ++it) {
    // Is this FEC packet protecting the media packet |packet|?
    ProtectedPacketList::iterator protected_it = std::lower_bound(
        (*it)->protected_pkt_list.begin(), (*it)->protected_pkt_list.end(),
        packet, SortablePacket::LessThan);
    if (protected_it != (*it)->protected_pkt_list.end() &&
        (*protected_it)->seq_num == packet->seq_num) {
      // Found an FEC packet which is protecting |packet|.
      (*protected_it)->pkt = packet->pkt;
    }
  }
}

void ForwardErrorCorrection::InsertFECPacket(
    ReceivedPacket* rx_packet,
    const RecoveredPacketList* recovered_packet_list) {
  fec_packet_received_ = true;

  // Check for duplicate.
  FecPacketList::iterator fec_packet_list_it = fec_packet_list_.begin();
  while (fec_packet_list_it != fec_packet_list_.end()) {
    if (rx_packet->seq_num == (*fec_packet_list_it)->seq_num) {
      // Delete duplicate FEC packet data.
      rx_packet->pkt = NULL;
      return;
    }
    fec_packet_list_it++;
  }
  FecPacket* fec_packet = new FecPacket;
  fec_packet->pkt = rx_packet->pkt;
  fec_packet->seq_num = rx_packet->seq_num;
  fec_packet->ssrc = rx_packet->ssrc;

  const uint16_t seq_num_base =
      ModuleRTPUtility::BufferToUWord16(&fec_packet->pkt->data[2]);
  const uint16_t maskSizeBytes =
      (fec_packet->pkt->data[0] & 0x40) ? kMaskSizeLBitSet
                                        : kMaskSizeLBitClear;  // L bit set?

  for (uint16_t byte_idx = 0; byte_idx < maskSizeBytes; ++byte_idx) {
    uint8_t packet_mask = fec_packet->pkt->data[12 + byte_idx];
    for (uint16_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
      if (packet_mask & (1 << (7 - bit_idx))) {
        ProtectedPacket* protected_packet = new ProtectedPacket;
        fec_packet->protected_pkt_list.push_back(protected_packet);
        // This wraps naturally with the sequence number.
        protected_packet->seq_num =
            static_cast<uint16_t>(seq_num_base + (byte_idx << 3) + bit_idx);
        protected_packet->pkt = NULL;
      }
    }
  }
  if (fec_packet->protected_pkt_list.empty()) {
    // All-zero packet mask; we can discard this FEC packet.
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, id_,
                 "FEC packet %u has an all-zero packet mask.",
                 fec_packet->seq_num, __FUNCTION__);
    delete fec_packet;
  } else {
    AssignRecoveredPackets(fec_packet, recovered_packet_list);
    // TODO(holmer): Consider replacing this with a binary search for the right
    // position, and then just insert the new packet. Would get rid of the sort.
    fec_packet_list_.push_back(fec_packet);
    fec_packet_list_.sort(SortablePacket::LessThan);
    if (fec_packet_list_.size() > kMaxFecPackets) {
      DiscardFECPacket(fec_packet_list_.front());
      fec_packet_list_.pop_front();
    }
    assert(fec_packet_list_.size() <= kMaxFecPackets);
  }
}

void ForwardErrorCorrection::AssignRecoveredPackets(
    FecPacket* fec_packet, const RecoveredPacketList* recovered_packets) {
  // Search for missing packets which have arrived or have been recovered by
  // another FEC packet.
  ProtectedPacketList* not_recovered = &fec_packet->protected_pkt_list;
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
    while ((*not_recovered_it)->seq_num != (*it)->seq_num)
      ++not_recovered_it;
    (*not_recovered_it)->pkt = (*it)->pkt;
  }
}

void ForwardErrorCorrection::InsertPackets(
    ReceivedPacketList* received_packet_list,
    RecoveredPacketList* recovered_packet_list) {

  while (!received_packet_list->empty()) {
    ReceivedPacket* rx_packet = received_packet_list->front();

    if (rx_packet->is_fec) {
      InsertFECPacket(rx_packet, recovered_packet_list);
    } else {
      // Insert packet at the end of |recoveredPacketList|.
      InsertMediaPacket(rx_packet, recovered_packet_list);
    }
    // Delete the received packet "wrapper", but not the packet data.
    delete rx_packet;
    received_packet_list->pop_front();
  }
  assert(received_packet_list->empty());
  DiscardOldPackets(recovered_packet_list);
}

void ForwardErrorCorrection::InitRecovery(const FecPacket* fec_packet,
                                          RecoveredPacket* recovered) {
  // This is the first packet which we try to recover with.
  const uint16_t ulp_header_size =
      fec_packet->pkt->data[0] & 0x40 ? kUlpHeaderSizeLBitSet
                                      : kUlpHeaderSizeLBitClear;  // L bit set?
  recovered->pkt = new Packet;
  memset(recovered->pkt->data, 0, IP_PACKET_SIZE);
  recovered->returned = false;
  recovered->was_recovered = true;
  uint8_t protection_length[2];
  // Copy the protection length from the ULP header.
  memcpy(protection_length, &fec_packet->pkt->data[10], 2);
  // Copy FEC payload, skipping the ULP header.
  memcpy(&recovered->pkt->data[kRtpHeaderSize],
         &fec_packet->pkt->data[kFecHeaderSize + ulp_header_size],
         ModuleRTPUtility::BufferToUWord16(protection_length));
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
                                          recovered->seq_num);
  // Recover the packet length.
  recovered->pkt->length =
      ModuleRTPUtility::BufferToUWord16(recovered->length_recovery) +
      kRtpHeaderSize;
}

void ForwardErrorCorrection::XorPackets(const Packet* src_packet,
                                        RecoveredPacket* dst_packet) {
  // XOR with the first 2 bytes of the RTP header.
  for (uint32_t i = 0; i < 2; ++i) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
  // XOR with the 5th to 8th bytes of the RTP header.
  for (uint32_t i = 4; i < 8; ++i) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
  // XOR with the network-ordered payload size.
  uint8_t media_payload_length[2];
  ModuleRTPUtility::AssignUWord16ToBuffer(media_payload_length,
                                          src_packet->length - kRtpHeaderSize);
  dst_packet->length_recovery[0] ^= media_payload_length[0];
  dst_packet->length_recovery[1] ^= media_payload_length[1];

  // XOR with RTP payload.
  // TODO(marpan/ajm): Are we doing more XORs than required here?
  for (int32_t i = kRtpHeaderSize; i < src_packet->length; ++i) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
}

void ForwardErrorCorrection::RecoverPacket(
    const FecPacket* fec_packet, RecoveredPacket* rec_packet_to_insert) {
  InitRecovery(fec_packet, rec_packet_to_insert);
  ProtectedPacketList::const_iterator protected_it =
      fec_packet->protected_pkt_list.begin();
  while (protected_it != fec_packet->protected_pkt_list.end()) {
    if ((*protected_it)->pkt == NULL) {
      // This is the packet we're recovering.
      rec_packet_to_insert->seq_num = (*protected_it)->seq_num;
    } else {
      XorPackets((*protected_it)->pkt, rec_packet_to_insert);
    }
    ++protected_it;
  }
  FinishRecovery(rec_packet_to_insert);
}

void ForwardErrorCorrection::AttemptRecover(
    RecoveredPacketList* recovered_packet_list) {
  FecPacketList::iterator fec_packet_list_it = fec_packet_list_.begin();
  while (fec_packet_list_it != fec_packet_list_.end()) {
    // Search for each FEC packet's protected media packets.
    int packets_missing = NumCoveredPacketsMissing(*fec_packet_list_it);

    // We can only recover one packet with an FEC packet.
    if (packets_missing == 1) {
      // Recovery possible.
      RecoveredPacket* packet_to_insert = new RecoveredPacket;
      packet_to_insert->pkt = NULL;
      RecoverPacket(*fec_packet_list_it, packet_to_insert);

      // Add recovered packet to the list of recovered packets and update any
      // FEC packets covering this packet with a pointer to the data.
      // TODO(holmer): Consider replacing this with a binary search for the
      // right position, and then just insert the new packet. Would get rid of
      // the sort.
      recovered_packet_list->push_back(packet_to_insert);
      recovered_packet_list->sort(SortablePacket::LessThan);
      UpdateCoveringFECPackets(packet_to_insert);
      DiscardOldPackets(recovered_packet_list);
      DiscardFECPacket(*fec_packet_list_it);
      fec_packet_list_it = fec_packet_list_.erase(fec_packet_list_it);

      // A packet has been recovered. We need to check the FEC list again, as
      // this may allow additional packets to be recovered.
      // Restart for first FEC packet.
      fec_packet_list_it = fec_packet_list_.begin();
    } else if (packets_missing == 0) {
      // Either all protected packets arrived or have been recovered. We can
      // discard this FEC packet.
      DiscardFECPacket(*fec_packet_list_it);
      fec_packet_list_it = fec_packet_list_.erase(fec_packet_list_it);
    } else {
      fec_packet_list_it++;
    }
  }
}

int ForwardErrorCorrection::NumCoveredPacketsMissing(
    const FecPacket* fec_packet) {
  int packets_missing = 0;
  ProtectedPacketList::const_iterator it =
      fec_packet->protected_pkt_list.begin();
  for (; it != fec_packet->protected_pkt_list.end(); ++it) {
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
  while (!fec_packet->protected_pkt_list.empty()) {
    delete fec_packet->protected_pkt_list.front();
    fec_packet->protected_pkt_list.pop_front();
  }
  assert(fec_packet->protected_pkt_list.empty());
  delete fec_packet;
}

void ForwardErrorCorrection::DiscardOldPackets(
    RecoveredPacketList* recovered_packet_list) {
  while (recovered_packet_list->size() > kMaxMediaPackets) {
    ForwardErrorCorrection::RecoveredPacket* packet =
        recovered_packet_list->front();
    delete packet;
    recovered_packet_list->pop_front();
  }
  assert(recovered_packet_list->size() <= kMaxMediaPackets);
}

uint16_t ForwardErrorCorrection::ParseSequenceNumber(uint8_t* packet) {
  return (packet[2] << 8) + packet[3];
}

int32_t ForwardErrorCorrection::DecodeFEC(
    ReceivedPacketList* received_packet_list,
    RecoveredPacketList* recovered_packet_list) {
  // TODO(marpan/ajm): can we check for multiple ULP headers, and return an
  // error?
  if (recovered_packet_list->size() == kMaxMediaPackets) {
    const unsigned int seq_num_diff =
        abs(static_cast<int>(received_packet_list->front()->seq_num) -
            static_cast<int>(recovered_packet_list->back()->seq_num));
    if (seq_num_diff > kMaxMediaPackets) {
      // A big gap in sequence numbers. The old recovered packets
      // are now useless, so it's safe to do a reset.
      ResetState(recovered_packet_list);
    }
  }
  InsertPackets(received_packet_list, recovered_packet_list);
  AttemptRecover(recovered_packet_list);
  return 0;
}

uint16_t ForwardErrorCorrection::PacketOverhead() {
  return kFecHeaderSize + kUlpHeaderSizeLBitSet;
}
}  // namespace webrtc
