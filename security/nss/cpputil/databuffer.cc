/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "databuffer.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#if defined(WIN32) || defined(WIN64)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace nss_test {

void DataBuffer::Assign(const uint8_t* d, size_t l) {
  if (d) {
    Allocate(l);
    memcpy(static_cast<void*>(data_), static_cast<const void*>(d), l);
  } else {
    assert(l == 0);
    data_ = nullptr;
    len_ = 0;
  }
}

// Write will do a new allocation and expand the size of the buffer if needed.
// Returns the offset of the end of the write.
size_t DataBuffer::Write(size_t index, const uint8_t* val, size_t count) {
  assert(val);
  if (index + count > len_) {
    size_t newlen = index + count;
    uint8_t* tmp = new uint8_t[newlen];  // Always > 0.
    if (data_) {
      memcpy(static_cast<void*>(tmp), static_cast<const void*>(data_), len_);
    }
    if (index > len_) {
      memset(static_cast<void*>(tmp + len_), 0, index - len_);
    }
    delete[] data_;
    data_ = tmp;
    len_ = newlen;
  }
  if (data_) {
    memcpy(static_cast<void*>(data_ + index), static_cast<const void*>(val),
           count);
  }
  return index + count;
}

// Write an integer, also performing host-to-network order conversion.
// Returns the offset of the end of the write.
size_t DataBuffer::Write(size_t index, uint32_t val, size_t count) {
  assert(count <= sizeof(uint32_t));
  uint32_t nvalue = htonl(val);
  auto* addr = reinterpret_cast<const uint8_t*>(&nvalue);
  return Write(index, addr + sizeof(uint32_t) - count, count);
}

void DataBuffer::Splice(const uint8_t* ins, size_t ins_len, size_t index,
                        size_t remove) {
  assert(ins);
  uint8_t* old_value = data_;
  size_t old_len = len_;

  // The amount of stuff remaining from the tail of the old.
  size_t tail_len = old_len - (std::min)(old_len, index + remove);
  // The new length: the head of the old, the new, and the tail of the old.
  len_ = index + ins_len + tail_len;
  data_ = new uint8_t[len_ ? len_ : 1];

  // The head of the old.
  if (old_value) {
    Write(0, old_value, (std::min)(old_len, index));
  }
  // Maybe a gap.
  if (old_value && index > old_len) {
    memset(old_value + index, 0, index - old_len);
  }
  // The new.
  Write(index, ins, ins_len);
  // The tail of the old.
  if (tail_len > 0) {
    Write(index + ins_len, old_value + index + remove, tail_len);
  }

  delete[] old_value;
}

// This can't use the same trick as Write(), since we might be reading from a
// smaller data source.
bool DataBuffer::Read(size_t index, size_t count, uint64_t* val) const {
  assert(count <= sizeof(uint64_t));
  assert(val);
  if ((index > len()) || (count > (len() - index))) {
    return false;
  }
  *val = 0;
  for (size_t i = 0; i < count; ++i) {
    *val = (*val << 8) | data()[index + i];
  }
  return true;
}

bool DataBuffer::Read(size_t index, size_t count, uint32_t* val) const {
  assert(count <= sizeof(uint32_t));
  uint64_t tmp;

  if (!Read(index, count, &tmp)) {
    return false;
  }
  *val = tmp & 0xffffffff;
  return true;
}

size_t DataBuffer::logging_limit = 32;

/* static */ void DataBuffer::SetLogLimit(size_t limit) {
  DataBuffer::logging_limit = limit;
}

}  // namespace nss_test
