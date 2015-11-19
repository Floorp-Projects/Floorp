/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/libyuv/include/scaler.h"

#include <algorithm>

// NOTE(ajm): Path provided by gyp.
#include "libyuv.h"  // NOLINT

namespace webrtc {

Scaler::Scaler()
    : method_(kScaleBox),
      src_width_(0),
      src_height_(0),
      dst_width_(0),
      dst_height_(0),
      set_(false) {}

Scaler::~Scaler() {}

int Scaler::Set(int src_width, int src_height,
                int dst_width, int dst_height,
                VideoType src_video_type, VideoType dst_video_type,
                ScaleMethod method) {
  set_ = false;
  if (src_width < 1 || src_height < 1 || dst_width < 1 || dst_height < 1)
    return -1;

  if (!SupportedVideoType(src_video_type, dst_video_type))
    return -1;

  src_width_ = src_width;
  src_height_ = src_height;
  dst_width_ = dst_width;
  dst_height_ = dst_height;
  method_ = method;
  set_ = true;
  return 0;
}

int Scaler::Scale(const I420VideoFrame& src_frame,
                  I420VideoFrame* dst_frame) {
  assert(dst_frame);
  if (src_frame.IsZeroSize())
    return -1;
  if (!set_)
    return -2;

  // Making sure that destination frame is of sufficient size.
  dst_frame->set_video_frame_buffer(
      buffer_pool_.CreateBuffer(dst_width_, dst_height_));

  // We want to preserve aspect ratio instead of stretching the frame.
  // Therefore, we need to crop the source frame. Calculate the largest center
  // aligned region of the source frame that can be used.
  const int cropped_src_width =
      std::min(src_width_, dst_width_ * src_height_ / dst_height_);
  const int cropped_src_height =
      std::min(src_height_, dst_height_ * src_width_ / dst_width_);
  // Make sure the offsets are even to avoid rounding errors for the U/V planes.
  const int src_offset_x = ((src_width_ - cropped_src_width) / 2) & ~1;
  const int src_offset_y = ((src_height_ - cropped_src_height) / 2) & ~1;

  const uint8_t* y_ptr = src_frame.buffer(kYPlane) +
                         src_offset_y * src_frame.stride(kYPlane) +
                         src_offset_x;
  const uint8_t* u_ptr = src_frame.buffer(kUPlane) +
                         src_offset_y / 2 * src_frame.stride(kUPlane) +
                         src_offset_x / 2;
  const uint8_t* v_ptr = src_frame.buffer(kVPlane) +
                         src_offset_y / 2 * src_frame.stride(kVPlane) +
                         src_offset_x / 2;

  return libyuv::I420Scale(y_ptr,
                           src_frame.stride(kYPlane),
                           u_ptr,
                           src_frame.stride(kUPlane),
                           v_ptr,
                           src_frame.stride(kVPlane),
                           cropped_src_width, cropped_src_height,
                           dst_frame->buffer(kYPlane),
                           dst_frame->stride(kYPlane),
                           dst_frame->buffer(kUPlane),
                           dst_frame->stride(kUPlane),
                           dst_frame->buffer(kVPlane),
                           dst_frame->stride(kVPlane),
                           dst_width_, dst_height_,
                           libyuv::FilterMode(method_));
}

bool Scaler::SupportedVideoType(VideoType src_video_type,
                                VideoType dst_video_type) {
  if (src_video_type != dst_video_type)
    return false;

  if ((src_video_type == kI420) || (src_video_type == kIYUV) ||
      (src_video_type == kYV12))
    return true;

  return false;
}

}  // namespace webrtc
