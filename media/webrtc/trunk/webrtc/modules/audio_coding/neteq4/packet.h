/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_PACKET_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_PACKET_H_

#include <list>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Struct for holding RTP packets.
struct Packet {
  RTPHeader header;
  uint8_t* payload;  // Datagram excluding RTP header and header extension.
  int payload_length;
  bool primary;  // Primary, i.e., not redundant payload.
  int waiting_time;

  // Constructor.
  Packet()
      : payload(NULL),
        payload_length(0),
        primary(true),
        waiting_time(0) {
  }

  // Comparison operators. Establish a packet ordering based on (1) timestamp,
  // (2) sequence number, and (3) redundancy. Timestamp and sequence numbers
  // are compared taking wrap-around into account. If both timestamp and
  // sequence numbers are identical, a primary payload is considered "smaller"
  // than a secondary.
  bool operator==(const Packet& rhs) const {
    return (this->header.timestamp == rhs.header.timestamp &&
        this->header.sequenceNumber == rhs.header.sequenceNumber &&
        this->primary == rhs.primary);
  }
  bool operator!=(const Packet& rhs) const { return !operator==(rhs); }
  bool operator<(const Packet& rhs) const {
    if (this->header.timestamp == rhs.header.timestamp) {
      if (this->header.sequenceNumber == rhs.header.sequenceNumber) {
        // Timestamp and sequence numbers are identical. Deem left hand side
        // to be "smaller" (i.e., "earlier") if it is primary, and right hand
        // side is not.
        return (this->primary && !rhs.primary);
      }
      return (static_cast<uint16_t>(rhs.header.sequenceNumber
          - this->header.sequenceNumber) < 0xFFFF / 2);
    }
    return (static_cast<uint32_t>(rhs.header.timestamp
        - this->header.timestamp) < 0xFFFFFFFF / 2);
  }
  bool operator>(const Packet& rhs) const { return rhs.operator<(*this); }
  bool operator<=(const Packet& rhs) const { return !operator>(rhs); }
  bool operator>=(const Packet& rhs) const { return !operator<(rhs); }
};

// A list of packets.
typedef std::list<Packet*> PacketList;

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_PACKET_H_
