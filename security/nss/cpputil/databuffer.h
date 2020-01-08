/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef databuffer_h__
#define databuffer_h__

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace nss_test {

class DataBuffer {
 public:
  DataBuffer() : data_(nullptr), len_(0) {}
  DataBuffer(const uint8_t* d, size_t l) : data_(nullptr), len_(0) {
    Assign(d, l);
  }
  DataBuffer(const DataBuffer& other) : data_(nullptr), len_(0) {
    Assign(other);
  }
  explicit DataBuffer(size_t l) : data_(nullptr), len_(0) { Allocate(l); }
  ~DataBuffer() { delete[] data_; }

  DataBuffer& operator=(const DataBuffer& other) {
    if (&other != this) {
      Assign(other);
    }
    return *this;
  }

  void Allocate(size_t l) {
    delete[] data_;
    data_ = new uint8_t[l ? l : 1]();  // Don't depend on new [0].
    len_ = l;
  }

  void Truncate(size_t l) { len_ = (std::min)(len_, l); }

  void Assign(const DataBuffer& other) { Assign(other.data(), other.len()); }

  void Assign(const uint8_t* d, size_t l);

  // Write will do a new allocation and expand the size of the buffer if needed.
  // Returns the offset of the end of the write.
  size_t Write(size_t index, const uint8_t* val, size_t count);
  size_t Write(size_t index, const DataBuffer& buf) {
    return Write(index, buf.data(), buf.len());
  }

  // Write an integer, also performing host-to-network order conversion.
  // Returns the offset of the end of the write.
  size_t Write(size_t index, uint32_t val, size_t count);

  // Starting at |index|, remove |remove| bytes and replace them with the
  // contents of |buf|.
  void Splice(const DataBuffer& buf, size_t index, size_t remove = 0) {
    Splice(buf.data(), buf.len(), index, remove);
  }

  void Splice(const uint8_t* ins, size_t ins_len, size_t index,
              size_t remove = 0);
  void Append(const DataBuffer& buf) { Splice(buf, len_); }

  bool Read(size_t index, size_t count, uint64_t* val) const;
  bool Read(size_t index, size_t count, uint32_t* val) const;

  const uint8_t* data() const { return data_; }
  uint8_t* data() { return data_; }
  size_t len() const { return len_; }
  bool empty() const { return len_ == 0; }

  static void SetLogLimit(size_t limit);
  friend std::ostream& operator<<(std::ostream& stream, const DataBuffer& buf);

 private:
  static size_t logging_limit;
  uint8_t* data_;
  size_t len_;
};

inline std::ostream& operator<<(std::ostream& stream, const DataBuffer& buf) {
  stream << "[" << buf.len() << "] ";
  for (size_t i = 0; i < buf.len(); ++i) {
    if (i >= DataBuffer::logging_limit) {
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
