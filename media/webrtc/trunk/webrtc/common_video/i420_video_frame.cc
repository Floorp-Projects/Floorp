/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/i420_video_frame.h"

#include <string.h>

#include <algorithm>  // swap

namespace webrtc {

I420VideoFrame::I420VideoFrame()
    : width_(0),
      height_(0),
      timestamp_(0),
      ntp_time_ms_(0),
      render_time_ms_(0) {}

I420VideoFrame::~I420VideoFrame() {}

int I420VideoFrame::CreateEmptyFrame(int width, int height,
                                     int stride_y, int stride_u, int stride_v) {
  if (CheckDimensions(width, height, stride_y, stride_u, stride_v) < 0)
    return -1;
  int size_y = stride_y * height;
  int half_height = (height + 1) / 2;
  int size_u = stride_u * half_height;
  int size_v = stride_v * half_height;
  width_ = width;
  height_ = height;
  y_plane_.CreateEmptyPlane(size_y, stride_y, size_y);
  u_plane_.CreateEmptyPlane(size_u, stride_u, size_u);
  v_plane_.CreateEmptyPlane(size_v, stride_v, size_v);
  // Creating empty frame - reset all values.
  timestamp_ = 0;
  ntp_time_ms_ = 0;
  render_time_ms_ = 0;
  return 0;
}

int I420VideoFrame::CreateFrame(int size_y, const uint8_t* buffer_y,
                                int size_u, const uint8_t* buffer_u,
                                int size_v, const uint8_t* buffer_v,
                                int width, int height,
                                int stride_y, int stride_u, int stride_v) {
  if (size_y < 1 || size_u < 1 || size_v < 1)
    return -1;
  if (CheckDimensions(width, height, stride_y, stride_u, stride_v) < 0)
    return -1;
  y_plane_.Copy(size_y, stride_y, buffer_y);
  u_plane_.Copy(size_u, stride_u, buffer_u);
  v_plane_.Copy(size_v, stride_v, buffer_v);
  width_ = width;
  height_ = height;
  return 0;
}

int I420VideoFrame::CopyFrame(const I420VideoFrame& videoFrame) {
  int ret = CreateFrame(videoFrame.allocated_size(kYPlane),
                        videoFrame.buffer(kYPlane),
                        videoFrame.allocated_size(kUPlane),
                        videoFrame.buffer(kUPlane),
                        videoFrame.allocated_size(kVPlane),
                        videoFrame.buffer(kVPlane),
                        videoFrame.width_, videoFrame.height_,
                        videoFrame.stride(kYPlane), videoFrame.stride(kUPlane),
                        videoFrame.stride(kVPlane));
  if (ret < 0)
    return ret;
  timestamp_ = videoFrame.timestamp_;
  ntp_time_ms_ = videoFrame.ntp_time_ms_;
  render_time_ms_ = videoFrame.render_time_ms_;
  return 0;
}

I420VideoFrame* I420VideoFrame::CloneFrame() const {
  scoped_ptr<I420VideoFrame> new_frame(new I420VideoFrame());
  if (new_frame->CopyFrame(*this) == -1) {
    // CopyFrame failed.
    return NULL;
  }
  return new_frame.release();
}

void I420VideoFrame::SwapFrame(I420VideoFrame* videoFrame) {
  y_plane_.Swap(videoFrame->y_plane_);
  u_plane_.Swap(videoFrame->u_plane_);
  v_plane_.Swap(videoFrame->v_plane_);
  std::swap(width_, videoFrame->width_);
  std::swap(height_, videoFrame->height_);
  std::swap(timestamp_, videoFrame->timestamp_);
  std::swap(ntp_time_ms_, videoFrame->ntp_time_ms_);
  std::swap(render_time_ms_, videoFrame->render_time_ms_);
}

uint8_t* I420VideoFrame::buffer(PlaneType type) {
  Plane* plane_ptr = GetPlane(type);
  if (plane_ptr)
    return plane_ptr->buffer();
  return NULL;
}

const uint8_t* I420VideoFrame::buffer(PlaneType type) const {
  const Plane* plane_ptr = GetPlane(type);
  if (plane_ptr)
    return plane_ptr->buffer();
  return NULL;
}

int I420VideoFrame::allocated_size(PlaneType type) const {
  const Plane* plane_ptr = GetPlane(type);
    if (plane_ptr)
      return plane_ptr->allocated_size();
  return -1;
}

int I420VideoFrame::stride(PlaneType type) const {
  const Plane* plane_ptr = GetPlane(type);
  if (plane_ptr)
    return plane_ptr->stride();
  return -1;
}

int I420VideoFrame::set_width(int width) {
  if (CheckDimensions(width, height_,
                      y_plane_.stride(), u_plane_.stride(),
                      v_plane_.stride()) < 0)
    return -1;
  width_ = width;
  return 0;
}

int I420VideoFrame::set_height(int height) {
  if (CheckDimensions(width_, height,
                      y_plane_.stride(), u_plane_.stride(),
                      v_plane_.stride()) < 0)
    return -1;
  height_ = height;
  return 0;
}

bool I420VideoFrame::IsZeroSize() const {
  return (y_plane_.IsZeroSize() && u_plane_.IsZeroSize() &&
    v_plane_.IsZeroSize());
}

void I420VideoFrame::ResetSize() {
  y_plane_.ResetSize();
  u_plane_.ResetSize();
  v_plane_.ResetSize();
}

void* I420VideoFrame::native_handle() const { return NULL; }

int I420VideoFrame::CheckDimensions(int width, int height,
                                    int stride_y, int stride_u, int stride_v) {
  int half_width = (width + 1) / 2;
  if (width < 1 || height < 1 ||
      stride_y < width || stride_u < half_width || stride_v < half_width)
    return -1;
  return 0;
}

const Plane* I420VideoFrame::GetPlane(PlaneType type) const {
  switch (type) {
    case kYPlane :
      return &y_plane_;
    case kUPlane :
      return &u_plane_;
    case kVPlane :
      return &v_plane_;
    default:
      assert(false);
  }
  return NULL;
}

Plane* I420VideoFrame::GetPlane(PlaneType type) {
  switch (type) {
    case kYPlane :
      return &y_plane_;
    case kUPlane :
      return &u_plane_;
    case kVPlane :
      return &v_plane_;
    default:
      assert(false);
  }
  return NULL;
}

}  // namespace webrtc
