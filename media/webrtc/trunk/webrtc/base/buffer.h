/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_BUFFER_H_
#define WEBRTC_BASE_BUFFER_H_

#include <string.h>

#include "webrtc/base/common.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {

// Basic buffer class, can be grown and shrunk dynamically.
// Unlike std::string/vector, does not initialize data when expanding capacity.
class Buffer {
 public:
  Buffer();
  explicit Buffer(size_t size);
  Buffer(const void* data, size_t size);
  Buffer(const void* data, size_t size, size_t capacity);
  Buffer(const Buffer& buf);
  ~Buffer();

  const char* data() const { return data_.get(); }
  char* data() { return data_.get(); }
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }

  // For backwards compatibility. TODO(kwiberg): Remove once Chromium doesn't
  // need it anymore.
  size_t length() const { return size(); }

  Buffer& operator=(const Buffer& buf) {
    if (&buf != this) {
      Construct(buf.data(), buf.size(), buf.size());
    }
    return *this;
  }
  bool operator==(const Buffer& buf) const {
    return (size_ == buf.size() && memcmp(data_.get(), buf.data(), size_) == 0);
  }
  bool operator!=(const Buffer& buf) const {
    return !operator==(buf);
  }

  void SetData(const void* data, size_t size) {
    ASSERT(data != NULL || size == 0);
    SetSize(size);
    memcpy(data_.get(), data, size);
  }
  void AppendData(const void* data, size_t size) {
    ASSERT(data != NULL || size == 0);
    size_t old_size = size_;
    SetSize(size_ + size);
    memcpy(data_.get() + old_size, data, size);
  }
  void SetSize(size_t size) {
    SetCapacity(size);
    size_ = size;
  }
  void SetCapacity(size_t capacity) {
    if (capacity > capacity_) {
      rtc::scoped_ptr<char[]> data(new char[capacity]);
      memcpy(data.get(), data_.get(), size_);
      data_.swap(data);
      capacity_ = capacity;
    }
  }

  void TransferTo(Buffer* buf) {
    ASSERT(buf != NULL);
    buf->data_.reset(data_.release());
    buf->size_ = size_;
    buf->capacity_ = capacity_;
    Construct(NULL, 0, 0);
  }

 protected:
  void Construct(const void* data, size_t size, size_t capacity) {
    data_.reset(new char[capacity_ = capacity]);
    SetData(data, size);
  }

  scoped_ptr<char[]> data_;
  size_t size_;
  size_t capacity_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_BUFFER_H_
