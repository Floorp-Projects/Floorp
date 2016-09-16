/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <emmintrin.h>

#include "nsReadableUtilsImpl.h"

namespace mozilla {
namespace SSE2 {

static inline bool
is_zero (__m128i x)
{
  return
    _mm_movemask_epi8(_mm_cmpeq_epi8(x, _mm_setzero_si128())) == 0xffff;
}

int32_t
FirstNonASCII(const char16_t* aBegin, const char16_t* aEnd)
{
  const size_t kNumUnicharsPerVector = sizeof(__m128i) / sizeof(char16_t);
  typedef NonASCIIParameters<sizeof(size_t)> p;
  const size_t kMask = p::mask();
  const uintptr_t kXmmAlignMask = 0xf;
  const uint16_t kShortMask = 0xff80;
  const size_t kNumUnicharsPerWord = p::numUnicharsPerWord();

  const char16_t* idx = aBegin;

  // Align ourselves to a 16-byte boundary as required by _mm_load_si128
  for (; idx != aEnd && ((uintptr_t(idx) & kXmmAlignMask) != 0); idx++) {
    if (!IsASCII(*idx)) {
      return idx - aBegin;
    }
  }

  // Check one XMM register (16 bytes) at a time.
  const char16_t* vectWalkEnd = aligned(aEnd, kXmmAlignMask);
  __m128i vectmask = _mm_set1_epi16(static_cast<int16_t>(kShortMask));
  for (; idx != vectWalkEnd; idx += kNumUnicharsPerVector) {
    const __m128i vect = *reinterpret_cast<const __m128i*>(idx);
    if (!is_zero(_mm_and_si128(vect, vectmask))) {
      return idx - aBegin;
    }
  }

  // Check one word at a time.
  const char16_t* wordWalkEnd = aligned(aEnd, p::alignMask());
  for(; idx != wordWalkEnd; idx += kNumUnicharsPerWord) {
    const size_t word = *reinterpret_cast<const size_t*>(idx);
    if (word & kMask) {
      return idx - aBegin;
    }
  }

  // Take care of the remainder one character at a time.
  for (; idx != aEnd; idx++) {
    if (!IsASCII(*idx)) {
      return idx - aBegin;
    }
  }

  return -1;
}

} // namespace SSE2
} // namespace mozilla
