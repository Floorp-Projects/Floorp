/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_FRAME_TYPE_H_
#define API_VIDEO_VIDEO_FRAME_TYPE_H_

#include "absl/strings/string_view.h"

namespace webrtc {

enum class VideoFrameType {
  kEmptyFrame = 0,
  // Wire format for MultiplexEncodedImagePacker seems to depend on numerical
  // values of these constants.
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

inline constexpr absl::string_view VideoFrameTypeToString(
    VideoFrameType frame_type) {
  if (frame_type == VideoFrameType::kEmptyFrame) {
    return "empty";
  }
  if (frame_type == VideoFrameType::kVideoFrameKey) {
    return "key";
  }
  if (frame_type == VideoFrameType::kVideoFrameDelta) {
    return "delta";
  }
  return "";
}

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_FRAME_TYPE_H_
