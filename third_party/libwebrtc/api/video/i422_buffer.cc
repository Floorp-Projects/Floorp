/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video/i422_buffer.h"

#include <string.h>

#include <algorithm>
#include <utility>

#include "api/video/i420_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_counted_object.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/scale.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {

namespace {

int I422DataSize(int height, int stride_y, int stride_u, int stride_v) {
  return stride_y * height + stride_u * height + stride_v * height;
}

int I422Rotate(const uint8_t* src_y,
               int src_stride_y,
               const uint8_t* src_u,
               int src_stride_u,
               const uint8_t* src_v,
               int src_stride_v,
               uint8_t* dst_y,
               int dst_stride_y,
               uint8_t* dst_u,
               int dst_stride_u,
               uint8_t* dst_v,
               int dst_stride_v,
               int width,
               int height,
               enum libyuv::RotationMode mode) {
  int halfwidth = (width + 1) >> 1;
  int halfheight = (height + 1) >> 1;
  if (!src_y || !src_u || !src_v || width <= 0 || height == 0 || !dst_y ||
      !dst_u || !dst_v) {
    return -1;
  }
  // Negative height means invert the image.
  if (height < 0) {
    height = -height;
    src_y = src_y + (height - 1) * src_stride_y;
    src_u = src_u + (height - 1) * src_stride_u;
    src_v = src_v + (height - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }

  switch (mode) {
    case libyuv::kRotate0:
      // copy frame
      libyuv::CopyPlane(src_y, src_stride_y, dst_y, dst_stride_y, width,
                        height);
      libyuv::CopyPlane(src_u, src_stride_u, dst_u, dst_stride_u, halfwidth,
                        height);
      libyuv::CopyPlane(src_v, src_stride_v, dst_v, dst_stride_v, halfwidth,
                        height);
      return 0;
    case libyuv::kRotate90:
      // We need to rotate and rescale, we use plane Y as temporal storage.
      libyuv::RotatePlane90(src_u, src_stride_u, dst_y, height, halfwidth,
                            height);
      libyuv::ScalePlane(dst_y, height, height, halfwidth, dst_u, halfheight,
                         halfheight, width, libyuv::kFilterBilinear);
      libyuv::RotatePlane90(src_v, src_stride_v, dst_y, height, halfwidth,
                            height);
      libyuv::ScalePlane(dst_y, height, height, halfwidth, dst_v, halfheight,
                         halfheight, width, libyuv::kFilterLinear);
      libyuv::RotatePlane90(src_y, src_stride_y, dst_y, dst_stride_y, width,
                            height);
      return 0;
    case libyuv::kRotate270:
      // We need to rotate and rescale, we use plane Y as temporal storage.
      libyuv::RotatePlane270(src_u, src_stride_u, dst_y, height, halfwidth,
                             height);
      libyuv::ScalePlane(dst_y, height, height, halfwidth, dst_u, halfheight,
                         halfheight, width, libyuv::kFilterBilinear);
      libyuv::RotatePlane270(src_v, src_stride_v, dst_y, height, halfwidth,
                             height);
      libyuv::ScalePlane(dst_y, height, height, halfwidth, dst_v, halfheight,
                         halfheight, width, libyuv::kFilterLinear);
      libyuv::RotatePlane270(src_y, src_stride_y, dst_y, dst_stride_y, width,
                             height);

      return 0;
    case libyuv::kRotate180:
      libyuv::RotatePlane180(src_y, src_stride_y, dst_y, dst_stride_y, width,
                             height);
      libyuv::RotatePlane180(src_u, src_stride_u, dst_u, dst_stride_u,
                             halfwidth, height);
      libyuv::RotatePlane180(src_v, src_stride_v, dst_v, dst_stride_v,
                             halfwidth, height);
      return 0;
    default:
      break;
  }
  return -1;
}

int I422Scale(const uint8_t* src_y,
              int src_stride_y,
              const uint8_t* src_u,
              int src_stride_u,
              const uint8_t* src_v,
              int src_stride_v,
              int src_width,
              int src_height,
              uint8_t* dst_y,
              int dst_stride_y,
              uint8_t* dst_u,
              int dst_stride_u,
              uint8_t* dst_v,
              int dst_stride_v,
              int dst_width,
              int dst_height,
              enum libyuv::FilterMode filtering) {
  if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
      src_width > 32768 || src_height > 32768 || !dst_y || !dst_u || !dst_v ||
      dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  int src_halfwidth = (src_width + 1) >> 1;
  int dst_halfwidth = (dst_width + 1) >> 1;

