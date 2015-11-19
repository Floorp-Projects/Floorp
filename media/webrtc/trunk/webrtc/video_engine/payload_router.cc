/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/payload_router.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

PayloadRouter::PayloadRouter()
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      active_(false) {}

PayloadRouter::~PayloadRouter() {}

size_t PayloadRouter::DefaultMaxPayloadLength() {
  const size_t kIpUdpSrtpLength = 44;
  return IP_PACKET_SIZE - kIpUdpSrtpLength;
}

void PayloadRouter::SetSendingRtpModules(
    const std::list<RtpRtcp*>& rtp_modules) {
  CriticalSectionScoped cs(crit_.get());
  rtp_modules_.clear();
  rtp_modules_.reserve(rtp_modules.size());
  for (auto* rtp_module : rtp_modules) {
    rtp_modules_.push_back(rtp_module);
  }
}

void PayloadRouter::set_active(bool active) {
  CriticalSectionScoped cs(crit_.get());
  active_ = active;
}

bool PayloadRouter::active() {
  CriticalSectionScoped cs(crit_.get());
  return active_ && !rtp_modules_.empty();
}

bool PayloadRouter::RoutePayload(FrameType frame_type,
                                 int8_t payload_type,
                                 uint32_t time_stamp,
                                 int64_t capture_time_ms,
                                 const uint8_t* payload_data,
                                 size_t payload_length,
                                 const RTPFragmentationHeader* fragmentation,
                                 const RTPVideoHeader* rtp_video_hdr) {
  CriticalSectionScoped cs(crit_.get());
  if (!active_ || rtp_modules_.empty())
    return false;

  // The simulcast index might actually be larger than the number of modules in
  // case the encoder was processing a frame during a codec reconfig.
  if (rtp_video_hdr != NULL &&
      rtp_video_hdr->simulcastIdx >= rtp_modules_.size())
    return false;

  int stream_idx = 0;
  if (rtp_video_hdr != NULL)
    stream_idx = rtp_video_hdr->simulcastIdx;
  return rtp_modules_[stream_idx]->SendOutgoingData(
      frame_type, payload_type, time_stamp, capture_time_ms, payload_data,
      payload_length, fragmentation, rtp_video_hdr) == 0 ? true : false;
}

void PayloadRouter::SetTargetSendBitrates(
    const std::vector<uint32_t>& stream_bitrates) {
  CriticalSectionScoped cs(crit_.get());
  if (stream_bitrates.size() < rtp_modules_.size()) {
    // There can be a size mis-match during codec reconfiguration.
    return;
  }
  int idx = 0;
  for (auto* rtp_module : rtp_modules_) {
    rtp_module->SetTargetSendBitrate(stream_bitrates[idx++]);
  }
}

size_t PayloadRouter::MaxPayloadLength() const {
  size_t min_payload_length = DefaultMaxPayloadLength();
  CriticalSectionScoped cs(crit_.get());
  for (auto* rtp_module : rtp_modules_) {
    size_t module_payload_length = rtp_module->MaxDataPayloadLength();
    if (module_payload_length < min_payload_length)
      min_payload_length = module_payload_length;
  }
  return min_payload_length;
}

}  // namespace webrtc
