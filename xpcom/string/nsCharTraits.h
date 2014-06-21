/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCharTraits_h___
#define nsCharTraits_h___

#include <ctype.h> // for |EOF|, |WEOF|
#include <string.h> // for |memcpy|, et al

#include "nscore.h" // for |char16_t|

// This file may be used (through nsUTF8Utils.h) from non-XPCOM code, in
// particular the standalone software updater. In that case stub out
// the macros provided by nsDebug.h which are only usable when linking XPCOM

#ifdef NS_NO_XPCOM
#define NS_WARNING(msg)
#define NS_ASSERTION(cond, msg)
#define NS_ERROR(msg)
#else
#include "nsDebug.h"  // for NS_ASSERTION
#endif

/*
 * Some macros for converting char16_t (UTF-16) to and from Unicode scalar
 * values.
 *
 * Note that UTF-16 represents all Unicode scalar values up to U+10FFFF by
 * using "surrogate pairs". These consist of a high surrogate, i.e. a code
 * point in the range U+D800 - U+DBFF, and a low surrogate, i.e. a code point
 * in the range U+DC00 - U+DFFF, like this:
 *
 *  U+D800 U+DC00 =  U+10000
 *  U+D800 U+DC01 =  U+10001
 *  ...
 *  U+DBFF U+DFFE = U+10FFFE
 *  U+DBFF U+DFFF = U+10FFFF
 *
 * These surrogate code points U+D800 - U+DFFF are not themselves valid Unicode
 * scalar values and are not well-formed UTF-16 except as high-surrogate /
 * low-surrogate pairs.
 */

#define PLANE1_BASE          uint32_t(0x00010000)
// High surrogates are in the range 0xD800 -- OxDBFF
#define NS_IS_HIGH_SURROGATE(u) ((uint32_t(u) & 0xFFFFFC00) == 0xD800)
// Low surrogates are in the range 0xDC00 -- 0xDFFF
#define NS_IS_LOW_SURROGATE(u)  ((uint32_t(u) & 0xFFFFFC00) == 0xDC00)
// Faster than testing NS_IS_HIGH_SURROGATE || NS_IS_LOW_SURROGATE
#define IS_SURROGATE(u)      ((uint32_t(u) & 0xFFFFF800) == 0xD800)

// Everything else is not a surrogate: 0x000 -- 0xD7FF, 0xE000 -- 0xFFFF

// N = (H - 0xD800) * 0x400 + 0x10000 + (L - 0xDC00)
// I wonder whether we could somehow assert that H is a high surrogate
// and L is a low surrogate
#define SURROGATE_TO_UCS4(h, l) (((uint32_t(h) & 0x03FF) << 10) + \
                                 (uint32_t(l) & 0x03FF) + PLANE1_BASE)

// Extract surrogates from a UCS4 char
// Reference: the Unicode standard 4.0, section 3.9
// Since (c - 0x10000) >> 10 == (c >> 10) - 0x0080 and
// 0xD7C0 == 0xD800 - 0x0080,
// ((c - 0x10000) >> 10) + 0xD800 can be simplified to
#define H_SURROGATE(c) char16_t(char16_t(uint32_t(c) >> 10) + \
                                char16_t(0xD7C0))
// where it's to be noted that 0xD7C0 is not bitwise-OR'd
// but added.

// Since 0x10000 & 0x03FF == 0,
// (c - 0x10000) & 0x03FF == c & 0x03FF so that
// ((c - 0x10000) & 0x03FF) | 0xDC00 is equivalent to
#define L_SURROGATE(c) char16_t(char16_t(uint32_t(c) & uint32_t(0x03FF)) | \
                                 char16_t(0xDC00))

#define IS_IN_BMP(ucs) (uint32_t(ucs) < PLANE1_BASE)
#define UCS2_REPLACEMENT_CHAR char16_t(0xFFFD)

#define UCS_END uint32_t(0x00110000)
#define IS_VALID_CHAR(c) ((uint32_t(c) < UCS_END) && !IS_SURROGATE(c))
#define ENSURE_VALID_CHAR(c) (IS_VALID_CHAR(c) ? (c) : UCS2_REPLACEMENT_CHAR)

