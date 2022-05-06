/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/qp_parser.h"

#include <utility>

#include "modules/video_coding/utility/vp8_header_parser.h"
#include "modules/video_coding/utility/vp9_uncompressed_header_parser.h"

namespace webrtc {

absl::optional<uint32_t> QpParser::Parse(VideoCodecType codec_type,
                                         size_t spatial_idx,
                                         const uint8_t* frame_data,
                                         size_t frame_size) {
  if (frame_data == nullptr || frame_size == 0) {
    return absl::nullopt;
  }

  if (codec_type == kVideoCodecVP8) {
    int qp = -1;
    if (vp8::GetQp(frame_data, frame_size, &qp)) {
      return qp;
    }
  } else if (codec_type == kVideoCodecVP9) {
    int qp = -1;
    if (vp9::GetQp(frame_data, frame_size, &qp)) {
      return qp;
    }
  } else if (codec_type == kVideoCodecH264) {
    H264BitstreamParser& parser = FetchOrCreateH264Parser(spatial_idx);
    parser.ParseBitstream(
        rtc::ArrayView<const uint8_t>(frame_data, frame_size));
    return parser.GetLastSliceQp();
  }

  return absl::nullopt;
}

H264BitstreamParser& QpParser::FetchOrCreateH264Parser(size_t spatial_idx) {
  if (h264_parsers_.find(spatial_idx) == h264_parsers_.end()) {
    h264_parsers_.emplace(std::make_pair(spatial_idx, H264BitstreamParser()));
  }
  return h264_parsers_.at(spatial_idx);
}

}  // namespace webrtc
