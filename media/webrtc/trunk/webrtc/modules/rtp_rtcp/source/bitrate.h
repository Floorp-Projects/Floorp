/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_BITRATE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_BITRATE_H_

#include <stdio.h>

#include <list>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;

class Bitrate {
 public:
  class Observer;
  Bitrate(Clock* clock, Observer* observer);
  virtual ~Bitrate();

  // Calculates rates.
  void Process();

  // Update with a packet.
  void Update(const size_t bytes);

  // Packet rate last second, updated roughly every 100 ms.
  uint32_t PacketRate() const;

  // Bitrate last second, updated roughly every 100 ms.
  uint32_t BitrateLast() const;

  // Bitrate last second, updated now.
  uint32_t BitrateNow() const;

  int64_t time_last_rate_update() const;

  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void BitrateUpdated(const BitrateStatistics& stats) = 0;
  };

 protected:
  Clock* clock_;

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> crit_;
  uint32_t packet_rate_;
  uint32_t bitrate_;
  uint8_t bitrate_next_idx_;
  int64_t packet_rate_array_[10];
  int64_t bitrate_array_[10];
  int64_t bitrate_diff_ms_[10];
  int64_t time_last_rate_update_;
  size_t bytes_count_;
  uint32_t packet_count_;
  Observer* const observer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_BITRATE_H_
