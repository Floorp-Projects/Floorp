/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/nv12_buffer.h"

#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "libyuv/include/libyuv/convert.h"

namespace webrtc {

namespace {

static const int kBufferAlignment = 64;

int NV12DataSize(int height, int stride_y, int stride_uv) {
  return stride_y * height + stride_uv * ((height + 1) / 2);
}

}  // namespace

NV12Buffer::NV12Buffer(int width, int height)
    : NV12Buffer(width, height, width, width + width % 2) {}

NV12Buffer::NV12Buffer(int width, int height, int stride_y, int stride_uv)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_uv_(stride_uv),
      data_(static_cast<uint8_t*>(
          AlignedMalloc(NV12DataSize(height_, stride_y_, stride_uv),
                        kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GE(stride_y, width);
  RTC_DCHECK_GE(stride_uv, (width + width % 2));
}

NV12Buffer::~NV12Buffer() = default;

// static
rtc::scoped_refptr<NV12Buffer> NV12Buffer::Create(int width, int height) {
  return new rtc::RefCountedObject<NV12Buffer>(width, height);
}

// static
rtc::scoped_refptr<NV12Buffer> NV12Buffer::Create(int width,
                                                  int height,
                                                  int stride_y,
                                                  int stride_uv) {
  return new rtc::RefCountedObject<NV12Buffer>(width, height, stride_y,
                                               stride_uv);
}

rtc::scoped_refptr<I420BufferInterface> NV12Buffer::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  libyuv::NV12ToI420(DataY(), StrideY(), DataUV(), StrideUV(),
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     width(), height());
  return i420_buffer;
}

int NV12Buffer::width() const {
  return width_;
}
int NV12Buffer::height() const {
  return height_;
}

int NV12Buffer::StrideY() const {
  return stride_y_;
}
int NV12Buffer::StrideUV() const {
  return stride_uv_;
}

const uint8_t* NV12Buffer::DataY() const {
  return data_.get();
}

const uint8_t* NV12Buffer::DataUV() const {
  return data_.get() + UVOffset();
}

uint8_t* NV12Buffer::MutableDataY() {
  return data_.get();
}

uint8_t* NV12Buffer::MutableDataUV() {
  return data_.get() + UVOffset();
}

size_t NV12Buffer::UVOffset() const {
  return stride_y_ * height_;
}

void NV12Buffer::InitializeData() {
  memset(data_.get(), 0, NV12DataSize(height_, stride_y_, stride_uv_));
}

}  // namespace webrtc
