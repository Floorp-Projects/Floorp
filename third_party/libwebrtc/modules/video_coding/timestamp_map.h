/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_
#define MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/rtp_packet_infos.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_content_type.h"
#include "api/video/video_rotation.h"

namespace webrtc {

struct FrameInformation {
  // This is likely not optional, but some inputs seem to sometimes be negative.
  // TODO(bugs.webrtc.org/13756): See if this can be replaced with Timestamp
  // once all inputs to this field use Timestamp instead of an integer.
  absl::optional<Timestamp> render_time;
  absl::optional<Timestamp> decode_start;
  VideoRotation rotation;
  VideoContentType content_type;
  EncodedImage::Timing timing;
  int64_t ntp_time_ms;
  RtpPacketInfos packet_infos;
  // ColorSpace is not stored here, as it might be modified by decoders.
};

class TimestampMap {
 public:
  explicit TimestampMap(size_t capacity);
  ~TimestampMap();

  void Add(uint32_t timestamp, const FrameInformation& data);
  absl::optional<FrameInformation> Pop(uint32_t timestamp);
  size_t Size() const;
  void Clear();

 private:
  struct TimestampDataTuple {
    uint32_t timestamp;
    FrameInformation data;
  };
  bool IsEmpty() const;

  std::unique_ptr<TimestampDataTuple[]> ring_buffer_;
  const size_t capacity_;
  size_t next_add_idx_;
  size_t next_pop_idx_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMESTAMP_MAP_H_
