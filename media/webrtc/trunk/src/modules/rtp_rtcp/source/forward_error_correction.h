/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_

#include <list>
#include <vector>

#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "system_wrappers/interface/ref_count.h"
#include "system_wrappers/interface/scoped_refptr.h"
#include "typedefs.h"

namespace webrtc {

// Forward declaration.
class FecPacket;

/**
 * Performs codec-independent forward error correction (FEC), based on RFC 5109.
 * Option exists to enable unequal protection (UEP) across packets.
 * This is not to be confused with protection within packets
 * (referred to as uneven level protection (ULP) in RFC 5109).
 */
class ForwardErrorCorrection {
 public:
  // Maximum number of media packets we can protect
  static const unsigned int kMaxMediaPackets = 48u;

  // TODO(holmer): As a next step all these struct-like packet classes should be
  // refactored into proper classes, and their members should be made private.
  // This will require parts of the functionality in forward_error_correction.cc
  // and receiver_fec.cc to be refactored into the packet classes.
  class Packet {
   public:
    Packet() : length(0), data(), ref_count_(0) {}
    virtual ~Packet() {}

    // Add a reference.
    virtual int32_t AddRef() {
      return ++ref_count_;
    }

    // Release a reference. Will delete the object if the reference count
    // reaches zero.
    virtual int32_t Release() {
      int32_t ref_count;
      ref_count = --ref_count_;
      if (ref_count == 0)
        delete this;
      return ref_count;
    }

    uint16_t length;  // Length of packet in bytes.
    uint8_t data[IP_PACKET_SIZE];  // Packet data.

   private:
    int32_t ref_count_;  // Counts the number of references to a packet.
  };

  // TODO(holmer): Refactor into a proper class.
  class SortablePacket {
   public:
    // True if first is <= than second.
    static bool LessThan(const SortablePacket* first,
                         const SortablePacket* second);

    uint16_t seqNum;
  };

  /**
   * The received list parameter of #DecodeFEC() must reference structs of this
   * type. The lastMediaPktInFrame is not required to be used for correct
   * recovery, but will reduce delay by allowing #DecodeFEC() to pre-emptively
   * determine frame completion. If set, we assume a FEC stream, and the
   * following assumptions must hold:\n
   *
   * 1. The media packets in a frame have contiguous sequence numbers, i.e. the
   *    frame's FEC packets have sequence numbers either lower than the first
   *    media packet or higher than the last media packet.\n
   * 2. All FEC packets have a sequence number base equal to the first media
   *    packet in the corresponding frame.\n
   *
   * The ssrc member is needed to ensure we can restore the SSRC field of
   * recovered packets. In most situations this could be retrieved from other
   * media packets, but in the case of an FEC packet protecting a single
   * missing media packet, we have no other means of obtaining it.
   */
  // TODO(holmer): Refactor into a proper class.
  class ReceivedPacket : public SortablePacket {
   public:
    uint32_t ssrc;  // SSRC of the current frame. Must be set for FEC
                    // packets, but not required for media packets.
    bool isFec;  // Set to true if this is an FEC packet and false
                 // otherwise.
    scoped_refptr<Packet> pkt;  // Pointer to the packet storage.
  };

  /**
   * The recovered list parameter of #DecodeFEC() will reference structs of
   * this type.
   */
  // TODO(holmer): Refactor into a proper class.
  class RecoveredPacket : public SortablePacket {
   public:
    bool wasRecovered;  // Will be true if this packet was recovered by
                        // the FEC. Otherwise it was a media packet passed in
                        // through the received packet list.
    bool returned;  // True when the packet already has been returned to the
                    // caller through the callback.
    uint8_t length_recovery[2];  // Two bytes used for recovering the packet
                                 // length with XOR operations.
    scoped_refptr<Packet> pkt;  // Pointer to the packet storage.
  };

  typedef std::list<Packet*> PacketList;
  typedef std::list<ReceivedPacket*> ReceivedPacketList;
  typedef std::list<RecoveredPacket*> RecoveredPacketList;

  /**
   * \param[in] id Module ID
   */
  ForwardErrorCorrection(int32_t id);

