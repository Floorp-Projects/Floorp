/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: softtabstop=2:shiftwidth=2:expandtab
 * */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: bcampen@mozilla.com

#include "MediaPipelineFilter.h"

#include "webrtc/common_types.h"
#include "webrtc/api/rtpparameters.h"
#include "mozilla/Logging.h"

// defined in MediaPipeline.cpp
extern mozilla::LazyLogModule gMediaPipelineLog;

#define DEBUG_LOG(x) MOZ_LOG(gMediaPipelineLog, LogLevel::Debug, x)

namespace mozilla {
MediaPipelineFilter::MediaPipelineFilter(
    const std::vector<webrtc::RtpExtension>& aExtMap)
    : mExtMap(aExtMap) {}

void MediaPipelineFilter::SetRemoteMediaStreamId(
    const Maybe<std::string>& aMid) {
  if (aMid != mRemoteMid) {
    DEBUG_LOG(("MediaPipelineFilter added new remote RTP MID: '%s'.",
               aMid.valueOr("").c_str()));
    mRemoteMidBinding = Nothing();
    mRemoteMid = aMid;
  }
}

void MediaPipelineFilter::AddRtpExtensionMapping(
    const webrtc::RtpExtension& aExt) {
  mExtMap.push_back(aExt);
}

Maybe<webrtc::RtpExtension> MediaPipelineFilter::GetRtpExtension(
    const std::string& aUri) const {
  for (const auto& ext : mExtMap) {
    if (ext.uri == aUri) {
      return Some(ext);
    }
  }
  return Nothing();
}

bool MediaPipelineFilter::Filter(const webrtc::RTPHeader& header) {
  DEBUG_LOG(("MediaPipelineFilter inspecting seq# %u SSRC: %u",
             header.sequenceNumber, header.ssrc));

  auto fromStreamId = [&](const webrtc::StreamId& aId) {
    return Maybe<std::string>(aId.empty() ? Nothing() : Some(aId.data()));
  };

  //
  //  MID Based Filtering
  //

  const auto& mid = fromStreamId(header.extension.mid);

  // Check to see if a bound SSRC is moved to a new MID
  if (mRemoteMidBinding == Some(header.ssrc) && mid && mRemoteMid != mid) {
    mRemoteMidBinding = Nothing();
  }
  // Bind an SSRC if a matching MID is found
  if (mid && mRemoteMid == mid) {
    DEBUG_LOG(("MediaPipelineFilter learned SSRC: %u for MID: '%s'",
               header.ssrc, mRemoteMid.value().c_str()));
    mRemoteMidBinding = Some(header.ssrc);
  }
  // Check for matching MID
  if (mRemoteMidBinding) {
    MOZ_ASSERT(mRemoteMid != Nothing());
    if (mRemoteMidBinding == Some(header.ssrc)) {
      DEBUG_LOG(
          ("MediaPipelineFilter SSRC: %u matched for MID: '%s'."
           " passing packet",
           header.ssrc, mRemoteMid.value().c_str()));
      return true;
    }
    DEBUG_LOG(
        ("MediaPipelineFilter SSRC: %u did not match bound SSRC %u for"
         " MID: '%s'. ignoring packet",
         header.ssrc, mRemoteMidBinding.value(), mRemoteMid.value().c_str()));
    return false;
  }

  //
  // RTP-STREAM-ID based filtering (for tests only)
  //

  const auto streamId = fromStreamId(header.extension.stream_id);
  if (streamId && !remote_rid_set_.empty()) {
    if (remote_rid_set_.count(streamId.value())) {
      DEBUG_LOG(("MediaPipelineFilter RID: %s matched. passing packet",
                 streamId.value().c_str()));
      return true;
    }
    DEBUG_LOG(("MediaPipelineFilter RID: %s did not match any of %zu RIDs",
               streamId.value().c_str(), remote_rid_set_.size()));
  }

  //
  // Remote SSRC based filtering
  //

  if (remote_ssrc_set_.count(header.ssrc)) {
    DEBUG_LOG(
        ("MediaPipelineFilter SSRC: %u matched remote SSRC set."
         " passing packet",
         header.ssrc));
    return true;
  }
  DEBUG_LOG(
      ("MediaPipelineFilter SSRC: %u did not match any of %zu"
       " remote SSRCS.",
       header.ssrc, remote_ssrc_set_.size()));

  //
  // PT, payload type, last ditch effort filtering
  //

  if (payload_type_set_.count(header.payloadType)) {
    DEBUG_LOG(
        ("MediaPipelineFilter payload-type: %u matched %zu"
         " unique payload type. learning ssrc. passing packet",
         header.ssrc, remote_ssrc_set_.size()));
    // Actual match. We need to update the ssrc map so we can route rtcp
    // sender reports correctly (these use a different payload-type field)
    AddRemoteSSRC(header.ssrc);
    return true;
  }
  DEBUG_LOG(
      ("MediaPipelineFilter payload-type: %u did not match any of %zu"
       " unique payload-types.",
       header.payloadType, payload_type_set_.size()));
  DEBUG_LOG(
      ("MediaPipelineFilter packet failed to match any criteria."
       " ignoring packet"));
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

void MediaPipelineFilter::Update(const MediaPipelineFilter& filter_update) {
  // We will not stomp the remote_ssrc_set_ if the update has no ssrcs,
  // because we don't want to unlearn any remote ssrcs unless the other end
  // has explicitly given us a new set.
  if (!filter_update.remote_ssrc_set_.empty()) {
    remote_ssrc_set_ = filter_update.remote_ssrc_set_;
  }
  // We don't want to overwrite the learned binding unless we have changed MIDs
  // or the update contains a MID binding.
  if (filter_update.mRemoteMidBinding ||
      (filter_update.mRemoteMid && filter_update.mRemoteMid != mRemoteMid)) {
    mRemoteMid = filter_update.mRemoteMid;
    mRemoteMidBinding = filter_update.mRemoteMidBinding;
  }
  payload_type_set_ = filter_update.payload_type_set_;

  // Use extmapping from new filter
  mExtMap = filter_update.mExtMap;
}

}  // end namespace mozilla
