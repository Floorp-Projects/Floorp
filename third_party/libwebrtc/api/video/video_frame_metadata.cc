/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/video_frame_metadata.h"

namespace webrtc {

VideoFrameMetadata::VideoFrameMetadata() = default;

uint16_t VideoFrameMetadata::GetWidth() const {
  return width_;
}

void VideoFrameMetadata::SetWidth(uint16_t width) {
  width_ = width;
}

uint16_t VideoFrameMetadata::GetHeight() const {
  return height_;
}

void VideoFrameMetadata::SetHeight(uint16_t height) {
  height_ = height;
}

absl::optional<int64_t> VideoFrameMetadata::GetFrameId() const {
  return frame_id_;
}

void VideoFrameMetadata::SetFrameId(absl::optional<int64_t> frame_id) {
  frame_id_ = frame_id;
}

int VideoFrameMetadata::GetSpatialIndex() const {
  return spatial_index_;
}

void VideoFrameMetadata::SetSpatialIndex(int spatial_index) {
  spatial_index_ = spatial_index;
}

int VideoFrameMetadata::GetTemporalIndex() const {
  return temporal_index_;
}

void VideoFrameMetadata::SetTemporalIndex(int temporal_index) {
  temporal_index_ = temporal_index;
}

rtc::ArrayView<const int64_t> VideoFrameMetadata::GetFrameDependencies() const {
  return frame_dependencies_;
}

void VideoFrameMetadata::SetFrameDependencies(
    rtc::ArrayView<const int64_t> frame_dependencies) {
  frame_dependencies_.assign(frame_dependencies.begin(),
                             frame_dependencies.end());
}

rtc::ArrayView<const DecodeTargetIndication>
VideoFrameMetadata::GetDecodeTargetIndications() const {
  return decode_target_indications_;
}

void VideoFrameMetadata::SetDecodeTargetIndications(
    rtc::ArrayView<const DecodeTargetIndication> decode_target_indications) {
  decode_target_indications_.assign(decode_target_indications.begin(),
                                    decode_target_indications.end());
}

}  // namespace webrtc
