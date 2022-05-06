/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_QP_PARSER_H_
#define MODULES_VIDEO_CODING_UTILITY_QP_PARSER_H_

#include <map>

#include "absl/types/optional.h"
#include "api/video/video_codec_type.h"
#include "common_video/h264/h264_bitstream_parser.h"

namespace webrtc {

class QpParser {
 public:
  absl::optional<uint32_t> Parse(VideoCodecType codec_type,
                                 size_t spatial_idx,
                                 const uint8_t* frame_data,
                                 size_t frame_size);

 private:
  H264BitstreamParser& FetchOrCreateH264Parser(size_t spatial_idx);

  std::map<size_t, H264BitstreamParser> h264_parsers_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_QP_PARSER_H_
