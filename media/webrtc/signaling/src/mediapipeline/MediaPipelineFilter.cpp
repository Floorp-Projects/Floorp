/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: softtabstop=2:shiftwidth=2:expandtab
 * */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: bcampen@mozilla.com

#include "MediaPipelineFilter.h"

#include "webrtc/modules/interface/module_common_types.h"

namespace mozilla {

MediaPipelineFilter::MediaPipelineFilter() : correlator_(0) {
}

bool MediaPipelineFilter::Filter(const webrtc::RTPHeader& header,
                                 uint32_t correlator) {
  if (correlator) {
    // This special correlator header takes precedence. It also lets us learn
    // about SSRC mappings if we don't know about them yet.
    if (correlator == correlator_) {
      AddRemoteSSRC(header.ssrc);
      return true;
    } else {
      // Some other stream; it is possible that an SSRC has moved, so make sure
      // we don't have that SSRC in our filter any more.
      remote_ssrc_set_.erase(header.ssrc);
      return false;
    }
  }

  if (remote_ssrc_set_.count(header.ssrc)) {
    return true;
  }

  // Last ditch effort...
  if (payload_type_set_.count(header.payloadType)) {
    // Actual match. We need to update the ssrc map so we can route rtcp
    // sender reports correctly (these use a different payload-type field)
    AddRemoteSSRC(header.ssrc);
    return true;
  }

  return false;
}

bool MediaPipelineFilter::FilterRTCP(uint32_t ssrc) {
  return remote_ssrc_set_.count(ssrc) != 0;
}

bool MediaPipelineFilter::FilterRTCPReceiverReport(uint32_t ssrc) {
  return local_ssrc_set_.count(ssrc) != 0;
}

void MediaPipelineFilter::AddLocalSSRC(uint32_t ssrc) {
  local_ssrc_set_.insert(ssrc);
}

void MediaPipelineFilter::AddRemoteSSRC(uint32_t ssrc) {
  remote_ssrc_set_.insert(ssrc);
}

void MediaPipelineFilter::AddUniquePT(uint8_t payload_type) {
  payload_type_set_.insert(payload_type);
}

void MediaPipelineFilter::SetCorrelator(uint32_t correlator) {
  correlator_ = correlator;
}

void MediaPipelineFilter::IncorporateRemoteDescription(
    const MediaPipelineFilter& remote_filter) {
  // Update SSRCs; we completely replace the remote SSRCs, since this could be
  // renegotiation. We leave our SSRCs alone, though.
  if (!remote_filter.remote_ssrc_set_.empty()) {
    remote_ssrc_set_ = remote_filter.remote_ssrc_set_;
  }

  // We do not mess with the payload types or correlator here, since the remote
  // SDP doesn't tell us anything about what we will be receiving.
}

} // end namespace mozilla


