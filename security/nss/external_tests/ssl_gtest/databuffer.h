/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef databuffer_h__
#define databuffer_h__

class DataBuffer {
 public:
  DataBuffer() : data_(nullptr), len_(0) {}
  DataBuffer(const uint8_t *data, size_t len) : data_(nullptr), len_(0) {
    Assign(data, len);
  }
  ~DataBuffer() { delete[] data_; }

  void Assign(const uint8_t *data, size_t len) {
    Allocate(len);
    memcpy(static_cast<void *>(data_), static_cast<const void *>(data), len);
  }

  void Allocate(size_t len) {
    delete[] data_;
    data_ = new unsigned char[len ? len : 1];  // Don't depend on new [0].
    len_ = len;
  }

  const uint8_t *data() const { return data_; }
  uint8_t *data() { return data_; }
  size_t len() const { return len_; }
  const bool empty() const { return len_ != 0; }

 private:
  uint8_t *data_;
  size_t len_;
};

#endif
