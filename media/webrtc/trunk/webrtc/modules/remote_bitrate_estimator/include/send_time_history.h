/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_SEND_TIME_HISTORY_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_SEND_TIME_HISTORY_H_

#include <map>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

namespace webrtc {

class SendTimeHistory {
 public:
  SendTimeHistory(Clock* clock, int64_t packet_age_limit);
  virtual ~SendTimeHistory();

  void AddAndRemoveOld(uint16_t sequence_number, size_t length, bool was_paced);
  bool OnSentPacket(uint16_t sequence_number, int64_t timestamp);
  // Look up PacketInfo for a sent packet, based on the sequence number, and
  // populate all fields except for receive_time. The packet parameter must
  // thus be non-null and have the sequence_number field set.
  bool GetInfo(PacketInfo* packet, bool remove);
  void Clear();

 private:
  void EraseOld();
  void UpdateOldestSequenceNumber();

  Clock* const clock_;
  const int64_t packet_age_limit_;
  uint16_t oldest_sequence_number_;  // Oldest may not be lowest.
  std::map<uint16_t, PacketInfo> history_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SendTimeHistory);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_INCLUDE_SEND_TIME_HISTORY_H_
