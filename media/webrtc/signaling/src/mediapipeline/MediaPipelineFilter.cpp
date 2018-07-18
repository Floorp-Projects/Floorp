/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: softtabstop=2:shiftwidth=2:expandtab
 * */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: bcampen@mozilla.com

#include "MediaPipelineFilter.h"

#include "webrtc/common_types.h"

#include "CSFLog.h"

static const char* mpfLogTag = "MediaPipelineFilter";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG mpfLogTag

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
    }
    // Some other stream; it is possible that an SSRC has moved, so make sure
    // we don't have that SSRC in our filter any more.
    remote_ssrc_set_.erase(header.ssrc);
    return false;
  }

  if (!header.extension.rtpStreamId.empty() &&
      !remote_rid_set_.empty() &&
      remote_rid_set_.count(header.extension.rtpStreamId.data())) {
    return true;
  }
  if (!header.extension.rtpStreamId.empty()) {
    CSFLogDebug(LOGTAG,
                "MediaPipelineFilter ignoring seq# %u ssrc: %u RID: %s",
                header.sequenceNumber, header.ssrc,
                header.extension.rtpStreamId.data());
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

void MediaPipelineFilter::AddRemoteSSRC(uint32_t ssrc) {
  remote_ssrc_set_.insert(ssrc);
}

void MediaPipelineFilter::AddRemoteRtpStreamId(const std::string& rtp_strm_id) {
  remote_rid_set_.insert(rtp_strm_id);
}

void MediaPipelineFilter::AddUniquePT(uint8_t payload_type) {
  payload_type_set_.insert(payload_type);
}

void MediaPipelineFilter::SetCorrelator(uint32_t correlator) {
  correlator_ = correlator;
}

void MediaPipelineFilter::Update(const MediaPipelineFilter& filter_update) {
  // We will not stomp the remote_ssrc_set_ if the update has no ssrcs,
  // because we don't want to unlearn any remote ssrcs unless the other end
  // has explicitly given us a new set.
  if (!filter_update.remote_ssrc_set_.empty()) {
    remote_ssrc_set_ = filter_update.remote_ssrc_set_;
  }

  payload_type_set_ = filter_update.payload_type_set_;
  correlator_ = filter_update.correlator_;
}

} // end namespace mozilla

