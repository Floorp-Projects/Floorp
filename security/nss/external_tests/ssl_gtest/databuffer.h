/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef databuffer_h__
#define databuffer_h__

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

extern bool g_ssl_gtest_verbose;

namespace nss_test {

class DataBuffer {
 public:
  DataBuffer() : data_(nullptr), len_(0) {}
  DataBuffer(const uint8_t* data, size_t len) : data_(nullptr), len_(0) {
    Assign(data, len);
  }
  explicit DataBuffer(const DataBuffer& other) : data_(nullptr), len_(0) {
    Assign(other);
  }
  ~DataBuffer() { delete[] data_; }

  DataBuffer& operator=(const DataBuffer& other) {
    if (&other != this) {
      Assign(other);
    }
    return *this;
  }

  void Allocate(size_t len) {
    delete[] data_;
    data_ = new uint8_t[len ? len : 1];  // Don't depend on new [0].
    len_ = len;
  }

  void Truncate(size_t len) { len_ = std::min(len_, len); }

  void Assign(const DataBuffer& other) { Assign(other.data(), other.len()); }

  void Assign(const uint8_t* data, size_t len) {
    if (data) {
      Allocate(len);
      memcpy(static_cast<void*>(data_), static_cast<const void*>(data), len);
    } else {
      assert(len == 0);
      data_ = nullptr;
      len_ = 0;
    }
  }

  // Write will do a new allocation and expand the size of the buffer if needed.
  // Returns the offset of the end of the write.
  size_t Write(size_t index, const uint8_t* val, size_t count) {
    if (index + count > len_) {
      size_t newlen = index + count;
      uint8_t* tmp = new uint8_t[newlen];  // Always > 0.
      memcpy(static_cast<void*>(tmp), static_cast<const void*>(data_), len_);
      if (index > len_) {
        memset(static_cast<void*>(tmp + len_), 0, index - len_);
      }
      delete[] data_;
      data_ = tmp;
      len_ = newlen;
    }
    memcpy(static_cast<void*>(data_ + index), static_cast<const void*>(val),
           count);
    return index + count;
  }

  size_t Write(size_t index, const DataBuffer& buf) {
    return Write(index, buf.data(), buf.len());
  }

  // Write an integer, also performing host-to-network order conversion.
  // Returns the offset of the end of the write.
  size_t Write(size_t index, uint32_t val, size_t count) {
    assert(count <= sizeof(uint32_t));
    uint32_t nvalue = htonl(val);
    auto* addr = reinterpret_cast<const uint8_t*>(&nvalue);
    return Write(index, addr + sizeof(uint32_t) - count, count);
  }

  // This can't use the same trick as Write(), since we might be reading from a
  // smaller data source.
  bool Read(size_t index, size_t count, uint32_t* val) const {
    assert(count < sizeof(uint32_t));
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

  // Starting at |index|, remove |remove| bytes and replace them with the
  // contents of |buf|.
  void Splice(const DataBuffer& buf, size_t index, size_t remove = 0) {
    Splice(buf.data(), buf.len(), index, remove);
  }

  void Splice(const uint8_t* ins, size_t ins_len, size_t index,
              size_t remove = 0) {
    uint8_t* old_value = data_;
    size_t old_len = len_;

    // The amount of stuff remaining from the tail of the old.
    size_t tail_len = old_len - std::min(old_len, index + remove);
    // The new length: the head of the old, the new, and the tail of the old.
    len_ = index + ins_len + tail_len;
    data_ = new uint8_t[len_ ? len_ : 1];

    // The head of the old.
    Write(0, old_value, std::min(old_len, index));
    // Maybe a gap.
    if (index > old_len) {
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

  void Append(const DataBuffer& buf) { Splice(buf, len_); }

  const uint8_t* data() const { return data_; }
  uint8_t* data() { return data_; }
  size_t len() const { return len_; }
  bool empty() const { return len_ == 0; }

 private:
  uint8_t* data_;
  size_t len_;
};

static const size_t kMaxBufferPrint = 32;

inline std::ostream& operator<<(std::ostream& stream, const DataBuffer& buf) {
  stream << "[" << buf.len() << "] ";
  for (size_t i = 0; i < buf.len(); ++i) {
    if (!g_ssl_gtest_verbose && i >= kMaxBufferPrint) {
      stream << "...";
      break;
    }
    stream << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<unsigned>(buf.data()[i]);
  }
  stream << std::dec;
  return stream;
}

inline bool operator==(const DataBuffer& a, const DataBuffer& b) {
  return (a.empty() && b.empty()) ||
         (a.len() == b.len() && 0 == memcmp(a.data(), b.data(), a.len()));
}

inline bool operator!=(const DataBuffer& a, const DataBuffer& b) {
  return !(a == b);
}

}  // namespace nss_test

#endif
