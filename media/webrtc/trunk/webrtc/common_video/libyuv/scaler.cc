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
  // Aligning stride values based on width.
  dst_frame->CreateEmptyFrame(dst_width_, dst_height_,
                              dst_width_, (dst_width_ + 1) / 2,
                              (dst_width_ + 1) / 2);

  return libyuv::I420Scale(src_frame.buffer(kYPlane),
                           src_frame.stride(kYPlane),
                           src_frame.buffer(kUPlane),
                           src_frame.stride(kUPlane),
                           src_frame.buffer(kVPlane),
                           src_frame.stride(kVPlane),
                           src_width_, src_height_,
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
