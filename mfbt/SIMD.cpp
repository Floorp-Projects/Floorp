/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SIMD.h"

#include <stdint.h>
#include <type_traits>

#include "mozilla/EndianUtils.h"
#include "mozilla/SSE.h"

namespace mozilla {

template <typename TValue>
const TValue* FindInBufferNaive(const TValue* ptr, TValue value,
                                size_t length) {
  const TValue* end = ptr + length;
  while (ptr < end) {
    if (*ptr == value) {
      return ptr;
    }
    ptr++;
  }
  return nullptr;
}

#ifdef MOZILLA_PRESUME_SSE2

#  include <immintrin.h>

const __m128i* Cast128(uintptr_t ptr) {
  return reinterpret_cast<const __m128i*>(ptr);
}

const __m256i* Cast256(uintptr_t ptr) {
  return reinterpret_cast<const __m256i*>(ptr);
}

template <typename T>
T GetAs(uintptr_t ptr) {
  return *reinterpret_cast<const T*>(ptr);
}

// Akin to ceil/floor, AlignDown/AlignUp will return the original pointer if it
// is already aligned.
uintptr_t AlignDown16(uintptr_t ptr) { return ptr & ~0xf; }

uintptr_t AlignUp16(uintptr_t ptr) { return AlignDown16(ptr + 0xf); }

uintptr_t AlignDown32(uintptr_t ptr) { return ptr & ~0x1f; }

uintptr_t AlignUp32(uintptr_t ptr) { return AlignDown32(ptr + 0x1f); }

template <typename TValue>
__m128i CmpEq128(__m128i a, __m128i b) {
  static_assert(sizeof(TValue) == 1 || sizeof(TValue) == 2);
  if (sizeof(TValue) == 1) {
    return _mm_cmpeq_epi8(a, b);
  }
  return _mm_cmpeq_epi16(a, b);
}

template <typename TValue>
__m256i CmpEq256(__m256i a, __m256i b) {
  static_assert(sizeof(TValue) == 1 || sizeof(TValue) == 2 ||
                sizeof(TValue) == 8);
  if (sizeof(TValue) == 1) {
    return _mm256_cmpeq_epi8(a, b);
  }
  if (sizeof(TValue) == 2) {
    return _mm256_cmpeq_epi16(a, b);
  }

  return _mm256_cmpeq_epi64(a, b);
}

#  ifdef __GNUC__

// Earlier versions of GCC are missing the _mm_loadu_si32 instruction. This
// workaround from Peter Cordes (https://stackoverflow.com/a/72837992) compiles
// down to the same instructions. We could just replace _mm_loadu_si32
__m128i Load32BitsIntoXMM(uintptr_t ptr) {
  int tmp;
  memcpy(&tmp, reinterpret_cast<const void*>(ptr),
         sizeof(tmp));            // unaligned aliasing-safe load
  return _mm_cvtsi32_si128(tmp);  // efficient on GCC/clang/MSVC
}

#  else

__m128i Load32BitsIntoXMM(uintptr_t ptr) {
  return _mm_loadu_si32(Cast128(ptr));
}

#  endif

const char* Check4x4Chars(__m128i needle, uintptr_t a, uintptr_t b, uintptr_t c,
                          uintptr_t d) {
  __m128i haystackA = Load32BitsIntoXMM(a);
  __m128i cmpA = CmpEq128<char>(needle, haystackA);
  __m128i haystackB = Load32BitsIntoXMM(b);
  __m128i cmpB = CmpEq128<char>(needle, haystackB);
  __m128i haystackC = Load32BitsIntoXMM(c);
  __m128i cmpC = CmpEq128<char>(needle, haystackC);
  __m128i haystackD = Load32BitsIntoXMM(d);
  __m128i cmpD = CmpEq128<char>(needle, haystackD);
  __m128i or_ab = _mm_or_si128(cmpA, cmpB);
  __m128i or_cd = _mm_or_si128(cmpC, cmpD);
  __m128i or_abcd = _mm_or_si128(or_ab, or_cd);
  int orMask = _mm_movemask_epi8(or_abcd);
  if (orMask & 0xf) {
    int cmpMask;
    cmpMask = _mm_movemask_epi8(cmpA);
    if (cmpMask & 0xf) {
      return reinterpret_cast<const char*>(a + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpB);
    if (cmpMask & 0xf) {
      return reinterpret_cast<const char*>(b + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpC);
    if (cmpMask & 0xf) {
      return reinterpret_cast<const char*>(c + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpD);
    if (cmpMask & 0xf) {
      return reinterpret_cast<const char*>(d + __builtin_ctz(cmpMask));
    }
  }

  return nullptr;
}

template <typename TValue>
const TValue* Check4x8Bytes(__m128i needle, uintptr_t a, uintptr_t b,
                            uintptr_t c, uintptr_t d) {
  __m128i haystackA = _mm_loadu_si64(Cast128(a));
  __m128i cmpA = CmpEq128<TValue>(needle, haystackA);
  __m128i haystackB = _mm_loadu_si64(Cast128(b));
  __m128i cmpB = CmpEq128<TValue>(needle, haystackB);
  __m128i haystackC = _mm_loadu_si64(Cast128(c));
  __m128i cmpC = CmpEq128<TValue>(needle, haystackC);
  __m128i haystackD = _mm_loadu_si64(Cast128(d));
  __m128i cmpD = CmpEq128<TValue>(needle, haystackD);
  __m128i or_ab = _mm_or_si128(cmpA, cmpB);
  __m128i or_cd = _mm_or_si128(cmpC, cmpD);
  __m128i or_abcd = _mm_or_si128(or_ab, or_cd);
  int orMask = _mm_movemask_epi8(or_abcd);
  if (orMask & 0xff) {
    int cmpMask;
    cmpMask = _mm_movemask_epi8(cmpA);
    if (cmpMask & 0xff) {
      return reinterpret_cast<const TValue*>(a + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpB);
    if (cmpMask & 0xff) {
      return reinterpret_cast<const TValue*>(b + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpC);
    if (cmpMask & 0xff) {
      return reinterpret_cast<const TValue*>(c + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpD);
    if (cmpMask & 0xff) {
      return reinterpret_cast<const TValue*>(d + __builtin_ctz(cmpMask));
    }
  }

  return nullptr;
}

template <typename TValue>
const TValue* Check4x16Bytes(__m128i needle, uintptr_t a, uintptr_t b,
                             uintptr_t c, uintptr_t d) {
  __m128i haystackA = _mm_loadu_si128(Cast128(a));
  __m128i cmpA = CmpEq128<TValue>(needle, haystackA);
  __m128i haystackB = _mm_loadu_si128(Cast128(b));
  __m128i cmpB = CmpEq128<TValue>(needle, haystackB);
  __m128i haystackC = _mm_loadu_si128(Cast128(c));
  __m128i cmpC = CmpEq128<TValue>(needle, haystackC);
  __m128i haystackD = _mm_loadu_si128(Cast128(d));
  __m128i cmpD = CmpEq128<TValue>(needle, haystackD);
  __m128i or_ab = _mm_or_si128(cmpA, cmpB);
  __m128i or_cd = _mm_or_si128(cmpC, cmpD);
  __m128i or_abcd = _mm_or_si128(or_ab, or_cd);
  int orMask = _mm_movemask_epi8(or_abcd);
  if (orMask) {
    int cmpMask;
    cmpMask = _mm_movemask_epi8(cmpA);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(a + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpB);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(b + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpC);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(c + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm_movemask_epi8(cmpD);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(d + __builtin_ctz(cmpMask));
    }
  }

  return nullptr;
}

template <typename TValue>
const TValue* Check4x32Bytes(__m256i needle, uintptr_t a, uintptr_t b,
                             uintptr_t c, uintptr_t d) {
  __m256i haystackA = _mm256_loadu_si256(Cast256(a));
  __m256i cmpA = CmpEq256<TValue>(needle, haystackA);
  __m256i haystackB = _mm256_loadu_si256(Cast256(b));
  __m256i cmpB = CmpEq256<TValue>(needle, haystackB);
  __m256i haystackC = _mm256_loadu_si256(Cast256(c));
  __m256i cmpC = CmpEq256<TValue>(needle, haystackC);
  __m256i haystackD = _mm256_loadu_si256(Cast256(d));
  __m256i cmpD = CmpEq256<TValue>(needle, haystackD);
  __m256i or_ab = _mm256_or_si256(cmpA, cmpB);
  __m256i or_cd = _mm256_or_si256(cmpC, cmpD);
  __m256i or_abcd = _mm256_or_si256(or_ab, or_cd);
  int orMask = _mm256_movemask_epi8(or_abcd);
  if (orMask) {
    int cmpMask;
    cmpMask = _mm256_movemask_epi8(cmpA);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(a + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm256_movemask_epi8(cmpB);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(b + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm256_movemask_epi8(cmpC);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(c + __builtin_ctz(cmpMask));
    }
    cmpMask = _mm256_movemask_epi8(cmpD);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(d + __builtin_ctz(cmpMask));
    }
  }

  return nullptr;
}

enum class HaystackOverlap {
  Overlapping,
  Sequential,
};

// Check two 16-byte chunks for the two-byte sequence loaded into needle1
// followed by needle1. `carryOut` is an optional pointer which we will
// populate based on whether the last character of b matches needle1. This
// should be provided on subsequent calls via `carryIn` so we can detect cases
// where the last byte of b's 16-byte chunk is needle1 and the first byte of
// the next a's 16-byte chunk is needle2. `overlap` and whether
// `carryIn`/`carryOut` are NULL should be knowable at compile time to avoid
// branching.
template <typename TValue>
const TValue* Check2x2x16Bytes(__m128i needle1, __m128i needle2, uintptr_t a,
                               uintptr_t b, __m128i* carryIn, __m128i* carryOut,
                               HaystackOverlap overlap) {
  const int shiftRightAmount = 16 - sizeof(TValue);
  const int shiftLeftAmount = sizeof(TValue);
  __m128i haystackA = _mm_loadu_si128(Cast128(a));
  __m128i cmpA1 = CmpEq128<TValue>(needle1, haystackA);
  __m128i cmpA2 = CmpEq128<TValue>(needle2, haystackA);
  __m128i cmpA;
  if (carryIn) {
    cmpA = _mm_and_si128(
        _mm_or_si128(_mm_bslli_si128(cmpA1, shiftLeftAmount), *carryIn), cmpA2);
  } else {
    cmpA = _mm_and_si128(_mm_bslli_si128(cmpA1, shiftLeftAmount), cmpA2);
  }
  __m128i haystackB = _mm_loadu_si128(Cast128(b));
  __m128i cmpB1 = CmpEq128<TValue>(needle1, haystackB);
  __m128i cmpB2 = CmpEq128<TValue>(needle2, haystackB);
  __m128i cmpB;
  if (overlap == HaystackOverlap::Overlapping) {
    cmpB = _mm_and_si128(_mm_bslli_si128(cmpB1, shiftLeftAmount), cmpB2);
  } else {
    MOZ_ASSERT(overlap == HaystackOverlap::Sequential);
    __m128i carryAB = _mm_bsrli_si128(cmpA1, shiftRightAmount);
    cmpB = _mm_and_si128(
        _mm_or_si128(_mm_bslli_si128(cmpB1, shiftLeftAmount), carryAB), cmpB2);
  }
  __m128i or_ab = _mm_or_si128(cmpA, cmpB);
  int orMask = _mm_movemask_epi8(or_ab);
  if (orMask) {
    int cmpMask;
    cmpMask = _mm_movemask_epi8(cmpA);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(a + __builtin_ctz(cmpMask) -
                                             shiftLeftAmount);
    }
    cmpMask = _mm_movemask_epi8(cmpB);
    if (cmpMask) {
      return reinterpret_cast<const TValue*>(b + __builtin_ctz(cmpMask) -
                                             shiftLeftAmount);
    }
  }

  if (carryOut) {
    _mm_store_si128(carryOut, _mm_bsrli_si128(cmpB1, shiftRightAmount));
  }

  return nullptr;
}

template <typename TValue>
const TValue* FindInBuffer(const TValue* ptr, TValue value, size_t length) {
  static_assert(sizeof(TValue) == 1 || sizeof(TValue) == 2);
  static_assert(std::is_unsigned<TValue>::value);
  uint64_t splat64;
  if (sizeof(TValue) == 1) {
    splat64 = 0x0101010101010101llu;
  } else {
    splat64 = 0x0001000100010001llu;
  }

  // Load our needle into a 16-byte register
  uint64_t u64_value = static_cast<uint64_t>(value) * splat64;
  int64_t i64_value = *reinterpret_cast<int64_t*>(&u64_value);
  __m128i needle = _mm_set_epi64x(i64_value, i64_value);

  size_t numBytes = length * sizeof(TValue);
  uintptr_t cur = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t end = cur + numBytes;

  if ((sizeof(TValue) > 1 && numBytes < 16) || numBytes < 4) {
    while (cur < end) {
      if (GetAs<TValue>(cur) == value) {
        return reinterpret_cast<const TValue*>(cur);
      }
      cur += sizeof(TValue);
    }
    return nullptr;
  }

  if (numBytes < 16) {
    // NOTE: here and below, we have some bit fiddling which could look a
    // little weird. The important thing to note though is it's just a trick
    // for getting the number 4 if numBytes is greater than or equal to 8,
    // and 0 otherwise. This lets us fully cover the range without any
    // branching for the case where numBytes is in [4,8), and [8,16). We get
    // four ranges from this - if numbytes > 8, we get:
    //   [0,4), [4,8], [end - 8), [end - 4)
    // and if numbytes < 8, we get
    //   [0,4), [0,4), [end - 4), [end - 4)
    uintptr_t a = cur;
    uintptr_t b = cur + ((numBytes & 8) >> 1);
    uintptr_t c = end - 4 - ((numBytes & 8) >> 1);
    uintptr_t d = end - 4;
    const char* charResult = Check4x4Chars(needle, a, b, c, d);
    // Note: we ensure above that sizeof(TValue) == 1 here, so this is
    // either char to char or char to something like a uint8_t.
    return reinterpret_cast<const TValue*>(charResult);
  }

  if (numBytes < 64) {
    // NOTE: see the above explanation of the similar chunk of code, but in
    // this case, replace 8 with 32 and 4 with 16.
    uintptr_t a = cur;
    uintptr_t b = cur + ((numBytes & 32) >> 1);
    uintptr_t c = end - 16 - ((numBytes & 32) >> 1);
    uintptr_t d = end - 16;
    return Check4x16Bytes<TValue>(needle, a, b, c, d);
  }

  // Get the initial unaligned load out of the way. This will overlap with the
  // aligned stuff below, but the overlapped part should effectively be free
  // (relative to a mispredict from doing a byte-by-byte loop).
  __m128i haystack = _mm_loadu_si128(Cast128(cur));
  __m128i cmp = CmpEq128<TValue>(needle, haystack);
  int cmpMask = _mm_movemask_epi8(cmp);
  if (cmpMask) {
    return reinterpret_cast<const TValue*>(cur + __builtin_ctz(cmpMask));
  }

  // Now we're working with aligned memory. Hooray! \o/
  cur = AlignUp16(cur);

  // The address of the final 48-63 bytes. We overlap this with what we check in
  // our hot loop below to avoid branching. Again, the overlap should be
  // negligible compared with a branch mispredict.
  uintptr_t tailStartPtr = AlignDown16(end - 48);
  uintptr_t tailEndPtr = end - 16;

  while (cur < tailStartPtr) {
    uintptr_t a = cur;
    uintptr_t b = cur + 16;
    uintptr_t c = cur + 32;
    uintptr_t d = cur + 48;
    const TValue* result = Check4x16Bytes<TValue>(needle, a, b, c, d);
    if (result) {
      return result;
    }
    cur += 64;
  }

  uintptr_t a = tailStartPtr;
  uintptr_t b = tailStartPtr + 16;
  uintptr_t c = tailStartPtr + 32;
  uintptr_t d = tailEndPtr;
  return Check4x16Bytes<TValue>(needle, a, b, c, d);
}

template <typename TValue>
const TValue* FindInBufferAVX2(const TValue* ptr, TValue value, size_t length) {
  static_assert(sizeof(TValue) == 1 || sizeof(TValue) == 2 ||
                sizeof(TValue) == 8);
  static_assert(std::is_unsigned<TValue>::value);

  // Load our needle into a 32-byte register
  __m256i needle;
  if (sizeof(TValue) == 1) {
    needle = _mm256_set1_epi8(value);
  } else if (sizeof(TValue) == 2) {
    needle = _mm256_set1_epi16(value);
  } else {
    needle = _mm256_set1_epi64x(value);
  }

  size_t numBytes = length * sizeof(TValue);
  uintptr_t cur = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t end = cur + numBytes;

  if (numBytes < 8 || (sizeof(TValue) == 8 && numBytes < 32)) {
    while (cur < end) {
      if (GetAs<TValue>(cur) == value) {
        return reinterpret_cast<const TValue*>(cur);
      }
      cur += sizeof(TValue);
    }
    return nullptr;
  }

  if constexpr (sizeof(TValue) != 8) {
    if (numBytes < 32) {
      __m128i needle_narrow;
      if (sizeof(TValue) == 1) {
        needle_narrow = _mm_set1_epi8(value);
      } else {
        needle_narrow = _mm_set1_epi16(value);
      }
      uintptr_t a = cur;
      uintptr_t b = cur + ((numBytes & 16) >> 1);
      uintptr_t c = end - 8 - ((numBytes & 16) >> 1);
      uintptr_t d = end - 8;
      return Check4x8Bytes<TValue>(needle_narrow, a, b, c, d);
    }
  }

  if (numBytes < 128) {
    // NOTE: see the above explanation of the similar chunk of code, but in
    // this case, replace 16 with 64 and 8 with 32.
    uintptr_t a = cur;
    uintptr_t b = cur + ((numBytes & 64) >> 1);
    uintptr_t c = end - 32 - ((numBytes & 64) >> 1);
    uintptr_t d = end - 32;
    return Check4x32Bytes<TValue>(needle, a, b, c, d);
  }

  // Get the initial unaligned load out of the way. This will overlap with the
  // aligned stuff below, but the overlapped part should effectively be free
  // (relative to a mispredict from doing a byte-by-byte loop).
  __m256i haystack = _mm256_loadu_si256(Cast256(cur));
  __m256i cmp = CmpEq256<TValue>(needle, haystack);
  int cmpMask = _mm256_movemask_epi8(cmp);
  if (cmpMask) {
    return reinterpret_cast<const TValue*>(cur + __builtin_ctz(cmpMask));
  }

  // Now we're working with aligned memory. Hooray! \o/
  cur = AlignUp32(cur);

  uintptr_t tailStartPtr = AlignDown32(end - 96);
  uintptr_t tailEndPtr = end - 32;

  while (cur < tailStartPtr) {
    uintptr_t a = cur;
    uintptr_t b = cur + 32;
    uintptr_t c = cur + 64;
    uintptr_t d = cur + 96;
    const TValue* result = Check4x32Bytes<TValue>(needle, a, b, c, d);
    if (result) {
      return result;
    }
    cur += 128;
  }

  uintptr_t a = tailStartPtr;
  uintptr_t b = tailStartPtr + 32;
  uintptr_t c = tailStartPtr + 64;
  uintptr_t d = tailEndPtr;
  return Check4x32Bytes<TValue>(needle, a, b, c, d);
}

template <typename TValue>
const TValue* TwoByteLoop(uintptr_t start, uintptr_t end, TValue v1, TValue v2);

template <>
const unsigned char* TwoByteLoop<unsigned char>(uintptr_t start, uintptr_t end,
                                                unsigned char v1,
                                                unsigned char v2) {
  uintptr_t cur = start;
  uintptr_t preEnd = end - sizeof(unsigned char);
  while (cur < preEnd) {
    // NOTE: this should only ever be called on little endian architectures.
    static_assert(MOZ_LITTLE_ENDIAN());
    uint16_t pattern =
        static_cast<uint16_t>(v1) | (static_cast<uint16_t>(v2) << 8);
    if (GetAs<uint16_t>(cur) == pattern) {
      return reinterpret_cast<const unsigned char*>(cur);
    }
    cur += sizeof(unsigned char);
  }
  return nullptr;
}

template <>
const char16_t* TwoByteLoop<char16_t>(uintptr_t start, uintptr_t end,
                                      char16_t v1, char16_t v2) {
  uintptr_t cur = start;
  uintptr_t preEnd = end - sizeof(char16_t);
  while (cur < preEnd) {
    // NOTE: this should only ever be called on little endian architectures
    static_assert(MOZ_LITTLE_ENDIAN());
    uint32_t pattern =
        static_cast<uint32_t>(v1) | (static_cast<uint32_t>(v2) << 16);
    if (GetAs<uint32_t>(cur) == pattern) {
      return reinterpret_cast<const char16_t*>(cur);
    }
    cur += sizeof(char16_t);
  }
  return nullptr;
}

template <typename TValue>
const TValue* FindTwoInBuffer(const TValue* ptr, TValue v1, TValue v2,
                              size_t length) {
  static_assert(sizeof(TValue) == 1 || sizeof(TValue) == 2);
  static_assert(std::is_unsigned<TValue>::value);
  uint64_t splat64;
  if (sizeof(TValue) == 1) {
    splat64 = 0x0101010101010101llu;
  } else {
    splat64 = 0x0001000100010001llu;
  }

  // Load our needle into a 16-byte register
  uint64_t u64_v1 = static_cast<uint64_t>(v1) * splat64;
  int64_t i64_v1 = *reinterpret_cast<int64_t*>(&u64_v1);
  __m128i needle1 = _mm_set_epi64x(i64_v1, i64_v1);
  uint64_t u64_v2 = static_cast<uint64_t>(v2) * splat64;
  int64_t i64_v2 = *reinterpret_cast<int64_t*>(&u64_v2);
  __m128i needle2 = _mm_set_epi64x(i64_v2, i64_v2);

  size_t numBytes = length * sizeof(TValue);
  uintptr_t cur = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t end = cur + numBytes;

  if (numBytes < 16) {
    return TwoByteLoop<TValue>(cur, end, v1, v2);
  }

  if (numBytes < 32) {
    uintptr_t a = cur;
    uintptr_t b = end - 16;
    return Check2x2x16Bytes<TValue>(needle1, needle2, a, b, nullptr, nullptr,
                                    HaystackOverlap::Overlapping);
  }

  // Get the initial unaligned load out of the way. This will likely overlap
  // with the aligned stuff below, but the overlapped part should effectively
  // be free.
  __m128i haystack = _mm_loadu_si128(Cast128(cur));
  __m128i cmp1 = CmpEq128<TValue>(needle1, haystack);
  __m128i cmp2 = CmpEq128<TValue>(needle2, haystack);
  int cmpMask1 = _mm_movemask_epi8(cmp1);
  int cmpMask2 = _mm_movemask_epi8(cmp2);
  int cmpMask = (cmpMask1 << sizeof(TValue)) & cmpMask2;
  if (cmpMask) {
    return reinterpret_cast<const TValue*>(cur + __builtin_ctz(cmpMask) -
                                           sizeof(TValue));
  }

  // Now we're working with aligned memory. Hooray! \o/
  cur = AlignUp16(cur);

  // The address of the final 48-63 bytes. We overlap this with what we check in
  // our hot loop below to avoid branching. Again, the overlap should be
  // negligible compared with a branch mispredict.
  uintptr_t tailEndPtr = end - 16;
  uintptr_t tailStartPtr = AlignDown16(tailEndPtr);

  __m128i cmpMaskCarry = _mm_set1_epi32(0);
  while (cur < tailStartPtr) {
    uintptr_t a = cur;
    uintptr_t b = cur + 16;
    const TValue* result =
        Check2x2x16Bytes<TValue>(needle1, needle2, a, b, &cmpMaskCarry,
                                 &cmpMaskCarry, HaystackOverlap::Sequential);
    if (result) {
      return result;
    }
    cur += 32;
  }

  uint32_t carry = (cur == tailStartPtr) ? 0xffffffff : 0;
  __m128i wideCarry = Load32BitsIntoXMM(reinterpret_cast<uintptr_t>(&carry));
  cmpMaskCarry = _mm_and_si128(cmpMaskCarry, wideCarry);
  uintptr_t a = tailStartPtr;
  uintptr_t b = tailEndPtr;
  return Check2x2x16Bytes<TValue>(needle1, needle2, a, b, &cmpMaskCarry,
                                  nullptr, HaystackOverlap::Overlapping);
}

const char* SIMD::memchr8SSE2(const char* ptr, char value, size_t length) {
  // Signed chars are just really annoying to do bit logic with. Convert to
  // unsigned at the outermost scope so we don't have to worry about it.
  const unsigned char* uptr = reinterpret_cast<const unsigned char*>(ptr);
  unsigned char uvalue = static_cast<unsigned char>(value);
  const unsigned char* uresult =
      FindInBuffer<unsigned char>(uptr, uvalue, length);
  return reinterpret_cast<const char*>(uresult);
}

const char* SIMD::memchr8(const char* ptr, char value, size_t length) {
  if (supports_avx2()) {
    const unsigned char* uptr = reinterpret_cast<const unsigned char*>(ptr);
    unsigned char uvalue = static_cast<unsigned char>(value);
    const unsigned char* uresult =
        FindInBufferAVX2<unsigned char>(uptr, uvalue, length);
    return reinterpret_cast<const char*>(uresult);
  }
  return memchr8SSE2(ptr, value, length);
}

const char16_t* SIMD::memchr16SSE2(const char16_t* ptr, char16_t value,
                                   size_t length) {
  return FindInBuffer<char16_t>(ptr, value, length);
}

const char16_t* SIMD::memchr16(const char16_t* ptr, char16_t value,
                               size_t length) {
  if (supports_avx2()) {
    return FindInBufferAVX2<char16_t>(ptr, value, length);
  }
  return memchr16SSE2(ptr, value, length);
}

const uint64_t* SIMD::memchr64(const uint64_t* ptr, uint64_t value,
                               size_t length) {
  if (supports_avx2()) {
    return FindInBufferAVX2<uint64_t>(ptr, value, length);
  }
  return FindInBufferNaive<uint64_t>(ptr, value, length);
}

const char* SIMD::memchr2x8(const char* ptr, char v1, char v2, size_t length) {
  // Signed chars are just really annoying to do bit logic with. Convert to
  // unsigned at the outermost scope so we don't have to worry about it.
  const unsigned char* uptr = reinterpret_cast<const unsigned char*>(ptr);
  unsigned char uv1 = static_cast<unsigned char>(v1);
  unsigned char uv2 = static_cast<unsigned char>(v2);
  const unsigned char* uresult =
      FindTwoInBuffer<unsigned char>(uptr, uv1, uv2, length);
  return reinterpret_cast<const char*>(uresult);
}

const char16_t* SIMD::memchr2x16(const char16_t* ptr, char16_t v1, char16_t v2,
                                 size_t length) {
  return FindTwoInBuffer<char16_t>(ptr, v1, v2, length);
}

#else

#  include <cstring>

const char* SIMD::memchr8(const char* ptr, char value, size_t length) {
  const void* result = ::memchr(reinterpret_cast<const void*>(ptr),
                                static_cast<int>(value), length);
  return reinterpret_cast<const char*>(result);
}

const char* SIMD::memchr8SSE2(const char* ptr, char value, size_t length) {
  return memchr8(ptr, value, length);
}

const char16_t* SIMD::memchr16(const char16_t* ptr, char16_t value,
                               size_t length) {
  return FindInBufferNaive<char16_t>(ptr, value, length);
}

const char16_t* SIMD::memchr16SSE2(const char16_t* ptr, char16_t value,
                                   size_t length) {
  return memchr16(ptr, value, length);
}

const uint64_t* SIMD::memchr64(const uint64_t* ptr, uint64_t value,
                               size_t length) {
  return FindInBufferNaive<uint64_t>(ptr, value, length);
}

const char* SIMD::memchr2x8(const char* ptr, char v1, char v2, size_t length) {
  const char* end = ptr + length - 1;
  while (ptr < end) {
    ptr = memchr8(ptr, v1, end - ptr);
    if (!ptr) {
      return nullptr;
    }
    if (ptr[1] == v2) {
      return ptr;
    }
    ptr++;
  }
  return nullptr;
}

const char16_t* SIMD::memchr2x16(const char16_t* ptr, char16_t v1, char16_t v2,
                                 size_t length) {
  const char16_t* end = ptr + length - 1;
  while (ptr < end) {
    ptr = memchr16(ptr, v1, end - ptr);
    if (!ptr) {
      return nullptr;
    }
    if (ptr[1] == v2) {
      return ptr;
    }
    ptr++;
  }
  return nullptr;
}

#endif

}  // namespace mozilla
