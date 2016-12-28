/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/config.h"

#include <sstream>
#include <string>

namespace webrtc {
std::string FecConfig::ToString() const {
  std::stringstream ss;
  ss << "{ulpfec_payload_type: " << ulpfec_payload_type;
  ss << ", red_payload_type: " << red_payload_type;
  ss << '}';
  return ss.str();
}

std::string RtpExtension::ToString() const {
  std::stringstream ss;
  ss << "{name: " << name;
  ss << ", id: " << id;
  ss << '}';
  return ss.str();
}

const char* RtpExtension::kTOffset = "urn:ietf:params:rtp-hdrext:toffset";
const char* RtpExtension::kAbsSendTime =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
const char* RtpExtension::kVideoRotation = "urn:3gpp:video-orientation";
const char* RtpExtension::kAudioLevel =
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level";
const char* RtpExtension::kTransportSequenceNumber =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions";
const char* RtpExtension::kRtpStreamId =
  "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id";

bool RtpExtension::IsSupportedForAudio(const std::string& name) {
  return name == webrtc::RtpExtension::kAbsSendTime ||
         name == webrtc::RtpExtension::kAudioLevel ||
         name == webrtc::RtpExtension::kTransportSequenceNumber ||
         name == webrtc::RtpExtension::kRtpStreamId;
}

bool RtpExtension::IsSupportedForVideo(const std::string& name) {
  return name == webrtc::RtpExtension::kTOffset ||
         name == webrtc::RtpExtension::kAbsSendTime ||
         name == webrtc::RtpExtension::kVideoRotation ||
         name == webrtc::RtpExtension::kTransportSequenceNumber ||
         name == webrtc::RtpExtension::kRtpStreamId;

}

VideoStream::VideoStream()
    : width(0),
      height(0),
      max_framerate(-1),
      min_bitrate_bps(-1),
      target_bitrate_bps(-1),
      max_bitrate_bps(-1),
      max_qp(-1) {}

VideoStream::~VideoStream() = default;

std::string VideoStream::ToString() const {
  std::stringstream ss;
  ss << "{width: " << width;
  ss << ", height: " << height;
  ss << ", max_framerate: " << max_framerate;
  ss << ", min_bitrate_bps:" << min_bitrate_bps;
  ss << ", target_bitrate_bps:" << target_bitrate_bps;
  ss << ", max_bitrate_bps:" << max_bitrate_bps;
  ss << ", max_qp: " << max_qp;

  ss << ", temporal_layer_thresholds_bps: [";
  for (size_t i = 0; i < temporal_layer_thresholds_bps.size(); ++i) {
    ss << temporal_layer_thresholds_bps[i];
    if (i != temporal_layer_thresholds_bps.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << '}';
  return ss.str();
}

VideoEncoderConfig::VideoEncoderConfig()
    : content_type(ContentType::kRealtimeVideo),
      encoder_specific_settings(NULL),
      min_transmit_bitrate_bps(0) {
}

VideoEncoderConfig::~VideoEncoderConfig() = default;

std::string VideoEncoderConfig::ToString() const {
  std::stringstream ss;

  ss << "{streams: [";
  for (size_t i = 0; i < streams.size(); ++i) {
    ss << streams[i].ToString();
    if (i != streams.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", content_type: ";
  switch (content_type) {
    case ContentType::kRealtimeVideo:
      ss << "kRealtimeVideo";
      break;
    case ContentType::kScreen:
      ss << "kScreenshare";
      break;
  }
  ss << ", encoder_specific_settings: ";
  ss << (encoder_specific_settings != NULL ? "(ptr)" : "NULL");

  ss << ", min_transmit_bitrate_bps: " << min_transmit_bitrate_bps;
  ss << '}';
  return ss.str();
}

}  // namespace webrtc
