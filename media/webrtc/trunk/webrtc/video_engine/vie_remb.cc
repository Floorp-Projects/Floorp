/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_remb.h"

#include <assert.h>

#include <algorithm>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

const int kRembSendIntervalMs = 200;

// % threshold for if we should send a new REMB asap.
const unsigned int kSendThresholdPercent = 97;

VieRemb::VieRemb()
    : list_crit_(CriticalSectionWrapper::CreateCriticalSection()),
      last_remb_time_(TickTime::MillisecondTimestamp()),
      last_send_bitrate_(0),
      bitrate_(0) {}

VieRemb::~VieRemb() {}

void VieRemb::AddReceiveChannel(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  if (std::find(receive_modules_.begin(), receive_modules_.end(), rtp_rtcp) !=
      receive_modules_.end())
    return;

  // The module probably doesn't have a remote SSRC yet, so don't add it to the
  // map.
  receive_modules_.push_back(rtp_rtcp);
}

void VieRemb::RemoveReceiveChannel(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());
  for (RtpModules::iterator it = receive_modules_.begin();
       it != receive_modules_.end(); ++it) {
    if ((*it) == rtp_rtcp) {
      receive_modules_.erase(it);
      break;
    }
  }
}

void VieRemb::AddRembSender(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);

  CriticalSectionScoped cs(list_crit_.get());

  // Verify this module hasn't been added earlier.
  if (std::find(rtcp_sender_.begin(), rtcp_sender_.end(), rtp_rtcp) !=
      rtcp_sender_.end())
    return;
  rtcp_sender_.push_back(rtp_rtcp);
}

void VieRemb::RemoveRembSender(RtpRtcp* rtp_rtcp) {
  assert(rtp_rtcp);

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

void VieRemb::OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                      unsigned int bitrate) {
  list_crit_->Enter();
  // If we already have an estimate, check if the new total estimate is below
  // kSendThresholdPercent of the previous estimate.
  if (last_send_bitrate_ > 0) {
    unsigned int new_remb_bitrate = last_send_bitrate_ - bitrate_ + bitrate;

    if (new_remb_bitrate < kSendThresholdPercent * last_send_bitrate_ / 100) {
      // The new bitrate estimate is less than kSendThresholdPercent % of the
      // last report. Send a REMB asap.
      last_remb_time_ = TickTime::MillisecondTimestamp() - kRembSendIntervalMs;
    }
  }
  bitrate_ = bitrate;

  // Calculate total receive bitrate estimate.
  int64_t now = TickTime::MillisecondTimestamp();

  if (now - last_remb_time_ < kRembSendIntervalMs) {
    list_crit_->Leave();
    return;
  }
  last_remb_time_ = now;

  if (ssrcs.empty() || receive_modules_.empty()) {
    list_crit_->Leave();
    return;
  }

  // Send a REMB packet.
  RtpRtcp* sender = NULL;
  if (!rtcp_sender_.empty()) {
    sender = rtcp_sender_.front();
  } else {
    sender = receive_modules_.front();
  }
  last_send_bitrate_ = bitrate_;

  list_crit_->Leave();

  if (sender) {
    // TODO(holmer): Change RTP module API to take a const vector reference.
    sender->SetREMBData(bitrate_, ssrcs.size(), &ssrcs[0]);
  }
}

}  // namespace webrtc
