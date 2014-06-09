/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_

#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Packet mask size in bytes (L bit is set).
static const int kMaskSizeLBitSet = 6;
// Packet mask size in bytes (L bit is cleared).
static const int kMaskSizeLBitClear = 2;

namespace internal {

class PacketMaskTable {
 public:
  PacketMaskTable(FecMaskType fec_mask_type, int num_media_packets);
  ~PacketMaskTable() {}
  FecMaskType fec_mask_type() const { return fec_mask_type_; }
  const uint8_t*** fec_packet_mask_table() const {
    return fec_packet_mask_table_;
  }

 private:
  FecMaskType InitMaskType(FecMaskType fec_mask_type, int num_media_packets);
  const uint8_t*** InitMaskTable(FecMaskType fec_mask_type_);
  const FecMaskType fec_mask_type_;
  const uint8_t*** fec_packet_mask_table_;
};

// Returns an array of packet masks. The mask of a single FEC packet
// corresponds to a number of mask bytes. The mask indicates which
// media packets should be protected by the FEC packet.

// \param[in]  num_media_packets       The number of media packets to protect.
//                                     [1, max_media_packets].
// \param[in]  num_fec_packets         The number of FEC packets which will
//                                     be generated. [1, num_media_packets].
// \param[in]  num_imp_packets         The number of important packets.
//                                     [0, num_media_packets].
//                                     num_imp_packets = 0 is the equal
//                                     protection scenario.
// \param[in]  use_unequal_protection  Enables unequal protection: allocates
//                                     more protection to the num_imp_packets.
// \param[in]  mask_table              An instance of the |PacketMaskTable|
//                                     class, which contains the type of FEC
//                                     packet mask used, and a pointer to the
//                                     corresponding packet masks.
// \param[out] packet_mask             A pointer to hold the packet mask array,
//                                     of size: num_fec_packets *
//                                     "number of mask bytes".
void GeneratePacketMasks(int num_media_packets, int num_fec_packets,
                         int num_imp_packets, bool use_unequal_protection,
                         const PacketMaskTable& mask_table,
                         uint8_t* packet_mask);

}  // namespace internal
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_INTERNAL_H_