template <class CharT>
struct nsCharTraits
{
};

template <>
struct nsCharTraits<char16_t>
{
  typedef char16_t char_type;
  typedef uint16_t  unsigned_char_type;
  typedef char      incompatible_char_type;

  static char_type* const sEmptyBuffer;

  static void
  assign(char_type& aLhs, char_type aRhs)
  {
    aLhs = aRhs;
  }


  // integer representation of characters:
  typedef int int_type;

  static char_type
  to_char_type(int_type aChar)
  {
    return char_type(aChar);
  }

  static int_type
  to_int_type(char_type aChar)
  {
    return int_type(static_cast<unsigned_char_type>(aChar));
  }

  static bool
  eq_int_type(int_type aLhs, int_type aRhs)
  {
    return aLhs == aRhs;
  }


  // |char_type| comparisons:

  static bool
  eq(char_type aLhs, char_type aRhs)
  {
    return aLhs == aRhs;
  }

  static bool
  lt(char_type aLhs, char_type aRhs)
  {
    return aLhs < aRhs;
  }


  // operations on s[n] arrays:

  static char_type*
  move(char_type* aStr1, const char_type* aStr2, size_t aN)
  {
    return static_cast<char_type*>(memmove(aStr1, aStr2,
                                           aN * sizeof(char_type)));
  }

  static char_type*
  copy(char_type* aStr1, const char_type* aStr2, size_t aN)
  {
    return static_cast<char_type*>(memcpy(aStr1, aStr2,
                                          aN * sizeof(char_type)));
  }

  static char_type*
  copyASCII(char_type* aStr1, const char* aStr2, size_t aN)
  {
    for (char_type* s = aStr1; aN--; ++s, ++aStr2) {
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      *s = static_cast<char_type>(*aStr2);
    }
    return aStr1;
  }

  static char_type*
  assign(char_type* aStr, size_t aN, char_type aChar)
  {
    char_type* result = aStr;
    while (aN--) {
      assign(*aStr++, aChar);
    }
    return result;
  }

  static int
  compare(const char_type* aStr1, const char_type* aStr2, size_t aN)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      if (!eq(*aStr1, *aStr2)) {
        return to_int_type(*aStr1) - to_int_type(*aStr2);
      }
    }

    return 0;
  }

  static int
  compareASCII(const char_type* aStr1, const char* aStr2, size_t aN)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      if (!eq_int_type(to_int_type(*aStr1),
                       to_int_type(static_cast<char_type>(*aStr2)))) {
        return to_int_type(*aStr1) -
               to_int_type(static_cast<char_type>(*aStr2));
      }
    }

    return 0;
  }

  // this version assumes that s2 is null-terminated and s1 has length n.
  // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
  // we return 1.
  static int
  compareASCIINullTerminated(const char_type* aStr1, size_t aN,
                             const char* aStr2)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      if (!*aStr2) {
        return 1;
      }
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      if (!eq_int_type(to_int_type(*aStr1),
                       to_int_type(static_cast<char_type>(*aStr2)))) {
        return to_int_type(*aStr1) -
               to_int_type(static_cast<char_type>(*aStr2));
      }
    }

    if (*aStr2) {
      return -1;
    }

    return 0;
  }

  /**
   * Convert c to its lower-case form, but only if c is in the ASCII
   * range. Otherwise leave it alone.
   */
  static char_type
  ASCIIToLower(char_type aChar)
  {
    if (aChar >= 'A' && aChar <= 'Z') {
      return char_type(aChar + ('a' - 'A'));
    }

    return aChar;
  }

  static int
  compareLowerCaseToASCII(const char_type* aStr1, const char* aStr2, size_t aN)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      NS_ASSERTION(!(*aStr2 >= 'A' && *aStr2 <= 'Z'),
                   "Unexpected uppercase character");
      char_type lower_s1 = ASCIIToLower(*aStr1);
      if (lower_s1 != static_cast<char_type>(*aStr2)) {
        return to_int_type(lower_s1) -
               to_int_type(static_cast<char_type>(*aStr2));
      }
    }

    return 0;
  }

  // this version assumes that s2 is null-terminated and s1 has length n.
  // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
  // we return 1.
  static int
  compareLowerCaseToASCIINullTerminated(const char_type* aStr1,
                                        size_t aN, const char* aStr2)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      if (!*aStr2) {
        return 1;
      }
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      NS_ASSERTION(!(*aStr2 >= 'A' && *aStr2 <= 'Z'),
                   "Unexpected uppercase character");
      char_type lower_s1 = ASCIIToLower(*aStr1);
      if (lower_s1 != static_cast<char_type>(*aStr2)) {
        return to_int_type(lower_s1) -
               to_int_type(static_cast<char_type>(*aStr2));
      }
    }

    if (*aStr2) {
      return -1;
    }

    return 0;
  }

  static size_t
  length(const char_type* aStr)
  {
    size_t result = 0;
    while (!eq(*aStr++, char_type(0))) {
      ++result;
    }
    return result;
  }

  static const char_type*
  find(const char_type* aStr, size_t aN, char_type aChar)
  {
    while (aN--) {
      if (eq(*aStr, aChar)) {
        return aStr;
      }
      ++aStr;
    }

    return 0;
  }
};

