/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_INCLUDE_PACKET_ROUTER_H_
#define WEBRTC_MODULES_PACING_INCLUDE_PACKET_ROUTER_H_

#include <list>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/include/paced_sender.h"

namespace webrtc {

class CriticalSectionWrapper;
class RTPFragmentationHeader;
class RtpRtcp;
struct RTPVideoHeader;

// PacketRouter routes outgoing data to the correct sending RTP module, based
// on the simulcast layer in RTPVideoHeader.
class PacketRouter : public PacedSender::Callback {
 public:
  PacketRouter();
  virtual ~PacketRouter();

  void AddRtpModule(RtpRtcp* rtp_module);
  void RemoveRtpModule(RtpRtcp* rtp_module);

  // Implements PacedSender::Callback.
  bool TimeToSendPacket(uint32_t ssrc,
                        uint16_t sequence_number,
                        int64_t capture_timestamp,
                        bool retransmission) override;

  size_t TimeToSendPadding(size_t bytes) override;

 private:
  // TODO(holmer): When the new video API has launched, remove crit_ and
  // assume rtp_modules_ will never change during a call. We should then also
  // switch rtp_modules_ to a map from ssrc to rtp module.
  rtc::scoped_ptr<CriticalSectionWrapper> crit_;

  // Map from ssrc to sending rtp module.
  std::list<RtpRtcp*> rtp_modules_ GUARDED_BY(crit_.get());

  DISALLOW_COPY_AND_ASSIGN(PacketRouter);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACING_INCLUDE_PACKET_ROUTER_H_
