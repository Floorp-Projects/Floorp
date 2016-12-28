/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/buffer.h"

#include <cassert>
#include <utility>

namespace rtc {

Buffer::Buffer() : size_(0), capacity_(0), data_(nullptr) {
  assert(IsConsistent());
}

Buffer::Buffer(const Buffer& buf) : Buffer(buf.data(), buf.size()) {
}

Buffer::Buffer(Buffer&& buf)
    : size_(buf.size()),
      capacity_(buf.capacity()),
      data_(std::move(buf.data_)) {
  assert(IsConsistent());
  buf.OnMovedFrom();
}

Buffer::Buffer(size_t size) : Buffer(size, size) {
}

Buffer::Buffer(size_t size, size_t capacity)
    : size_(size),
      capacity_(std::max(size, capacity)),
      data_(new uint8_t[capacity_]) {
  assert(IsConsistent());
}

// Note: The destructor works even if the buffer has been moved from.
Buffer::~Buffer() = default;

};  // namespace rtc