  libyuv::ScalePlane(src_y, src_stride_y, src_width, src_height, dst_y,
                     dst_stride_y, dst_width, dst_height, filtering);
  libyuv::ScalePlane(src_u, src_stride_u, src_halfwidth, src_height, dst_u,
                     dst_stride_u, dst_halfwidth, dst_height, filtering);
  libyuv::ScalePlane(src_v, src_stride_v, src_halfwidth, src_height, dst_v,
                     dst_stride_v, dst_halfwidth, dst_height, filtering);
  return 0;
}
}  // namespace

I422Buffer::I422Buffer(int width, int height)
    : I422Buffer(width, height, width, (width + 1) / 2, (width + 1) / 2) {}

I422Buffer::I422Buffer(int width,
                       int height,
                       int stride_y,
                       int stride_u,
                       int stride_v)
    : width_(width),
      height_(height),
      stride_y_(stride_y),
      stride_u_(stride_u),
      stride_v_(stride_v),
      data_(static_cast<uint8_t*>(
          AlignedMalloc(I422DataSize(height, stride_y, stride_u, stride_v),
                        kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GE(stride_y, width);
  RTC_DCHECK_GE(stride_u, (width + 1) / 2);
  RTC_DCHECK_GE(stride_v, (width + 1) / 2);
}

I422Buffer::~I422Buffer() {}

// static
rtc::scoped_refptr<I422Buffer> I422Buffer::Create(int width, int height) {
  return rtc::make_ref_counted<I422Buffer>(width, height);
}

// static
rtc::scoped_refptr<I422Buffer> I422Buffer::Create(int width,
                                                  int height,
                                                  int stride_y,
                                                  int stride_u,
                                                  int stride_v) {
  return rtc::make_ref_counted<I422Buffer>(width, height, stride_y, stride_u,
                                           stride_v);
}

// static
rtc::scoped_refptr<I422Buffer> I422Buffer::Copy(
    const I422BufferInterface& source) {
  return Copy(source.width(), source.height(), source.DataY(), source.StrideY(),
              source.DataU(), source.StrideU(), source.DataV(),
              source.StrideV());
}

// static
rtc::scoped_refptr<I422Buffer> I422Buffer::Copy(
    const I420BufferInterface& source) {
  const int width = source.width();
  const int height = source.height();
  rtc::scoped_refptr<I422Buffer> buffer = Create(width, height);
  RTC_CHECK_EQ(
      0, libyuv::I420ToI422(
             source.DataY(), source.StrideY(), source.DataU(), source.StrideU(),
             source.DataV(), source.StrideV(), buffer->MutableDataY(),
             buffer->StrideY(), buffer->MutableDataU(), buffer->StrideU(),
             buffer->MutableDataV(), buffer->StrideV(), width, height));
  return buffer;
}

// static
rtc::scoped_refptr<I422Buffer> I422Buffer::Copy(int width,
                                                int height,
                                                const uint8_t* data_y,
                                                int stride_y,
                                                const uint8_t* data_u,
                                                int stride_u,
                                                const uint8_t* data_v,
                                                int stride_v) {
  // Note: May use different strides than the input data.
  rtc::scoped_refptr<I422Buffer> buffer = Create(width, height);
  RTC_CHECK_EQ(0, libyuv::I422Copy(data_y, stride_y, data_u, stride_u, data_v,
                                   stride_v, buffer->MutableDataY(),
                                   buffer->StrideY(), buffer->MutableDataU(),
                                   buffer->StrideU(), buffer->MutableDataV(),
                                   buffer->StrideV(), width, height));
  return buffer;
}

// static
rtc::scoped_refptr<I422Buffer> I422Buffer::Rotate(
    const I422BufferInterface& src,
    VideoRotation rotation) {
  RTC_CHECK(src.DataY());
  RTC_CHECK(src.DataU());
  RTC_CHECK(src.DataV());

  int rotated_width = src.width();
  int rotated_height = src.height();
  if (rotation == webrtc::kVideoRotation_90 ||
      rotation == webrtc::kVideoRotation_270) {
    std::swap(rotated_width, rotated_height);
  }

  rtc::scoped_refptr<webrtc::I422Buffer> buffer =
      I422Buffer::Create(rotated_width, rotated_height);

  RTC_CHECK_EQ(
      0,
      I422Rotate(src.DataY(), src.StrideY(), src.DataU(), src.StrideU(),
                 src.DataV(), src.StrideV(), buffer->MutableDataY(),
                 buffer->StrideY(), buffer->MutableDataU(), buffer->StrideU(),
                 buffer->MutableDataV(), buffer->StrideV(), src.width(),
                 src.height(), static_cast<libyuv::RotationMode>(rotation)));

  return buffer;
}

rtc::scoped_refptr<I420BufferInterface> I422Buffer::ToI420() {
  rtc::scoped_refptr<I420Buffer> i420_buffer =
      I420Buffer::Create(width(), height());
  libyuv::I422ToI420(DataY(), StrideY(), DataU(), StrideU(), DataV(), StrideV(),
                     i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                     i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                     i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                     width(), height());
  return i420_buffer;
}

void I422Buffer::InitializeData() {
  memset(data_.get(), 0,
         I422DataSize(height_, stride_y_, stride_u_, stride_v_));
}

int I422Buffer::width() const {
  return width_;
}

int I422Buffer::height() const {
  return height_;
}

const uint8_t* I422Buffer::DataY() const {
  return data_.get();
}
const uint8_t* I422Buffer::DataU() const {
  return data_.get() + stride_y_ * height_;
}
const uint8_t* I422Buffer::DataV() const {
  return data_.get() + stride_y_ * height_ + stride_u_ * height_;
}

int I422Buffer::StrideY() const {
  return stride_y_;
}
int I422Buffer::StrideU() const {
  return stride_u_;
}
int I422Buffer::StrideV() const {
  return stride_v_;
}

uint8_t* I422Buffer::MutableDataY() {
  return const_cast<uint8_t*>(DataY());
}
uint8_t* I422Buffer::MutableDataU() {
  return const_cast<uint8_t*>(DataU());
}
uint8_t* I422Buffer::MutableDataV() {
  return const_cast<uint8_t*>(DataV());
}

void I422Buffer::CropAndScaleFrom(const I422BufferInterface& src,
                                  int offset_x,
                                  int offset_y,
                                  int crop_width,
                                  int crop_height) {
  RTC_CHECK_LE(crop_width, src.width());
  RTC_CHECK_LE(crop_height, src.height());
  RTC_CHECK_LE(crop_width + offset_x, src.width());
  RTC_CHECK_LE(crop_height + offset_y, src.height());
  RTC_CHECK_GE(offset_x, 0);
  RTC_CHECK_GE(offset_y, 0);

  // Make sure offset is even so that u/v plane becomes aligned.
  const int uv_offset_x = offset_x / 2;
  const int uv_offset_y = offset_y;
  offset_x = uv_offset_x * 2;

  const uint8_t* y_plane = src.DataY() + src.StrideY() * offset_y + offset_x;
  const uint8_t* u_plane =
      src.DataU() + src.StrideU() * uv_offset_y + uv_offset_x;
  const uint8_t* v_plane =
      src.DataV() + src.StrideV() * uv_offset_y + uv_offset_x;
  int res = I422Scale(y_plane, src.StrideY(), u_plane, src.StrideU(), v_plane,
                      src.StrideV(), crop_width, crop_height, MutableDataY(),
                      StrideY(), MutableDataU(), StrideU(), MutableDataV(),
                      StrideV(), width(), height(), libyuv::kFilterBox);

  RTC_DCHECK_EQ(res, 0);
}

}  // namespace webrtc
