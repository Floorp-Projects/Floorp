/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_TIME_HISTORY_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_TIME_HISTORY_H_

#include <map>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/basictypes.h"

namespace webrtc {

class SendTimeHistory {
 public:
  explicit SendTimeHistory(int64_t packet_age_limit);
  virtual ~SendTimeHistory();

  void AddAndRemoveOldSendTimes(uint16_t sequence_number, int64_t timestamp);
  bool GetSendTime(uint16_t sequence_number, int64_t* timestamp, bool remove);
  void Clear();

 private:
  void EraseOld(int64_t limit);
  void UpdateOldestSequenceNumber();

  const int64_t packet_age_limit_;
  uint16_t oldest_sequence_number_;  // Oldest may not be lowest.
  std::map<uint16_t, int64_t> history_;

  DISALLOW_COPY_AND_ASSIGN(SendTimeHistory);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_TIME_HISTORY_H_
