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
    case kRealtimeVideo:
      ss << "kRealtimeVideo";
      break;
    case kScreenshare:
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
