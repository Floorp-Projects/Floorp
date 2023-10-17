// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_BASE_COMMON_H_
#define LIB_JXL_BASE_COMMON_H_

// Shared constants and helper functions.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "lib/jxl/base/compiler_specific.h"

namespace jxl {
// Some enums and typedefs used by more than one header file.

constexpr size_t kBitsPerByte = 8;  // more clear than CHAR_BIT

constexpr inline size_t RoundUpBitsToByteMultiple(size_t bits) {
  return (bits + 7) & ~size_t(7);
}

constexpr inline size_t RoundUpToBlockDim(size_t dim) {
  return (dim + 7) & ~size_t(7);
}

static inline bool JXL_MAYBE_UNUSED SafeAdd(const uint64_t a, const uint64_t b,
                                            uint64_t& sum) {
  sum = a + b;
  return sum >= a;  // no need to check b - either sum >= both or < both.
}

template <typename T1, typename T2>
constexpr inline T1 DivCeil(T1 a, T2 b) {
  return (a + b - 1) / b;
}

// Works for any `align`; if a power of two, compiler emits ADD+AND.
constexpr inline size_t RoundUpTo(size_t what, size_t align) {
  return DivCeil(what, align) * align;
}

constexpr double kPi = 3.14159265358979323846264338327950288;

// Reasonable default for sRGB, matches common monitors. We map white to this
// many nits (cd/m^2) by default. Butteraugli was tuned for 250 nits, which is
// very close.
// NB: This constant is not very "base", but it is shared between modules.
static constexpr float kDefaultIntensityTarget = 255;

template <typename T>
constexpr T Pi(T multiplier) {
  return static_cast<T>(multiplier * kPi);
}

// Prior to C++14 (i.e. C++11): provide our own make_unique
#if __cplusplus < 201402L
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

template <typename T>
JXL_INLINE T Clamp1(T val, T low, T hi) {
  return val < low ? low : val > hi ? hi : val;
}

// conversion from integer to string.
template <typename T>
std::string ToString(T n) {
  char data[32] = {};
  if (T(0.1) != T(0)) {
    // float
    snprintf(data, sizeof(data), "%g", static_cast<double>(n));
  } else if (T(-1) > T(0)) {
    // unsigned
    snprintf(data, sizeof(data), "%llu", static_cast<unsigned long long>(n));
  } else {
    // signed
    snprintf(data, sizeof(data), "%lld", static_cast<long long>(n));
  }
  return data;
}

}  // namespace jxl

#endif  // LIB_JXL_BASE_COMMON_H_
