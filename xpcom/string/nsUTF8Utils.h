/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsUTF8Utils_h_
#define nsUTF8Utils_h_

// This file may be used in two ways: if MOZILLA_INTERNAL_API is defined, this
// file will provide signatures for the Mozilla abstract string types. It will
// use XPCOM assertion/debugging macros, etc.

#include <type_traits>

#include "nscore.h"
#include "mozilla/Assertions.h"
#include "mozilla/EndianUtils.h"

#include "nsCharTraits.h"

#ifdef MOZILLA_INTERNAL_API
#  define UTF8UTILS_WARNING(msg) NS_WARNING(msg)
#else
#  define UTF8UTILS_WARNING(msg)
#endif

class UTF8traits {
 public:
  static bool isASCII(char aChar) { return (aChar & 0x80) == 0x00; }
  static bool isInSeq(char aChar) { return (aChar & 0xC0) == 0x80; }
  static bool is2byte(char aChar) { return (aChar & 0xE0) == 0xC0; }
  static bool is3byte(char aChar) { return (aChar & 0xF0) == 0xE0; }
  static bool is4byte(char aChar) { return (aChar & 0xF8) == 0xF0; }
  static bool is5byte(char aChar) { return (aChar & 0xFC) == 0xF8; }
  static bool is6byte(char aChar) { return (aChar & 0xFE) == 0xFC; }
  // return the number of bytes in a sequence beginning with aChar
  static int bytes(char aChar) {
    if (isASCII(aChar)) {
      return 1;
    }
    if (is2byte(aChar)) {
      return 2;
    }
    if (is3byte(aChar)) {
      return 3;
    }
    if (is4byte(aChar)) {
      return 4;
    }
    MOZ_ASSERT_UNREACHABLE("should not be used for in-sequence characters");
    return 1;
  }
};

/**
 * Extract the next Unicode scalar value from the buffer and return it. The
 * pointer passed in is advanced to the start of the next character in the
 * buffer. Upon error, the return value is 0xFFFD, *aBuffer is advanced
 * over the maximal valid prefix and *aErr is set to true (if aErr is not
 * null).
 *
 * Note: This method never sets *aErr to false to allow error accumulation
 * across multiple calls.
 *
 * Precondition: *aBuffer < aEnd
 */
class UTF8CharEnumerator {
 public:
  static inline char32_t NextChar(const char** aBuffer, const char* aEnd,
                                  bool* aErr = nullptr) {
    MOZ_ASSERT(aBuffer, "null buffer pointer pointer");
    MOZ_ASSERT(aEnd, "null end pointer");

    const unsigned char* p = reinterpret_cast<const unsigned char*>(*aBuffer);
    const unsigned char* end = reinterpret_cast<const unsigned char*>(aEnd);

    MOZ_ASSERT(p, "null buffer");
    MOZ_ASSERT(p < end, "Bogus range");

    unsigned char first = *p;
    ++p;

    if (MOZ_LIKELY(first < 0x80U)) {
      *aBuffer = reinterpret_cast<const char*>(p);
      return first;
    }

    // Unsigned underflow is defined behavior
    if (MOZ_UNLIKELY((p == end) || ((first - 0xC2U) >= (0xF5U - 0xC2U)))) {
      *aBuffer = reinterpret_cast<const char*>(p);
      if (aErr) {
        *aErr = true;
      }
      return 0xFFFDU;
    }

    unsigned char second = *p;

    if (first < 0xE0U) {
      // Two-byte
      if (MOZ_LIKELY((second & 0xC0U) == 0x80U)) {
        ++p;
        *aBuffer = reinterpret_cast<const char*>(p);
        return ((uint32_t(first) & 0x1FU) << 6) | (uint32_t(second) & 0x3FU);
      }
      *aBuffer = reinterpret_cast<const char*>(p);
      if (aErr) {
        *aErr = true;
      }
      return 0xFFFDU;
    }

    if (MOZ_LIKELY(first < 0xF0U)) {
      // Three-byte
      unsigned char lower = 0x80U;
      unsigned char upper = 0xBFU;
      if (first == 0xE0U) {
        lower = 0xA0U;
      } else if (first == 0xEDU) {
        upper = 0x9FU;
      }
      if (MOZ_LIKELY(second >= lower && second <= upper)) {
        ++p;
        if (MOZ_LIKELY(p != end)) {
          unsigned char third = *p;
          if (MOZ_LIKELY((third & 0xC0U) == 0x80U)) {
            ++p;
            *aBuffer = reinterpret_cast<const char*>(p);
            return ((uint32_t(first) & 0xFU) << 12) |
                   ((uint32_t(second) & 0x3FU) << 6) |
                   (uint32_t(third) & 0x3FU);
          }
        }
      }
      *aBuffer = reinterpret_cast<const char*>(p);
      if (aErr) {
        *aErr = true;
      }
      return 0xFFFDU;
    }

    // Four-byte
    unsigned char lower = 0x80U;
    unsigned char upper = 0xBFU;
    if (first == 0xF0U) {
      lower = 0x90U;
    } else if (first == 0xF4U) {
      upper = 0x8FU;
    }
    if (MOZ_LIKELY(second >= lower && second <= upper)) {
      ++p;
      if (MOZ_LIKELY(p != end)) {
        unsigned char third = *p;
        if (MOZ_LIKELY((third & 0xC0U) == 0x80U)) {
          ++p;
          if (MOZ_LIKELY(p != end)) {
            unsigned char fourth = *p;
            if (MOZ_LIKELY((fourth & 0xC0U) == 0x80U)) {
              ++p;
              *aBuffer = reinterpret_cast<const char*>(p);
              return ((uint32_t(first) & 0x7U) << 18) |
                     ((uint32_t(second) & 0x3FU) << 12) |
                     ((uint32_t(third) & 0x3FU) << 6) |
                     (uint32_t(fourth) & 0x3FU);
            }
          }
        }
      }
    }
    *aBuffer = reinterpret_cast<const char*>(p);
    if (aErr) {
      *aErr = true;
    }
    return 0xFFFDU;
  }
};

