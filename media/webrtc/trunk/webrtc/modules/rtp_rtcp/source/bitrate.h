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

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpRtcpClock;

class Bitrate {
 public:
  explicit Bitrate(RtpRtcpClock* clock);

  // Calculates rates.
  void Process();

  // Update with a packet.
  void Update(const WebRtc_Word32 bytes);

  // Packet rate last second, updated roughly every 100 ms.
  WebRtc_UWord32 PacketRate() const;

  // Bitrate last second, updated roughly every 100 ms.
  WebRtc_UWord32 BitrateLast() const;

  // Bitrate last second, updated now.
  WebRtc_UWord32 BitrateNow() const;

 protected:
  RtpRtcpClock& clock_;

 private:
  WebRtc_UWord32 packet_rate_;
  WebRtc_UWord32 bitrate_;
  WebRtc_UWord8 bitrate_next_idx_;
  WebRtc_Word64 packet_rate_array_[10];
  WebRtc_Word64 bitrate_array_[10];
  WebRtc_Word64 bitrate_diff_ms_[10];
  WebRtc_Word64 time_last_rate_update_;
  WebRtc_UWord32 bytes_count_;
  WebRtc_UWord32 packet_count_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_BITRATE_H_
