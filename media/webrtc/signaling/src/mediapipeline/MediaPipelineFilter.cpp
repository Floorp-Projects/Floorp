/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: softtabstop=2:shiftwidth=2:expandtab
 * */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: bcampen@mozilla.com

#include "logging.h"

#include "MediaPipelineFilter.h"

#include "webrtc/modules/interface/module_common_types.h"

// Logging context
MOZ_MTLOG_MODULE("mediapipeline")

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

MediaPipelineFilter::Result
MediaPipelineFilter::FilterRTCP(const unsigned char* data,
                                size_t len) const {
  if (len < FIRST_SSRC_OFFSET) {
    return FAIL;
  }

  uint8_t payload_type = data[PT_OFFSET];

  switch (payload_type) {
    case SENDER_REPORT_T:
      return CheckRtcpReport(data, len, RECEIVER_REPORT_START_SR) ? PASS : FAIL;
    case RECEIVER_REPORT_T:
      return CheckRtcpReport(data, len, SENDER_REPORT_START_RR) ? PASS : FAIL;
    default:
      return UNSUPPORTED;
  }

  return UNSUPPORTED;
}

bool MediaPipelineFilter::CheckRtcpSsrc(const unsigned char* data,
                                        size_t len,
                                        size_t ssrc_offset,
                                        uint8_t flags) const {
  if (ssrc_offset + 4 > len) {
    return false;
  }

  uint32_t ssrc = 0;
  ssrc += (uint32_t)data[ssrc_offset++] << 24;
  ssrc += (uint32_t)data[ssrc_offset++] << 16;
  ssrc += (uint32_t)data[ssrc_offset++] << 8;
  ssrc += (uint32_t)data[ssrc_offset++];

  if (flags | MAYBE_LOCAL_SSRC) {
    if (local_ssrc_set_.count(ssrc)) {
      return true;
    }
  }

  if (flags | MAYBE_REMOTE_SSRC) {
    if (remote_ssrc_set_.count(ssrc)) {
      return true;
    }
  }
  return false;
}

static uint8_t GetCount(const unsigned char* data, size_t len) {
  // This might be a count of rr reports, or might be a count of stuff like
  // SDES reports, or what-have-you. They all live in bits 3-7.
  return data[0] & 0x1F;
}

bool MediaPipelineFilter::CheckRtcpReport(const unsigned char* data,
                                          size_t len,
                                          size_t first_rr_offset) const {
  bool remote_ssrc_matched = CheckRtcpSsrc(data,
                                           len,
                                           FIRST_SSRC_OFFSET,
                                           MAYBE_REMOTE_SSRC);

  uint8_t rr_count = GetCount(data, len);

  // We need to check for consistency. If the remote ssrc matched, the local
  // ssrcs must too. If it did not, this may just be because our filter is
  // incomplete, so the local ssrcs could go either way.
  bool ssrcs_must_match = remote_ssrc_matched;
  bool ssrcs_must_not_match = false;

  for (uint8_t rr_num = 0; rr_num < rr_count; ++rr_num) {
    size_t ssrc_offset = first_rr_offset + (rr_num * RECEIVER_REPORT_SIZE);

    if (!CheckRtcpSsrc(data, len, ssrc_offset, MAYBE_LOCAL_SSRC)) {
      ssrcs_must_not_match = true;
      if (ssrcs_must_match) {
        break;
      }
    } else {
      ssrcs_must_match = true;
      if (ssrcs_must_not_match) {
        break;
      }
    }
  }

  if (ssrcs_must_match && ssrcs_must_not_match) {
    MOZ_MTLOG(ML_ERROR, "Received an RTCP packet with SSRCs from "
              "multiple m-lines! This is broken.");
    return false;
  }

  // This is set if any ssrc matched
  return ssrcs_must_match;
}

} // end namespace mozilla