  virtual ~ForwardErrorCorrection();

  /**
   * Generates a list of FEC packets from supplied media packets.
   *
   * \param[in]  mediaPacketList     List of media packets to protect, of type
   *                                 #Packet. All packets must belong to the
   *                                 same frame and the list must not be empty.
   * \param[in]  protectionFactor    FEC protection overhead in the [0, 255]
   *                                 domain. To obtain 100% overhead, or an
   *                                 equal number of FEC packets as media
   *                                 packets, use 255.
   * \param[in] numImportantPackets  The number of "important" packets in the
   *                                 frame. These packets may receive greater
   *                                 protection than the remaining packets. The
   *                                 important packets must be located at the
   *                                 start of the media packet list. For codecs
   *                                 with data partitioning, the important
   *                                 packets may correspond to first partition
   *                                 packets.
   * \param[in] useUnequalProtection Parameter to enable/disable unequal
   *                                 protection  (UEP) across packets. Enabling
   *                                 UEP will allocate more protection to the
   *                                 numImportantPackets from the start of the
   *                                 mediaPacketList.
   * \param[out] fecPacketList       List of FEC packets, of type #Packet. Must
   *                                 be empty on entry. The memory available
   *                                 through the list will be valid until the
   *                                 next call to GenerateFEC().
   *
   * \return 0 on success, -1 on failure.
   */
  int32_t GenerateFEC(const PacketList& mediaPacketList,
                      uint8_t protectionFactor,
                      int numImportantPackets,
                      bool useUnequalProtection,
                      PacketList* fecPacketList);

  /**
   *  Decodes a list of media and FEC packets. It will parse the input received
   *  packet list, storing FEC packets internally and inserting media packets to
   *  the output recovered packet list. The recovered list will be sorted by
   *  ascending sequence number and have duplicates removed. The function
   *  should be called as new packets arrive, with the recovered list being
   *  progressively assembled with each call. The received packet list will be
   *  empty at output.\n
   *
   *  The user will allocate packets submitted through the received list. The
   *  function will handle allocation of recovered packets and optionally
   *  deleting of all packet memory. The user may delete the recovered list
   *  packets, in which case they must remove deleted packets from the
   *  recovered list.\n
   *
   * \param[in]  receivedPacketList  List of new received packets, of type
   *                                 #ReceivedPacket, belonging to a single
   *                                 frame. At output the list will be empty,
   *                                 with packets  either stored internally,
   *                                 or accessible through the recovered list.
   * \param[out] recoveredPacketList List of recovered media packets, of type
   *                                 #RecoveredPacket, belonging to a single
   *                                 frame. The memory available through the
   *                                 list will be valid until the next call to
   *                                 DecodeFEC().
   *
   * \return 0 on success, -1 on failure.
   */
  int32_t DecodeFEC(ReceivedPacketList* receivedPacketList,
                    RecoveredPacketList* recoveredPacketList);
  /**
   * Gets the size in bytes of the FEC/ULP headers, which must be accounted for
   * as packet overhead.
   * \return Packet overhead in bytes.
   */
  static uint16_t PacketOverhead();

  // Reset internal states from last frame and clear the recoveredPacketList.
  // Frees all memory allocated by this class.
  void ResetState(RecoveredPacketList* recoveredPacketList);

 private:
  typedef std::list<FecPacket*> FecPacketList;

  void GenerateFecUlpHeaders(const PacketList& mediaPacketList,
                             uint8_t* packetMask,
                             bool lBit,
                             uint32_t numFecPackets);

  // Analyzes |media_packets| for holes in the sequence and inserts zero columns
  // into the |packet_mask| where those holes are found. Zero columns means that
  // those packets will have no protection.
  // Returns the number of bits used for one row of the new packet mask.
  // Requires that |packet_mask| has at least 6 * |num_fec_packets| bytes
  // allocated.
  int InsertZerosInBitMasks(const PacketList& media_packets,
                            uint8_t* packet_mask,
                            uint16_t num_mask_bytes,
                            uint32_t num_fec_packets);

