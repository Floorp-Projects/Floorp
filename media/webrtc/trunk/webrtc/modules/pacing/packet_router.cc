/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/include/packet_router.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

PacketRouter::PacketRouter()
    : crit_(CriticalSectionWrapper::CreateCriticalSection()) {
}

PacketRouter::~PacketRouter() {
}

void PacketRouter::AddRtpModule(RtpRtcp* rtp_module) {
  CriticalSectionScoped cs(crit_.get());
  DCHECK(std::find(rtp_modules_.begin(), rtp_modules_.end(), rtp_module) ==
         rtp_modules_.end());
  rtp_modules_.push_back(rtp_module);
}

void PacketRouter::RemoveRtpModule(RtpRtcp* rtp_module) {
  CriticalSectionScoped cs(crit_.get());
  rtp_modules_.remove(rtp_module);
}

bool PacketRouter::TimeToSendPacket(uint32_t ssrc,
                                    uint16_t sequence_number,
                                    int64_t capture_timestamp,
                                    bool retransmission) {
  CriticalSectionScoped cs(crit_.get());
  for (auto* rtp_module : rtp_modules_) {
    if (rtp_module->SendingMedia() && ssrc == rtp_module->SSRC()) {
      return rtp_module->TimeToSendPacket(ssrc, sequence_number,
                                          capture_timestamp, retransmission);
    }
  }
  return true;
}

size_t PacketRouter::TimeToSendPadding(size_t bytes) {
  CriticalSectionScoped cs(crit_.get());
  for (auto* rtp_module : rtp_modules_) {
    if (rtp_module->SendingMedia())
      return rtp_module->TimeToSendPadding(bytes);
  }
  return 0;
}
}  // namespace webrtc
