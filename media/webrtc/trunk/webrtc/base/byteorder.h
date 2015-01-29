/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_BYTEORDER_H_
#define WEBRTC_BASE_BYTEORDER_H_

#if defined(WEBRTC_POSIX) && !defined(__native_client__)
#include <arpa/inet.h>
#endif

#if defined(WEBRTC_WIN)
#include <stdlib.h>
#endif

#include "webrtc/base/basictypes.h"

namespace rtc {

// Reading and writing of little and big-endian numbers from memory
// TODO: Optimized versions, with direct read/writes of
// integers in host-endian format, when the platform supports it.

inline void Set8(void* memory, size_t offset, uint8 v) {
  static_cast<uint8*>(memory)[offset] = v;
}

inline uint8 Get8(const void* memory, size_t offset) {
  return static_cast<const uint8*>(memory)[offset];
}

inline void SetBE16(void* memory, uint16 v) {
  Set8(memory, 0, static_cast<uint8>(v >> 8));
  Set8(memory, 1, static_cast<uint8>(v >> 0));
}

inline void SetBE32(void* memory, uint32 v) {
  Set8(memory, 0, static_cast<uint8>(v >> 24));
  Set8(memory, 1, static_cast<uint8>(v >> 16));
  Set8(memory, 2, static_cast<uint8>(v >> 8));
  Set8(memory, 3, static_cast<uint8>(v >> 0));
}

inline void SetBE64(void* memory, uint64 v) {
  Set8(memory, 0, static_cast<uint8>(v >> 56));
  Set8(memory, 1, static_cast<uint8>(v >> 48));
  Set8(memory, 2, static_cast<uint8>(v >> 40));
  Set8(memory, 3, static_cast<uint8>(v >> 32));
  Set8(memory, 4, static_cast<uint8>(v >> 24));
  Set8(memory, 5, static_cast<uint8>(v >> 16));
  Set8(memory, 6, static_cast<uint8>(v >> 8));
  Set8(memory, 7, static_cast<uint8>(v >> 0));
}

inline uint16 GetBE16(const void* memory) {
  return static_cast<uint16>((Get8(memory, 0) << 8) |
                             (Get8(memory, 1) << 0));
}

inline uint32 GetBE32(const void* memory) {
  return (static_cast<uint32>(Get8(memory, 0)) << 24) |
      (static_cast<uint32>(Get8(memory, 1)) << 16) |
      (static_cast<uint32>(Get8(memory, 2)) << 8) |
      (static_cast<uint32>(Get8(memory, 3)) << 0);
}

inline uint64 GetBE64(const void* memory) {
  return (static_cast<uint64>(Get8(memory, 0)) << 56) |
      (static_cast<uint64>(Get8(memory, 1)) << 48) |
      (static_cast<uint64>(Get8(memory, 2)) << 40) |
      (static_cast<uint64>(Get8(memory, 3)) << 32) |
      (static_cast<uint64>(Get8(memory, 4)) << 24) |
      (static_cast<uint64>(Get8(memory, 5)) << 16) |
      (static_cast<uint64>(Get8(memory, 6)) << 8) |
      (static_cast<uint64>(Get8(memory, 7)) << 0);
}

inline void SetLE16(void* memory, uint16 v) {
  Set8(memory, 0, static_cast<uint8>(v >> 0));
  Set8(memory, 1, static_cast<uint8>(v >> 8));
}

inline void SetLE32(void* memory, uint32 v) {
  Set8(memory, 0, static_cast<uint8>(v >> 0));
  Set8(memory, 1, static_cast<uint8>(v >> 8));
  Set8(memory, 2, static_cast<uint8>(v >> 16));
  Set8(memory, 3, static_cast<uint8>(v >> 24));
}

inline void SetLE64(void* memory, uint64 v) {
  Set8(memory, 0, static_cast<uint8>(v >> 0));
  Set8(memory, 1, static_cast<uint8>(v >> 8));
  Set8(memory, 2, static_cast<uint8>(v >> 16));
  Set8(memory, 3, static_cast<uint8>(v >> 24));
  Set8(memory, 4, static_cast<uint8>(v >> 32));
  Set8(memory, 5, static_cast<uint8>(v >> 40));
  Set8(memory, 6, static_cast<uint8>(v >> 48));
  Set8(memory, 7, static_cast<uint8>(v >> 56));
}

inline uint16 GetLE16(const void* memory) {
  return static_cast<uint16>((Get8(memory, 0) << 0) |
                             (Get8(memory, 1) << 8));
}

inline uint32 GetLE32(const void* memory) {
  return (static_cast<uint32>(Get8(memory, 0)) << 0) |
      (static_cast<uint32>(Get8(memory, 1)) << 8) |
      (static_cast<uint32>(Get8(memory, 2)) << 16) |
      (static_cast<uint32>(Get8(memory, 3)) << 24);
}

inline uint64 GetLE64(const void* memory) {
  return (static_cast<uint64>(Get8(memory, 0)) << 0) |
      (static_cast<uint64>(Get8(memory, 1)) << 8) |
      (static_cast<uint64>(Get8(memory, 2)) << 16) |
      (static_cast<uint64>(Get8(memory, 3)) << 24) |
      (static_cast<uint64>(Get8(memory, 4)) << 32) |
      (static_cast<uint64>(Get8(memory, 5)) << 40) |
      (static_cast<uint64>(Get8(memory, 6)) << 48) |
      (static_cast<uint64>(Get8(memory, 7)) << 56);
}

// Check if the current host is big endian.
inline bool IsHostBigEndian() {
  static const int number = 1;
  return 0 == *reinterpret_cast<const char*>(&number);
}

inline uint16 HostToNetwork16(uint16 n) {
  uint16 result;
  SetBE16(&result, n);
  return result;
}

inline uint32 HostToNetwork32(uint32 n) {
  uint32 result;
  SetBE32(&result, n);
  return result;
}

inline uint64 HostToNetwork64(uint64 n) {
  uint64 result;
  SetBE64(&result, n);
  return result;
}

inline uint16 NetworkToHost16(uint16 n) {
  return GetBE16(&n);
}

inline uint32 NetworkToHost32(uint32 n) {
  return GetBE32(&n);
}

inline uint64 NetworkToHost64(uint64 n) {
  return GetBE64(&n);
}

}  // namespace rtc

#endif  // WEBRTC_BASE_BYTEORDER_H_