  // Inserts |num_zeros| zero columns into |new_mask| at position
  // |new_bit_index|. If the current byte of |new_mask| can't fit all zeros, the
  // byte will be filled with zeros from |new_bit_index|, but the next byte will
  // be untouched.
  static void InsertZeroColumns(int num_zeros,
                                uint8_t* new_mask,
                                int new_mask_bytes,
                                int num_fec_packets,
                                int new_bit_index);

  // Copies the left most bit column from the byte pointed to by
  // |old_bit_index| in |old_mask| to the right most column of the byte pointed
  // to by |new_bit_index| in |new_mask|. |old_mask_bytes| and |new_mask_bytes|
  // represent the number of bytes used per row for each mask. |num_fec_packets|
  // represent the number of rows of the masks.
  // The copied bit is shifted out from |old_mask| and is shifted one step to
  // the left in |new_mask|. |new_mask| will contain "xxxx xxn0" after this
  // operation, where x are previously inserted bits and n is the new bit.
  static void CopyColumn(uint8_t* new_mask,
                         int new_mask_bytes,
                         uint8_t* old_mask,
                         int old_mask_bytes,
                         int num_fec_packets,
                         int new_bit_index,
                         int old_bit_index);

  void GenerateFecBitStrings(const PacketList& mediaPacketList,
                             uint8_t* packetMask,
                             uint32_t numFecPackets,
                             bool lBit);

  // Insert received packets into FEC or recovered list.
  void InsertPackets(ReceivedPacketList* receivedPacketList,
                     RecoveredPacketList* recoveredPacketList);

  // Insert media packet into recovered packet list. We delete duplicates.
  void InsertMediaPacket(ReceivedPacket* rxPacket,
                         RecoveredPacketList* recoveredPacketList);

  // Assigns pointers to the recovered packet from all FEC packets which cover
  // it.
  // Note: This reduces the complexity when we want to try to recover a packet
  // since we don't have to find the intersection between recovered packets and
  // packets covered by the FEC packet.
  void UpdateCoveringFECPackets(RecoveredPacket* packet);

  // Insert packet into FEC list. We delete duplicates.
  void InsertFECPacket(ReceivedPacket* rxPacket,
                       const RecoveredPacketList* recoveredPacketList);

  // Assigns pointers to already recovered packets covered by this FEC packet.
  static void AssignRecoveredPackets(
      FecPacket* fec_packet,
      const RecoveredPacketList* recovered_packets);

  // Insert into recovered list in correct position.
  void InsertRecoveredPacket(
      RecoveredPacket* recPacketToInsert,
      RecoveredPacketList* recoveredPacketList);

  // Attempt to recover missing packets.
  void AttemptRecover(RecoveredPacketList* recoveredPacketList);

  // Initializes the packet recovery using the FEC packet.
  static  void InitRecovery(const FecPacket* fec_packet,
                            RecoveredPacket* recovered);

  // Performs XOR between |src_packet| and |dst_packet| and stores the result
  // in |dst_packet|.
  static void XorPackets(const Packet* src_packet,
                         RecoveredPacket* dst_packet);

  // Finish up the recovery of a packet.
  static  void FinishRecovery(RecoveredPacket* recovered);

  // Recover a missing packet.
  void RecoverPacket(const FecPacket* fecPacket,
                     RecoveredPacket* recPacketToInsert);

  // Get the number of missing media packets which are covered by this
  // FEC packet. An FEC packet can recover at most one packet, and if zero
  // packets are missing the FEC packet can be discarded.
  // This function returns 2 when two or more packets are missing.
  static int NumCoveredPacketsMissing(const FecPacket* fec_packet);

  static uint16_t LatestSequenceNumber(uint16_t first,
                                       uint16_t second);

  static void DiscardFECPacket(FecPacket* fec_packet);
  static void DiscardOldPackets(RecoveredPacketList* recoveredPacketList);
  static uint16_t ParseSequenceNumber(uint8_t* packet);

  int32_t _id;
  std::vector<Packet> _generatedFecPackets;
  FecPacketList _fecPacketList;
  bool _fecPacketReceived;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
