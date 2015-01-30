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

#include "webrtc/base/scoped_ptr.h"

namespace rtc {

// Basic buffer class, can be grown and shrunk dynamically.
// Unlike std::string/vector, does not initialize data when expanding capacity.
class Buffer {
 public:
  Buffer() {
    Construct(NULL, 0, 0);
  }
  Buffer(const void* data, size_t length) {
    Construct(data, length, length);
  }
  Buffer(const void* data, size_t length, size_t capacity) {
    Construct(data, length, capacity);
  }
  Buffer(const Buffer& buf) {
    Construct(buf.data(), buf.length(), buf.length());
  }

  const char* data() const { return data_.get(); }
  char* data() { return data_.get(); }
  // TODO: should this be size(), like STL?
  size_t length() const { return length_; }
  size_t capacity() const { return capacity_; }

  Buffer& operator=(const Buffer& buf) {
    if (&buf != this) {
      Construct(buf.data(), buf.length(), buf.length());
    }
    return *this;
  }
  bool operator==(const Buffer& buf) const {
    return (length_ == buf.length() &&
            memcmp(data_.get(), buf.data(), length_) == 0);
  }
  bool operator!=(const Buffer& buf) const {
    return !operator==(buf);
  }

  void SetData(const void* data, size_t length) {
    ASSERT(data != NULL || length == 0);
    SetLength(length);
    memcpy(data_.get(), data, length);
  }
  void AppendData(const void* data, size_t length) {
    ASSERT(data != NULL || length == 0);
    size_t old_length = length_;
    SetLength(length_ + length);
    memcpy(data_.get() + old_length, data, length);
  }
  void SetLength(size_t length) {
    SetCapacity(length);
    length_ = length;
  }
  void SetCapacity(size_t capacity) {
    if (capacity > capacity_) {
      rtc::scoped_ptr<char[]> data(new char[capacity]);
      memcpy(data.get(), data_.get(), length_);
      data_.swap(data);
      capacity_ = capacity;
    }
  }

  void TransferTo(Buffer* buf) {
    ASSERT(buf != NULL);
    buf->data_.reset(data_.release());
    buf->length_ = length_;
    buf->capacity_ = capacity_;
    Construct(NULL, 0, 0);
  }

 protected:
  void Construct(const void* data, size_t length, size_t capacity) {
    data_.reset(new char[capacity_ = capacity]);
    SetData(data, length);
  }

  scoped_ptr<char[]> data_;
  size_t length_;
  size_t capacity_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_BUFFER_H_
