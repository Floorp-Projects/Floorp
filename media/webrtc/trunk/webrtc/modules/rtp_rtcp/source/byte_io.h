/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_BYTE_IO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_BYTE_IO_H_


// This file contains classes for reading and writing integer types from/to
// byte array representations. Signed/unsigned, partial (whole byte) sizes,
// and big/little endian byte order is all supported.
//
// Usage examples:
//
// uint8_t* buffer = ...;
//
// // Read an unsigned 4 byte integer in big endian format
// uint32_t val = ByteReader<uint32_t>::ReadBigEndian(buffer);
//
// // Read a signed 24-bit (3 byte) integer in little endian format
// int32_t val = ByteReader<int32_t, 3>::ReadLittle(buffer);
//
// // Write an unsigned 8 byte integer in little endian format
// ByteWriter<uint64_t>::WriteLittleEndian(buffer, val);
//
// Write an unsigned 40-bit (5 byte) integer in big endian format
// ByteWriter<uint64_t, 5>::WriteBigEndian(buffer, val);
//
// These classes are implemented as recursive templetizations, inteded to make
// it easy for the compiler to completely inline the reading/writing.


#include <limits>

#include "webrtc/typedefs.h"

namespace webrtc {

// Class for reading integers from a sequence of bytes.
// T = type of integer, B = bytes to read, is_signed = true if signed integer
// If is_signed is true and B < sizeof(T), sign extension might be needed
template<typename T, unsigned int B = sizeof(T),
    bool is_signed = std::numeric_limits<T>::is_signed>
class ByteReader {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    if (is_signed && B < sizeof(T)) {
      return SignExtend(InternalReadBigEndian(data));
    }
    return InternalReadBigEndian(data);
  }

  static T ReadLittleEndian(const uint8_t* data) {
    if (is_signed && B < sizeof(T)) {
      return SignExtend(InternalReadLittleEndian(data));
    }
    return InternalReadLittleEndian(data);
  }

 private:
  static T InternalReadBigEndian(const uint8_t* data) {
    T val(0);
    for (unsigned int i = 0; i < B; ++i) {
      val |= static_cast<T>(data[i]) << ((B - 1 - i) * 8);
    }
    return val;
  }

  static T InternalReadLittleEndian(const uint8_t* data) {
    T val(0);
    for (unsigned int i = 0; i < B; ++i) {
      val |= static_cast<T>(data[i]) << (i * 8);
    }
    return val;
  }

  // If number of bytes is less than native data type (eg 24 bit, in int32_t),
  // and the most significant bit of the actual data is set, we must sign
  // extend the remaining byte(s) with ones so that the correct negative
  // number is retained.
  // Ex: 0x810A0B -> 0xFF810A0B, but 0x710A0B -> 0x00710A0B
  static T SignExtend(const T val) {
    uint8_t msb = static_cast<uint8_t>(val >> ((B - 1) * 8));
    if (msb & 0x80) {
      // Sign extension is -1 (all ones) shifted left B bytes.
      // The "B % sizeof(T)"-part is there to avoid compiler warning for
      // shifting the whole size of the data type.
      T sign_extend = (sizeof(T) == B ? 0 :
          (static_cast<T>(-1L) << ((B % sizeof(T)) * 8)));

      return val | sign_extend;
    }
    return val;
  }
};

// Class for writing integers to a sequence of bytes
// T = type of integer, B = bytes to write
template<typename T, unsigned int B = sizeof(T)>
class ByteWriter {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    for (unsigned int i = 0; i < B; ++i) {
      data[i] = val >> ((B - 1 - i) * 8);
    }
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    for (unsigned int i = 0; i < B; ++i) {
      data[i] = val >> (i * 8);
    }
  }
};


// -------- Below follows specializations for B in { 2, 4, 8 } --------


// Specializations for two byte words
template<typename T, bool is_signed>
class ByteReader<T, 2, is_signed> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    return (data[0] << 8) | data[1];
  }

  static T ReadLittleEndian(const uint8_t* data) {
    return data[0] | (data[1] << 8);
  }
};

template<typename T>
class ByteWriter<T, 2> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    data[0] = val >> 8;
    data[1] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    data[0] = val;
    data[1] = val >> 8;
  }
};

// Specializations for four byte words.
template<typename T, bool is_signed>
class ByteReader<T, 4, is_signed> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  }

  static T ReadLittleEndian(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  }
};

// Specializations for four byte words.
template<typename T>
class ByteWriter<T, 4> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    data[0] = val >> 24;
    data[1] = val >> 16;
    data[2] = val >> 8;
    data[3] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    data[0] = val;
    data[1] = val >> 8;
    data[2] = val >> 16;
    data[3] = val >> 24;
  }
};

// Specializations for eight byte words.
template<typename T, bool is_signed>
class ByteReader<T, 8, is_signed> {
 public:
  static T ReadBigEndian(const uint8_t* data) {
    return
        (Get(data, 0) << 56) | (Get(data, 1) << 48) |
        (Get(data, 2) << 40) | (Get(data, 3) << 32) |
        (Get(data, 4) << 24) | (Get(data, 5) << 16) |
        (Get(data, 6) << 8)  |  Get(data, 7);
  }

  static T ReadLittleEndian(const uint8_t* data) {
    return
         Get(data, 0)        | (Get(data, 1) << 8)  |
        (Get(data, 2) << 16) | (Get(data, 3) << 24) |
        (Get(data, 4) << 32) | (Get(data, 5) << 40) |
        (Get(data, 6) << 48) | (Get(data, 7) << 56);
  }

 private:
  inline static T Get(const uint8_t* data, unsigned int index) {
    return static_cast<T>(data[index]);
  }
};

template<typename T>
class ByteWriter<T, 8> {
 public:
  static void WriteBigEndian(uint8_t* data, T val) {
    data[0] = val >> 56;
    data[1] = val >> 48;
    data[2] = val >> 40;
    data[3] = val >> 32;
    data[4] = val >> 24;
    data[5] = val >> 16;
    data[6] = val >> 8;
    data[7] = val;
  }

  static void WriteLittleEndian(uint8_t* data, T val) {
    data[0] = val;
    data[1] = val >> 8;
    data[2] = val >> 16;
    data[3] = val >> 24;
    data[4] = val >> 32;
    data[5] = val >> 40;
    data[6] = val >> 48;
    data[7] = val >> 56;
  }
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_BYTE_IO_H_
