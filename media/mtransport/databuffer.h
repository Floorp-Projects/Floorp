/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef databuffer_h__
#define databuffer_h__
#include <algorithm>
#include <mozilla/UniquePtr.h>
#include <m_cpp_utils.h>
#include <nsISupportsImpl.h>

namespace mozilla {

class DataBuffer {
 public:
  DataBuffer() : data_(nullptr), len_(0), capacity_(0) {}
  DataBuffer(const uint8_t *data, size_t len) {
    Assign(data, len, len);
  }
  DataBuffer(const uint8_t *data, size_t len, size_t capacity) {
    Assign(data, len, capacity);
  }

  // to ensure extra space for expansion
  void Assign(const uint8_t *data, size_t len, size_t capacity) {
    MOZ_RELEASE_ASSERT(len <= capacity);
    Allocate(capacity); // sets len_ = capacity
    memcpy(static_cast<void *>(data_.get()),
           static_cast<const void *>(data), len);
    len_ = len;
  }

  void Allocate(size_t capacity) {
    data_.reset(new uint8_t[capacity ? capacity : 1]);  // Don't depend on new [0].
    len_ = capacity_ = capacity;
  }

  void EnsureCapacity(size_t capacity) {
    if (capacity_ < capacity) {
      uint8_t *new_data = new uint8_t[ capacity ? capacity : 1];
      memcpy(static_cast<void *>(new_data),
             static_cast<const void *>(data_.get()), len_);
      data_.reset(new_data); // after copying!  Deletes old data
      capacity_ = capacity;
    }
  }

  // used when something writes to the buffer (having checked
  // capacity() or used EnsureCapacity()) and increased the length.
  void SetLength(size_t len) {
    MOZ_RELEASE_ASSERT(len <= capacity_);
    len_ = len;
  }

  const uint8_t *data() const { return data_.get(); }
  uint8_t *data() { return data_.get(); }
  size_t len() const { return len_; }
  size_t capacity() const { return capacity_; }

private:
  UniquePtr<uint8_t[]> data_;
  size_t len_;
  size_t capacity_;

  DISALLOW_COPY_ASSIGN(DataBuffer);
};

}

#endif
