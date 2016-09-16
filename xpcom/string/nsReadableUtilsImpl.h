/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

namespace mozilla {

inline bool IsASCII(char16_t aChar) {
  return (aChar & 0xFF80) == 0;
}

/**
 * Provides a pointer before or equal to |aPtr| that is is suitably aligned.
 */
inline const char16_t* aligned(const char16_t* aPtr, const uintptr_t aMask)
{
  return reinterpret_cast<const char16_t*>(
      reinterpret_cast<const uintptr_t>(aPtr) & ~aMask);
}

/**
 * Structures for word-sized vectorization of ASCII checking for UTF-16
 * strings.
 */
template<size_t size> struct NonASCIIParameters;
template<> struct NonASCIIParameters<4> {
  static inline size_t mask() { return 0xff80ff80; }
  static inline uintptr_t alignMask() { return 0x3; }
  static inline size_t numUnicharsPerWord() { return 2; }
};

template<> struct NonASCIIParameters<8> {
  static inline size_t mask() {
    static const uint64_t maskAsUint64 = UINT64_C(0xff80ff80ff80ff80);
    // We have to explicitly cast this 64-bit value to a size_t, or else
    // compilers for 32-bit platforms will warn about it being too large to fit
    // in the size_t return type. (Fortunately, this code isn't actually
    // invoked on 32-bit platforms -- they'll use the <4> specialization above.
    // So it is, in fact, OK that this value is too large for a 32-bit size_t.)
    return (size_t)maskAsUint64;
  }
  static inline uintptr_t alignMask() { return 0x7; }
  static inline size_t numUnicharsPerWord() { return 4; }
};

namespace SSE2 {

int32_t FirstNonASCII(const char16_t* aBegin, const char16_t* aEnd);

} // namespace SSE2
} // namespace mozilla
