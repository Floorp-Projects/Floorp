/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/plane.h"

#include <string.h>  // memcpy

#include <algorithm>  // swap

namespace webrtc {

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment =  64;

Plane::Plane()
    : allocated_size_(0),
      plane_size_(0),
      stride_(0) {}

Plane::~Plane() {}

int Plane::CreateEmptyPlane(int allocated_size, int stride, int plane_size) {
  if (allocated_size < 1 || stride < 1 || plane_size < 1)
    return -1;
  stride_ = stride;
  if (MaybeResize(allocated_size) < 0)
    return -1;
  plane_size_ = plane_size;
  return 0;
}

int Plane::MaybeResize(int new_size) {
  if (new_size <= 0)
    return -1;
  if (new_size <= allocated_size_)
    return 0;
  scoped_ptr<uint8_t, AlignedFreeDeleter> new_buffer(static_cast<uint8_t*>(
      AlignedMalloc(new_size, kBufferAlignment)));
  if (buffer_.get()) {
    memcpy(new_buffer.get(), buffer_.get(), plane_size_);
  }
  buffer_.reset(new_buffer.release());
  allocated_size_ = new_size;
  return 0;
}

int Plane::Copy(const Plane& plane) {
  if (MaybeResize(plane.allocated_size_) < 0)
    return -1;
  if (plane.buffer_.get())
    memcpy(buffer_.get(), plane.buffer_.get(), plane.plane_size_);
  stride_ = plane.stride_;
  plane_size_ = plane.plane_size_;
  return 0;
}

int Plane::Copy(int size, int stride, const uint8_t* buffer) {
  if (MaybeResize(size) < 0)
    return -1;
  memcpy(buffer_.get(), buffer, size);
  plane_size_ = size;
  stride_ = stride;
  return 0;
}

void Plane::Swap(Plane& plane) {
  std::swap(stride_, plane.stride_);
  std::swap(allocated_size_, plane.allocated_size_);
  std::swap(plane_size_, plane.plane_size_);
  buffer_.swap(plane.buffer_);
}

}  // namespace webrtc
