/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/bytebuffer.h"

#include <assert.h>
#include <string.h>

#include <algorithm>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/byteorder.h"

namespace rtc {

static const int DEFAULT_SIZE = 4096;

ByteBuffer::ByteBuffer() {
  Construct(NULL, DEFAULT_SIZE, ORDER_NETWORK);
}

ByteBuffer::ByteBuffer(ByteOrder byte_order) {
  Construct(NULL, DEFAULT_SIZE, byte_order);
}

ByteBuffer::ByteBuffer(const char* bytes, size_t len) {
  Construct(bytes, len, ORDER_NETWORK);
}

ByteBuffer::ByteBuffer(const char* bytes, size_t len, ByteOrder byte_order) {
  Construct(bytes, len, byte_order);
}

ByteBuffer::ByteBuffer(const char* bytes) {
  Construct(bytes, strlen(bytes), ORDER_NETWORK);
}

void ByteBuffer::Construct(const char* bytes, size_t len,
                           ByteOrder byte_order) {
  version_ = 0;
  start_ = 0;
  size_ = len;
  byte_order_ = byte_order;
  bytes_ = new char[size_];

  if (bytes) {
    end_ = len;
    memcpy(bytes_, bytes, end_);
  } else {
    end_ = 0;
  }
}

ByteBuffer::~ByteBuffer() {
  delete[] bytes_;
}

bool ByteBuffer::ReadUInt8(uint8* val) {
  if (!val) return false;

  return ReadBytes(reinterpret_cast<char*>(val), 1);
}

bool ByteBuffer::ReadUInt16(uint16* val) {
  if (!val) return false;

  uint16 v;
  if (!ReadBytes(reinterpret_cast<char*>(&v), 2)) {
    return false;
  } else {
    *val = (byte_order_ == ORDER_NETWORK) ? NetworkToHost16(v) : v;
    return true;
  }
}

bool ByteBuffer::ReadUInt24(uint32* val) {
  if (!val) return false;

  uint32 v = 0;
  char* read_into = reinterpret_cast<char*>(&v);
  if (byte_order_ == ORDER_NETWORK || IsHostBigEndian()) {
    ++read_into;
  }

  if (!ReadBytes(read_into, 3)) {
    return false;
  } else {
    *val = (byte_order_ == ORDER_NETWORK) ? NetworkToHost32(v) : v;
    return true;
  }
}

bool ByteBuffer::ReadUInt32(uint32* val) {
  if (!val) return false;

  uint32 v;
  if (!ReadBytes(reinterpret_cast<char*>(&v), 4)) {
    return false;
  } else {
    *val = (byte_order_ == ORDER_NETWORK) ? NetworkToHost32(v) : v;
    return true;
  }
}

bool ByteBuffer::ReadUInt64(uint64* val) {
  if (!val) return false;

  uint64 v;
  if (!ReadBytes(reinterpret_cast<char*>(&v), 8)) {
    return false;
  } else {
    *val = (byte_order_ == ORDER_NETWORK) ? NetworkToHost64(v) : v;
    return true;
  }
}

bool ByteBuffer::ReadString(std::string* val, size_t len) {
  if (!val) return false;

  if (len > Length()) {
    return false;
  } else {
    val->append(bytes_ + start_, len);
    start_ += len;
    return true;
  }
}

bool ByteBuffer::ReadBytes(char* val, size_t len) {
  if (len > Length()) {
    return false;
  } else {
    memcpy(val, bytes_ + start_, len);
    start_ += len;
    return true;
  }
}

void ByteBuffer::WriteUInt8(uint8 val) {
  WriteBytes(reinterpret_cast<const char*>(&val), 1);
}

void ByteBuffer::WriteUInt16(uint16 val) {
  uint16 v = (byte_order_ == ORDER_NETWORK) ? HostToNetwork16(val) : val;
  WriteBytes(reinterpret_cast<const char*>(&v), 2);
}

void ByteBuffer::WriteUInt24(uint32 val) {
  uint32 v = (byte_order_ == ORDER_NETWORK) ? HostToNetwork32(val) : val;
  char* start = reinterpret_cast<char*>(&v);
  if (byte_order_ == ORDER_NETWORK || IsHostBigEndian()) {
    ++start;
  }
  WriteBytes(start, 3);
}

void ByteBuffer::WriteUInt32(uint32 val) {
  uint32 v = (byte_order_ == ORDER_NETWORK) ? HostToNetwork32(val) : val;
  WriteBytes(reinterpret_cast<const char*>(&v), 4);
}

void ByteBuffer::WriteUInt64(uint64 val) {
  uint64 v = (byte_order_ == ORDER_NETWORK) ? HostToNetwork64(val) : val;
  WriteBytes(reinterpret_cast<const char*>(&v), 8);
}

void ByteBuffer::WriteString(const std::string& val) {
  WriteBytes(val.c_str(), val.size());
}

void ByteBuffer::WriteBytes(const char* val, size_t len) {
  memcpy(ReserveWriteBuffer(len), val, len);
}

char* ByteBuffer::ReserveWriteBuffer(size_t len) {
  if (Length() + len > Capacity())
    Resize(Length() + len);

  char* start = bytes_ + end_;
  end_ += len;
  return start;
}

void ByteBuffer::Resize(size_t size) {
  size_t len = std::min(end_ - start_, size);
  if (size <= size_) {
    // Don't reallocate, just move data backwards
    memmove(bytes_, bytes_ + start_, len);
  } else {
    // Reallocate a larger buffer.
    size_ = std::max(size, 3 * size_ / 2);
    char* new_bytes = new char[size_];
    memcpy(new_bytes, bytes_ + start_, len);
    delete [] bytes_;
    bytes_ = new_bytes;
  }
  start_ = 0;
  end_ = len;
  ++version_;
}

bool ByteBuffer::Consume(size_t size) {
  if (size > Length())
    return false;
  start_ += size;
  return true;
}

ByteBuffer::ReadPosition ByteBuffer::GetReadPosition() const {
  return ReadPosition(start_, version_);
}

bool ByteBuffer::SetReadPosition(const ReadPosition &position) {
  if (position.version_ != version_) {
    return false;
  }
  start_ = position.start_;
  return true;
}

void ByteBuffer::Clear() {
  memset(bytes_, 0, size_);
  start_ = end_ = 0;
  ++version_;
}

}  // namespace rtc
