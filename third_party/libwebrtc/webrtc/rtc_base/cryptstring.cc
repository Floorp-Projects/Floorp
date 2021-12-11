/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cryptstring.h"

namespace rtc {

size_t EmptyCryptStringImpl::GetLength() const {
  return 0;
}

void EmptyCryptStringImpl::CopyTo(char* dest, bool nullterminate) const {
  if (nullterminate) {
    *dest = '\0';
  }
}

std::string EmptyCryptStringImpl::UrlEncode() const {
  return "";
}

CryptStringImpl* EmptyCryptStringImpl::Copy() const {
  return new EmptyCryptStringImpl();
}

void EmptyCryptStringImpl::CopyRawTo(std::vector<unsigned char>* dest) const {
  dest->clear();
}

CryptString::CryptString() : impl_(new EmptyCryptStringImpl()) {
}

CryptString::CryptString(const CryptString& other)
    : impl_(other.impl_->Copy()) {
}

CryptString::CryptString(const CryptStringImpl& impl) : impl_(impl.Copy()) {
}

CryptString::~CryptString() = default;

size_t InsecureCryptStringImpl::GetLength() const {
  return password_.size();
}

void InsecureCryptStringImpl::CopyTo(char* dest, bool nullterminate) const {
  memcpy(dest, password_.data(), password_.size());
  if (nullterminate)
    dest[password_.size()] = 0;
}

std::string InsecureCryptStringImpl::UrlEncode() const {
  return password_;
}

CryptStringImpl* InsecureCryptStringImpl::Copy() const {
  InsecureCryptStringImpl* copy = new InsecureCryptStringImpl;
  copy->password() = password_;
  return copy;
}

void InsecureCryptStringImpl::CopyRawTo(
    std::vector<unsigned char>* dest) const {
  dest->resize(password_.size());
  memcpy(&dest->front(), password_.data(), password_.size());
}

};  // namespace rtc