/**
 * Extract the next Unicode scalar value from the buffer and return it. The
 * pointer passed in is advanced to the start of the next character in the
 * buffer. Upon error, the return value is 0xFFFD, *aBuffer is advanced over
 * the unpaired surrogate and *aErr is set to true (if aErr is not null).
 *
 * Note: This method never sets *aErr to false to allow error accumulation
 * across multiple calls.
 *
 * Precondition: *aBuffer < aEnd
 */
class UTF16CharEnumerator {
 public:
  static inline char32_t NextChar(const char16_t** aBuffer,
                                  const char16_t* aEnd, bool* aErr = nullptr) {
    MOZ_ASSERT(aBuffer, "null buffer pointer pointer");
    MOZ_ASSERT(aEnd, "null end pointer");

    const char16_t* p = *aBuffer;

    MOZ_ASSERT(p, "null buffer");
    MOZ_ASSERT(p < aEnd, "Bogus range");

    char16_t c = *p++;

    // Let's use encoding_rs-style code golf here.
    // Unsigned underflow is defined behavior
    char16_t cMinusSurrogateStart = c - 0xD800U;
    if (MOZ_LIKELY(cMinusSurrogateStart > (0xDFFFU - 0xD800U))) {
      *aBuffer = p;
      return c;
    }
    if (MOZ_LIKELY(cMinusSurrogateStart <= (0xDBFFU - 0xD800U))) {
      // High surrogate
      if (MOZ_LIKELY(p != aEnd)) {
        char16_t second = *p;
        // Unsigned underflow is defined behavior
        if (MOZ_LIKELY((second - 0xDC00U) <= (0xDFFFU - 0xDC00U))) {
          *aBuffer = ++p;
          return (uint32_t(c) << 10) + uint32_t(second) -
                 (((0xD800U << 10) - 0x10000U) + 0xDC00U);
        }
      }
    }
    // Unpaired surrogate
    *aBuffer = p;
    if (aErr) {
      *aErr = true;
    }
    return 0xFFFDU;
  }
};

template <typename Char, typename UnsignedT>
inline UnsignedT RewindToPriorUTF8Codepoint(const Char* utf8Chars,
                                            UnsignedT index) {
  static_assert(std::is_same_v<Char, char> ||
                    std::is_same_v<Char, unsigned char> ||
                    std::is_same_v<Char, signed char>,
                "UTF-8 data must be in 8-bit units");
  static_assert(std::is_unsigned_v<UnsignedT>, "index type must be unsigned");
  while (index > 0 && (utf8Chars[index] & 0xC0) == 0x80) --index;

  return index;
}

#undef UTF8UTILS_WARNING

#endif /* !defined(nsUTF8Utils_h_) */
