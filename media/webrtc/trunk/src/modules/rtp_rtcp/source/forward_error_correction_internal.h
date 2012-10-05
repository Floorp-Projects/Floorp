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
#include "typedefs.h"

namespace webrtc {

// Packet mask size in bytes (L bit is set).
static const int kMaskSizeLBitSet = 6;
// Packet mask size in bytes (L bit is cleared).
static const int kMaskSizeLBitClear = 2;

namespace internal {

class PacketMaskTable {
 public:
  PacketMaskTable(FecMaskType fec_mask_type, int num_media_packets);
  ~PacketMaskTable() {
  }
  FecMaskType fec_mask_type() const { return fec_mask_type_; }
  const uint8_t*** fec_packet_mask_table() const {
    return fec_packet_mask_table_;
  }
 private:
  FecMaskType InitMaskType(FecMaskType fec_mask_type,
                           int num_media_packets);
  const uint8_t*** InitMaskTable(FecMaskType fec_mask_type_);
  const FecMaskType fec_mask_type_;
  const uint8_t*** fec_packet_mask_table_;
};

 /**
  * Returns an array of packet masks. The mask of a single FEC packet
  * corresponds to a number of mask bytes. The mask indicates which
  * media packets should be protected by the FEC packet.

  * \param[in]  numMediaPackets       The number of media packets to protect.
  *                                    [1, maxMediaPackets].
  * \param[in]  numFecPackets         The number of FEC packets which will
  *                                    be generated. [1, numMediaPackets].
  * \param[in]  numImpPackets         The number of important packets.
  *                                    [0, numMediaPackets].
  *                                   numImpPackets = 0 is the equal
  *                                    protection scenario.
  * \param[in]  useUnequalProtection  Enables unequal protection: allocates
  *                                    more protection to the numImpPackets.
  * \param[in]  mask_table            An instance of the |PacketMaskTable|
  *                                    class, which contains the type of FEC
  *                                    packet mask used, and a pointer to the
  *                                    corresponding packet masks.
  * \param[out] packetMask            A pointer to hold the packet mask array,
  *                                    of size:
  *                                    numFecPackets * "number of mask bytes".
  */
void GeneratePacketMasks(int numMediaPackets,
                         int numFecPackets,
                         int numImpPackets,
                         bool useUnequalProtection,
                         const PacketMaskTable& mask_table,
                         uint8_t* packetMask);

} // namespace internal
} // namespace webrtc
