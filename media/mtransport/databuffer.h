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
  DataBuffer() : data_(nullptr), len_(0) {}
  DataBuffer(const uint8_t *data, size_t len) {
    Assign(data, len);
  }

  void Assign(const uint8_t *data, size_t len) {
    Allocate(len);
    memcpy(static_cast<void *>(data_.get()),
           static_cast<const void *>(data), len);
  }

  void Allocate(size_t len) {
    data_.reset(new uint8_t[len ? len : 1]);  // Don't depend on new [0].
    len_ = len;
  }

  const uint8_t *data() const { return data_.get(); }
  uint8_t *data() { return data_.get(); }
  size_t len() const { return len_; }
  const bool empty() const { return len_ != 0; }

private:
  UniquePtr<uint8_t[]> data_;
  size_t len_;

  DISALLOW_COPY_ASSIGN(DataBuffer);
};

}

#endif
