/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_remb.h"

#include <algorithm>
#include <cassert>

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/utility/interface/process_thread.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

const int kRembSendIntervallMs = 1000;
const int kRembTimeOutThresholdMs = 2000;
const unsigned int kRembMinimumBitrateKbps = 50;

// % threshold for if we should send a new REMB asap.
const unsigned int kSendThresholdPercent = 97;

VieRemb::VieRemb(ProcessThread* process_thread)
    : process_thread_(process_thread),
      list_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      last_remb_time_(TickTime::MillisecondTimestamp()),
      last_send_bitrate_(0) {
  process_thread->RegisterModule(this);
}

VieRemb::~VieRemb() {
  process_thread_->DeRegisterModule(this);
}

void VieRemb::AddReceiveChannel(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, -1,
               "VieRemb::AddReceiveChannel(%p)", rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  if (std::find(receive_modules_.begin(), receive_modules_.end(), rtp_rtcp) !=
      receive_modules_.end())
    return;

  WEBRTC_TRACE(kTraceInfo, kTraceVideo, -1, "AddRembChannel");
  // The module probably doesn't have a remote SSRC yet, so don't add it to the
  // map.
  receive_modules_.push_back(rtp_rtcp);
}

void VieRemb::RemoveReceiveChannel(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, -1,
               "VieRemb::RemoveReceiveChannel(%p)", rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  unsigned int ssrc = rtp_rtcp->RemoteSSRC();
  for (RtpModules::iterator it = receive_modules_.begin();
       it != receive_modules_.end(); ++it) {
    if ((*it) == rtp_rtcp) {
      receive_modules_.erase(it);
      break;
    }
  }
  update_time_bitrates_.erase(ssrc);
}

void VieRemb::AddRembSender(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, -1,
               "VieRemb::AddRembSender(%p)", rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());

  // Verify this module hasn't been added earlier.
  if (std::find(rtcp_sender_.begin(), rtcp_sender_.end(), rtp_rtcp) !=
      rtcp_sender_.end())
    return;
  rtcp_sender_.push_back(rtp_rtcp);
}

void VieRemb::RemoveRembSender(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);
  WEBRTC_TRACE(kTraceStateInfo, kTraceVideo, -1,
               "VieRemb::RemoveRembSender(%p)", rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  for (RtpModules::iterator it = rtcp_sender_.begin();
       it != rtcp_sender_.end(); ++it) {
    if ((*it) == rtp_rtcp) {
      rtcp_sender_.erase(it);
      return;
    }
  }
}

bool VieRemb::InUse() const {
  CriticalSectionScoped cs(list_crit_.get());
  if (receive_modules_.empty() && rtcp_sender_.empty())
    return false;
  else
    return true;
}

void VieRemb::OnReceiveBitrateChanged(unsigned int ssrc, unsigned int bitrate) {
  WEBRTC_TRACE(kTraceStream, kTraceVideo, -1,
               "VieRemb::UpdateBitrateEstimate(ssrc: %u, bitrate: %u)",
               ssrc, bitrate);
  CriticalSectionScoped cs(list_crit_.get());

  // Check if this is a new ssrc and add it to the map if it is.
  if (update_time_bitrates_.find(ssrc) == update_time_bitrates_.end()) {
    update_time_bitrates_[ssrc] = std::make_pair(
        TickTime::MillisecondTimestamp(), bitrate);
  }

  // If we already have an estimate, check if the new total estimate is below
  // kSendThresholdPercent of the previous estimate.
  if (last_send_bitrate_ > 0) {
    unsigned int new_remb_bitrate = last_send_bitrate_ -
        update_time_bitrates_[ssrc].second + bitrate;

    if (new_remb_bitrate < kSendThresholdPercent * last_send_bitrate_ / 100) {
      // The new bitrate estimate is less than kSendThresholdPercent % of the
      // last report. Send a REMB asap.
      last_remb_time_ = TickTime::MillisecondTimestamp() - kRembSendIntervallMs;
    }
  }
  update_time_bitrates_[ssrc] = std::make_pair(
      TickTime::MillisecondTimestamp(), bitrate);
}

WebRtc_Word32 VieRemb::ChangeUniqueId(const WebRtc_Word32 id) {
  return 0;
}

WebRtc_Word32 VieRemb::TimeUntilNextProcess() {
  return kRembSendIntervallMs -
      (TickTime::MillisecondTimestamp() - last_remb_time_);
}

WebRtc_Word32 VieRemb::Process() {
  int64_t now = TickTime::MillisecondTimestamp();
  if (now - last_remb_time_ < kRembSendIntervallMs)
    return 0;

  last_remb_time_ = now;

  // Calculate total receive bitrate estimate.
  list_crit_->Enter();

  // Remove any timed out estimates.
  SsrcTimeBitrate::iterator it = update_time_bitrates_.begin();
  while (it != update_time_bitrates_.end()) {
    if (TickTime::MillisecondTimestamp() - it->second.first >
      kRembTimeOutThresholdMs) {
      update_time_bitrates_.erase(it++);
    } else {
      ++it;
    }
  }

  int num_bitrates = update_time_bitrates_.size();

  if (num_bitrates == 0) {
    list_crit_->Leave();
    return 0;
  }

  // TODO(mflodman) Use std::vector and change RTP module API.
  unsigned int* ssrcs = new unsigned int[num_bitrates];

  unsigned int total_bitrate = 0;
  int idx = 0;
  for (it = update_time_bitrates_.begin(); it != update_time_bitrates_.end();
      ++it, ++idx) {
    total_bitrate += it->second.second;
    ssrcs[idx] = it->first;
  }

  // Send a REMB packet.
  RtpRtcp* sender = NULL;
  if (!rtcp_sender_.empty()) {
    sender = rtcp_sender_.front();
  } else if (!receive_modules_.empty()) {
    sender = receive_modules_.front();
  }
  last_send_bitrate_ = total_bitrate;

  // Never send a REMB lower than last_send_bitrate_.
  if (last_send_bitrate_ < kRembMinimumBitrateKbps) {
    last_send_bitrate_ = kRembMinimumBitrateKbps;
  }
  list_crit_->Leave();

  if (sender) {
    sender->SetREMBData(total_bitrate, num_bitrates, ssrcs);
  }
  delete [] ssrcs;
  return 0;
}

}  // namespace webrtc