template <>
struct nsCharTraits<char>
{
  typedef char           char_type;
  typedef unsigned char  unsigned_char_type;
  typedef char16_t      incompatible_char_type;

  static char_type* const sEmptyBuffer;

  static void
  assign(char_type& aLhs, char_type aRhs)
  {
    aLhs = aRhs;
  }


  // integer representation of characters:

  typedef int int_type;

  static char_type
  to_char_type(int_type aChar)
  {
    return char_type(aChar);
  }

  static int_type
  to_int_type(char_type aChar)
  {
    return int_type(static_cast<unsigned_char_type>(aChar));
  }

  static bool
  eq_int_type(int_type aLhs, int_type aRhs)
  {
    return aLhs == aRhs;
  }


  // |char_type| comparisons:

  static bool eq(char_type aLhs, char_type aRhs)
  {
    return aLhs == aRhs;
  }

  static bool
  lt(char_type aLhs, char_type aRhs)
  {
    return aLhs < aRhs;
  }


  // operations on s[n] arrays:

  static char_type*
  move(char_type* aStr1, const char_type* aStr2, size_t aN)
  {
    return static_cast<char_type*>(memmove(aStr1, aStr2,
                                           aN * sizeof(char_type)));
  }

  static char_type*
  copy(char_type* aStr1, const char_type* aStr2, size_t aN)
  {
    return static_cast<char_type*>(memcpy(aStr1, aStr2,
                                          aN * sizeof(char_type)));
  }

  static char_type*
  copyASCII(char_type* aStr1, const char* aStr2, size_t aN)
  {
    return copy(aStr1, aStr2, aN);
  }

  static char_type*
  assign(char_type* aStr, size_t aN, char_type aChar)
  {
    return static_cast<char_type*>(memset(aStr, to_int_type(aChar), aN));
  }

  static int
  compare(const char_type* aStr1, const char_type* aStr2, size_t aN)
  {
    return memcmp(aStr1, aStr2, aN);
  }

  static int
  compareASCII(const char_type* aStr1, const char* aStr2, size_t aN)
  {
#ifdef DEBUG
    for (size_t i = 0; i < aN; ++i) {
      NS_ASSERTION(!(aStr2[i] & ~0x7F), "Unexpected non-ASCII character");
    }
#endif
    return compare(aStr1, aStr2, aN);
  }

  // this version assumes that s2 is null-terminated and s1 has length n.
  // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
  // we return 1.
  static int
  compareASCIINullTerminated(const char_type* aStr1, size_t aN,
                             const char* aStr2)
  {
    // can't use strcmp here because we don't want to stop when aStr1
    // contains a null
    for (; aN--; ++aStr1, ++aStr2) {
      if (!*aStr2) {
        return 1;
      }
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      if (*aStr1 != *aStr2) {
        return to_int_type(*aStr1) - to_int_type(*aStr2);
      }
    }

    if (*aStr2) {
      return -1;
    }

    return 0;
  }

