/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_PACKET_DATA_H_
#define NET_DCSCTP_PACKET_DATA_H_

#include <cstdint>
#include <utility>
#include <vector>

namespace dcsctp {

// Represents data that is either received and extracted from a DATA/I-DATA
// chunk, or data that is supposed to be sent, and wrapped in a DATA/I-DATA
// chunk (depending on peer capabilities).
//
// The data wrapped in this structure is actually the same as the DATA/I-DATA
// chunk (actually the union of them), but to avoid having all components be
// aware of the implementation details of the different chunks, this abstraction
// is used instead. A notable difference is also that it doesn't carry a
// Transmission Sequence Number (TSN), as that is not known when a chunk is
// created (assigned late, just when sending), and that the TSNs in DATA/I-DATA
// are wrapped numbers, and within the library, unwrapped sequence numbers are
// preferably used.
struct Data {
  Data(uint16_t stream_id,
       uint16_t ssn,
       uint32_t message_id,
       uint32_t fsn,
       uint32_t ppid,
       std::vector<uint8_t> payload,
       bool is_beginning,
       bool is_end,
       bool is_unordered)
      : stream_id(stream_id),
        ssn(ssn),
        message_id(message_id),
        fsn(fsn),
        ppid(ppid),
        payload(std::move(payload)),
        is_beginning(is_beginning),
        is_end(is_end),
        is_unordered(is_unordered) {}

  // Move-only, to avoid accidental copies.
  Data(Data&& other) = default;
  Data& operator=(Data&& other) = default;

  // Creates a copy of this `Data` object.
  Data Clone() {
    return Data(stream_id, ssn, message_id, fsn, ppid, payload, is_beginning,
                is_end, is_unordered);
  }

  // The size of this data, which translates to the size of its payload.
  size_t size() const { return payload.size(); }

  // Stream Identifier.
  uint16_t stream_id;

  // Stream Sequence Number (SSN), per stream, for ordered chunks. Defined by
  // RFC4960 and used only in DATA chunks (not I-DATA).
  uint16_t ssn;

  // Message Identifier (MID) per stream and ordered/unordered. Defined by
  // RFC8260, and used together with options.is_unordered and stream_id to
  // uniquely identify a message. Used only in I-DATA chunks (not DATA).
  uint32_t message_id;
  // Fragment Sequence Number (FSN) per stream and ordered/unordered, as above.
  uint32_t fsn;

  // Payload Protocol Identifier (PPID).
  uint32_t ppid;

  // The actual data payload.
  std::vector<uint8_t> payload;

  // If this data represents the first, last or a middle chunk.
  bool is_beginning;
  bool is_end;
  // If this data is sent/received unordered.
  bool is_unordered;
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_PACKET_DATA_H_
