/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_FRAME_METADATA_H_
#define API_VIDEO_VIDEO_FRAME_METADATA_H_

#include <cstdint>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// A subset of metadata from the RTP video header, exposed in insertable streams
// API.
class RTC_EXPORT VideoFrameMetadata {
 public:
  VideoFrameMetadata();
  VideoFrameMetadata(const VideoFrameMetadata&) = default;
  VideoFrameMetadata& operator=(const VideoFrameMetadata&) = default;

  uint16_t GetWidth() const;
  void SetWidth(uint16_t width);

  uint16_t GetHeight() const;
  void SetHeight(uint16_t height);

  absl::optional<int64_t> GetFrameId() const;
  void SetFrameId(absl::optional<int64_t> frame_id);

  int GetSpatialIndex() const;
  void SetSpatialIndex(int spatial_index);

  int GetTemporalIndex() const;
  void SetTemporalIndex(int temporal_index);

  rtc::ArrayView<const int64_t> GetFrameDependencies() const;
  void SetFrameDependencies(rtc::ArrayView<const int64_t> frame_dependencies);

  rtc::ArrayView<const DecodeTargetIndication> GetDecodeTargetIndications()
      const;
  void SetDecodeTargetIndications(
      rtc::ArrayView<const DecodeTargetIndication> decode_target_indications);

 private:
  int16_t width_ = 0;
  int16_t height_ = 0;
  absl::optional<int64_t> frame_id_;
  int spatial_index_ = 0;
  int temporal_index_ = 0;
  absl::InlinedVector<int64_t, 5> frame_dependencies_;
  absl::InlinedVector<DecodeTargetIndication, 10> decode_target_indications_;
};
}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_FRAME_METADATA_H_