  /**
   * Convert c to its lower-case form, but only if c is ASCII.
   */
  static char_type
  ASCIIToLower(char_type aChar)
  {
    if (aChar >= 'A' && aChar <= 'Z') {
      return char_type(aChar + ('a' - 'A'));
    }

    return aChar;
  }

  static int
  compareLowerCaseToASCII(const char_type* aStr1, const char* aStr2, size_t aN)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      NS_ASSERTION(!(*aStr2 >= 'A' && *aStr2 <= 'Z'),
                   "Unexpected uppercase character");
      char_type lower_s1 = ASCIIToLower(*aStr1);
      if (lower_s1 != *aStr2) {
        return to_int_type(lower_s1) - to_int_type(*aStr2);
      }
    }
    return 0;
  }

  // this version assumes that s2 is null-terminated and s1 has length n.
  // if s1 is shorter than s2 then we return -1; if s1 is longer than s2,
  // we return 1.
  static int
  compareLowerCaseToASCIINullTerminated(const char_type* aStr1, size_t aN,
                                        const char* aStr2)
  {
    for (; aN--; ++aStr1, ++aStr2) {
      if (!*aStr2) {
        return 1;
      }
      NS_ASSERTION(!(*aStr2 & ~0x7F), "Unexpected non-ASCII character");
      NS_ASSERTION(!(*aStr2 >= 'A' && *aStr2 <= 'Z'),
                   "Unexpected uppercase character");
      char_type lower_s1 = ASCIIToLower(*aStr1);
      if (lower_s1 != *aStr2) {
        return to_int_type(lower_s1) - to_int_type(*aStr2);
      }
    }

    if (*aStr2) {
      return -1;
    }

    return 0;
  }

  static size_t
  length(const char_type* aStr)
  {
    return strlen(aStr);
  }

  static const char_type*
  find(const char_type* aStr, size_t aN, char_type aChar)
  {
    return reinterpret_cast<const char_type*>(memchr(aStr, to_int_type(aChar),
                                                     aN));
  }
};

template <class InputIterator>
struct nsCharSourceTraits
{
  typedef typename InputIterator::difference_type difference_type;

  static uint32_t
  readable_distance(const InputIterator& aFirst, const InputIterator& aLast)
  {
    // assumes single fragment
    return uint32_t(aLast.get() - aFirst.get());
  }

  static const typename InputIterator::value_type*
  read(const InputIterator& aIter)
  {
    return aIter.get();
  }

  static void
  advance(InputIterator& aStr, difference_type aN)
  {
    aStr.advance(aN);
  }
};

template <class CharT>
struct nsCharSourceTraits<CharT*>
{
  typedef ptrdiff_t difference_type;

  static uint32_t
  readable_distance(CharT* aStr)
  {
    return uint32_t(nsCharTraits<CharT>::length(aStr));
    // return numeric_limits<uint32_t>::max();
  }

  static uint32_t
  readable_distance(CharT* aFirst, CharT* aLast)
  {
    return uint32_t(aLast - aFirst);
  }

  static const CharT*
  read(CharT* aStr)
  {
    return aStr;
  }

  static void
  advance(CharT*& aStr, difference_type aN)
  {
    aStr += aN;
  }
};

template <class OutputIterator>
struct nsCharSinkTraits
{
  static void
  write(OutputIterator& aIter, const typename OutputIterator::value_type* aStr,
        uint32_t aN)
  {
    aIter.write(aStr, aN);
  }
};

template <class CharT>
struct nsCharSinkTraits<CharT*>
{
  static void
  write(CharT*& aIter, const CharT* aStr, uint32_t aN)
  {
    nsCharTraits<CharT>::move(aIter, aStr, aN);
    aIter += aN;
  }
};

#endif // !defined(nsCharTraits_h___)
