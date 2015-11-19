/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/interface/video_frame_buffer.h"

#include "webrtc/base/checks.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {

VideoFrameBuffer::~VideoFrameBuffer() {}

I420Buffer::I420Buffer(int width, int height)
    : I420Buffer(width, height, width, (width + 1) / 2, (width + 1) / 2) {
}

I420Buffer::I420Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      data_(static_cast<uint8_t*>(AlignedMalloc(
          stride_y * height + (stride_u + stride_v) * ((height + 1) / 2),
          kBufferAlignment))) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GE(stride_y, width);
  DCHECK_GE(stride_u, (width + 1) / 2);
  DCHECK_GE(stride_v, (width + 1) / 2);
}

I420Buffer::~I420Buffer() {
}

int I420Buffer::width() const {
  return width_;
}

int I420Buffer::height() const {
  return height_;
}

const uint8_t* I420Buffer::data(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return data_.get();
    case kUPlane:
      return data_.get() + stride_y_ * height_;
    case kVPlane:
      return data_.get() + stride_y_ * height_ +
             stride_u_ * ((height_ + 1) / 2);
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

uint8_t* I420Buffer::data(PlaneType type) {
  DCHECK(HasOneRef());
  return const_cast<uint8_t*>(
      static_cast<const VideoFrameBuffer*>(this)->data(type));
}

int I420Buffer::stride(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return stride_y_;
    case kUPlane:
      return stride_u_;
    case kVPlane:
      return stride_v_;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

rtc::scoped_refptr<NativeHandle> I420Buffer::native_handle() const {
  return nullptr;
}

TextureBuffer::TextureBuffer(
    const rtc::scoped_refptr<NativeHandle>& native_handle,
    int width,
    int height)
    : native_handle_(native_handle), width_(width), height_(height) {
  DCHECK(native_handle.get());
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
}

TextureBuffer::~TextureBuffer() {
}

int TextureBuffer::width() const {
  return width_;
}

int TextureBuffer::height() const {
  return height_;
}

const uint8_t* TextureBuffer::data(PlaneType type) const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

uint8_t* TextureBuffer::data(PlaneType type) {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

int TextureBuffer::stride(PlaneType type) const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}

rtc::scoped_refptr<NativeHandle> TextureBuffer::native_handle() const {
  return native_handle_;
}


WrappedI420Buffer::WrappedI420Buffer(int desired_width,
                                     int desired_height,
                                     int width,
                                     int height,
                                     const uint8_t* y_plane,
                                     int y_stride,
                                     const uint8_t* u_plane,
                                     int u_stride,
                                     const uint8_t* v_plane,
                                     int v_stride,
                                     const rtc::Callback0<void>& no_longer_used)
  :  width_(desired_width),
     height_(desired_height),
     y_plane_(y_plane),
     u_plane_(u_plane),
     v_plane_(v_plane),
     y_stride_(y_stride),
     u_stride_(u_stride),
     v_stride_(v_stride),
     no_longer_used_cb_(no_longer_used) {
  CHECK(width >= desired_width && height >= desired_height);

  // Center crop to |desired_width| x |desired_height|.
  // Make sure offset is even so that u/v plane becomes aligned.
  const int offset_x = ((width - desired_width) / 2) & ~1;
  const int offset_y = ((height - desired_height) / 2) & ~1;
  y_plane_ += y_stride_ * offset_y + offset_x;
  u_plane_ += u_stride_ * (offset_y / 2) + (offset_x / 2);
  v_plane_ += v_stride_ * (offset_y / 2) + (offset_x / 2);
}

WrappedI420Buffer::~WrappedI420Buffer() {
  no_longer_used_cb_();
}


int WrappedI420Buffer::width() const {
  return width_;
}

int WrappedI420Buffer::height() const {
  return height_;
}

const uint8_t* WrappedI420Buffer::data(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return y_plane_;
    case kUPlane:
      return u_plane_;
    case kVPlane:
      return v_plane_;
    default:
      RTC_NOTREACHED();
      return nullptr;
  }
}

uint8_t* WrappedI420Buffer::data(PlaneType type) {
  RTC_NOTREACHED();
  return nullptr;
}

int WrappedI420Buffer::stride(PlaneType type) const {
  switch (type) {
    case kYPlane:
      return y_stride_;
    case kUPlane:
      return u_stride_;
    case kVPlane:
      return v_stride_;
    default:
      RTC_NOTREACHED();
      return 0;
  }
}

rtc::scoped_refptr<NativeHandle> WrappedI420Buffer::native_handle() const {
  return nullptr;
}

}  // namespace webrtc
