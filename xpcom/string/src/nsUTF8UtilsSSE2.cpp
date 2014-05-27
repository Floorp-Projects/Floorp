/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsAlgorithm.h"
#include <emmintrin.h>
#include <nsUTF8Utils.h>

void
LossyConvertEncoding16to8::write_sse2(const char16_t* aSource,
                                      uint32_t aSourceLength)
{
  char* dest = mDestination;

  // Align source to a 16-byte boundary.
  uint32_t i = 0;
  uint32_t alignLen =
    XPCOM_MIN<uint32_t>(aSourceLength, uint32_t(-NS_PTR_TO_INT32(aSource) & 0xf) / sizeof(char16_t));
  for (; i < alignLen; ++i) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
  }

  // Walk 64 bytes (four XMM registers) at a time.
  __m128i vectmask = _mm_set1_epi16(0x00ff);
  for (; aSourceLength - i > 31; i += 32) {
    __m128i source1 = _mm_load_si128(reinterpret_cast<const __m128i*>(aSource + i));
    source1 = _mm_and_si128(source1, vectmask);

    __m128i source2 = _mm_load_si128(reinterpret_cast<const __m128i*>(aSource + i + 8));
    source2 = _mm_and_si128(source2, vectmask);

    __m128i source3 = _mm_load_si128(reinterpret_cast<const __m128i*>(aSource + i + 16));
    source3 = _mm_and_si128(source3, vectmask);

    __m128i source4 = _mm_load_si128(reinterpret_cast<const __m128i*>(aSource + i + 24));
    source4 = _mm_and_si128(source4, vectmask);


    // Pack the source data.  SSE2 views this as a saturating uint16_t to
    // uint8_t conversion, but since we masked off the high-order byte of every
    // uint16_t, we're really just grabbing the low-order bytes of source1 and
    // source2.
    __m128i packed1 = _mm_packus_epi16(source1, source2);
    __m128i packed2 = _mm_packus_epi16(source3, source4);

    // This store needs to be unaligned since there's no guarantee that the
    // alignment we did above for the source will align the destination.
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i),      packed1);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i + 16), packed2);
  }

  // Finish up the rest.
  for (; i < aSourceLength; ++i) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
  }

  mDestination += i;
}

void
LossyConvertEncoding8to16::write_sse2(const char* aSource,
                                      uint32_t aSourceLength)
{
  char16_t* dest = mDestination;

  // Align source to a 16-byte boundary.  We choose to align source rather than
  // dest because we'd rather have our loads than our stores be fast. You have
  // to wait for a load to complete, but you can keep on moving after issuing a
  // store.
  uint32_t i = 0;
  uint32_t alignLen = XPCOM_MIN(aSourceLength, uint32_t(-NS_PTR_TO_INT32(aSource) & 0xf));
  for (; i < alignLen; ++i) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
  }

  // Walk 32 bytes (two XMM registers) at a time.
  for (; aSourceLength - i > 31; i += 32) {
    __m128i source1 = _mm_load_si128(reinterpret_cast<const __m128i*>(aSource + i));
    __m128i source2 = _mm_load_si128(reinterpret_cast<const __m128i*>(aSource + i + 16));

    // Interleave 0s in with the bytes of source to create lo and hi.
    __m128i lo1 = _mm_unpacklo_epi8(source1, _mm_setzero_si128());
    __m128i hi1 = _mm_unpackhi_epi8(source1, _mm_setzero_si128());
    __m128i lo2 = _mm_unpacklo_epi8(source2, _mm_setzero_si128());
    __m128i hi2 = _mm_unpackhi_epi8(source2, _mm_setzero_si128());

    // store lo and hi into dest.
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i),      lo1);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i + 8),  hi1);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i + 16), lo2);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + i + 24), hi2);
  }

  // Finish up whatever's left.
  for (; i < aSourceLength; ++i) {
    dest[i] = static_cast<unsigned char>(aSource[i]);
  }

  mDestination += i;
}
