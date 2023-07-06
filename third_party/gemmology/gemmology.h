#ifndef GEMMOLOGY_H
#define GEMMOLOGY_H

#include "gemmology_fwd.h"

#include <cstdint>
#include <cstring>
#include <tuple>

#include <xsimd/xsimd.hpp>

namespace gemmology {

namespace {

//
// Arch specific implementation of various elementary operations
//

namespace kernel {

#ifdef __AVX512BW__
template <class Arch>
std::tuple<xsimd::batch<int8_t, Arch>, xsimd::batch<int8_t, Arch>>
interleave(xsimd::batch<int8_t, Arch> first, xsimd::batch<int8_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return {_mm512_unpacklo_epi8(first, second),
          _mm512_unpackhi_epi8(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int16_t, Arch>, xsimd::batch<int16_t, Arch>>
interleave(xsimd::batch<int16_t, Arch> first,
           xsimd::batch<int16_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return {_mm512_unpacklo_epi16(first, second),
          _mm512_unpackhi_epi16(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
interleave(xsimd::batch<int32_t, Arch> first,
           xsimd::batch<int32_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return {_mm512_unpacklo_epi32(first, second),
          _mm512_unpackhi_epi32(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int64_t, Arch>, xsimd::batch<int64_t, Arch>>
interleave(xsimd::batch<int64_t, Arch> first,
           xsimd::batch<int64_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return {_mm512_unpacklo_epi64(first, second),
          _mm512_unpackhi_epi64(first, second)};
}

template <class Arch>
xsimd::batch<int8_t, Arch>
deinterleave(xsimd::batch<int16_t, Arch> first,
             xsimd::batch<int16_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return _mm512_packs_epi16(first, second);
}

template <class Arch>
xsimd::batch<int16_t, Arch>
deinterleave(xsimd::batch<int32_t, Arch> first,
             xsimd::batch<int32_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return _mm512_packs_epi32(first, second);
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
madd(xsimd::batch<int16_t, Arch> x, xsimd::batch<int16_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return _mm512_madd_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return _mm512_maddubs_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<int8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  return _mm512_madd_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int32_t, xsimd::avx2>
PermuteSummer(xsimd::batch<int32_t, Arch> pack0123,
              xsimd::batch<int32_t, Arch> pack4567,
              xsimd::kernel::requires_arch<xsimd::avx512bw>) {
  // Form [0th 128-bit register of pack0123, 0st 128-bit register of pack4567,
  // 2nd 128-bit register of pack0123, 2nd 128-bit register of pack4567]
  __m512i mix0 =
      _mm512_mask_permutex_epi64(pack0123, 0xcc, pack4567, (0 << 4) | (1 << 6));
  // Form [1st 128-bit register of pack0123, 1st 128-bit register of pack4567,
  // 3rd 128-bit register of pack0123, 3rd 128-bit register of pack4567]
  __m512i mix1 =
      _mm512_mask_permutex_epi64(pack4567, 0x33, pack0123, 2 | (3 << 2));
  __m512i added = _mm512_add_epi32(mix0, mix1);
  // Now we have 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7.
  // Fold register over itself.
  return _mm256_add_epi32(_mm512_castsi512_si256(added),
                          _mm512_extracti64x4_epi64(added, 1));
}
#endif

#ifdef __AVX2__
template <class Arch>
std::tuple<xsimd::batch<int8_t, Arch>, xsimd::batch<int8_t, Arch>>
interleave(xsimd::batch<int8_t, Arch> first, xsimd::batch<int8_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx2>) {
  return {_mm256_unpacklo_epi8(first, second),
          _mm256_unpackhi_epi8(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int16_t, Arch>, xsimd::batch<int16_t, Arch>>
interleave(xsimd::batch<int16_t, Arch> first,
           xsimd::batch<int16_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx2>) {
  return {_mm256_unpacklo_epi16(first, second),
          _mm256_unpackhi_epi16(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
interleave(xsimd::batch<int32_t, Arch> first,
           xsimd::batch<int32_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx2>) {
  return {_mm256_unpacklo_epi32(first, second),
          _mm256_unpackhi_epi32(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int64_t, Arch>, xsimd::batch<int64_t, Arch>>
interleave(xsimd::batch<int64_t, Arch> first,
           xsimd::batch<int64_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::avx2>) {
  return {_mm256_unpacklo_epi64(first, second),
          _mm256_unpackhi_epi64(first, second)};
}

template <class Arch>
xsimd::batch<int8_t, Arch>
deinterleave(xsimd::batch<int16_t, Arch> first,
             xsimd::batch<int16_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::avx2>) {
  return _mm256_packs_epi16(first, second);
}

template <class Arch>
xsimd::batch<int16_t, Arch>
deinterleave(xsimd::batch<int32_t, Arch> first,
             xsimd::batch<int32_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::avx2>) {
  return _mm256_packs_epi32(first, second);
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
madd(xsimd::batch<int16_t, Arch> x, xsimd::batch<int16_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::avx2>) {
  return _mm256_madd_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::avx2>) {
  return _mm256_maddubs_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<int8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::avx2>) {
  return _mm256_maddubs_epi16(xsimd::abs(x), _mm256_sign_epi8(y, x));
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
PermuteSummer(xsimd::batch<int32_t, Arch> pack0123,
              xsimd::batch<int32_t, Arch> pack4567,
              xsimd::kernel::requires_arch<xsimd::avx2>) {
  // This instruction generates 1s 2s 3s 4s 5f 6f 7f 8f
  __m256i rev = _mm256_permute2f128_si256(pack0123, pack4567, 0x21);
  // This instruction generates 1f 2f 3f 4f 5s 6s 7s 8s
  __m256i blended = _mm256_blend_epi32(pack0123, pack4567, 0xf0);
  return _mm256_add_epi32(rev, blended);
}
#endif

#ifdef __SSSE3__

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::ssse3>) {
  return _mm_maddubs_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<int8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::ssse3>) {
  return _mm_maddubs_epi16(xsimd::abs(x), _mm_sign_epi8(y, x));
}
#endif

#ifdef __SSE2__
template <class Arch>
std::tuple<xsimd::batch<int8_t, Arch>, xsimd::batch<int8_t, Arch>>
interleave(xsimd::batch<int8_t, Arch> first, xsimd::batch<int8_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::sse2>) {
  return {_mm_unpacklo_epi8(first, second), _mm_unpackhi_epi8(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int16_t, Arch>, xsimd::batch<int16_t, Arch>>
interleave(xsimd::batch<int16_t, Arch> first,
           xsimd::batch<int16_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::sse2>) {
  return {_mm_unpacklo_epi16(first, second), _mm_unpackhi_epi16(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
interleave(xsimd::batch<int32_t, Arch> first,
           xsimd::batch<int32_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::sse2>) {
  return {_mm_unpacklo_epi32(first, second), _mm_unpackhi_epi32(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int64_t, Arch>, xsimd::batch<int64_t, Arch>>
interleave(xsimd::batch<int64_t, Arch> first,
           xsimd::batch<int64_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::sse2>) {
  return {_mm_unpacklo_epi64(first, second), _mm_unpackhi_epi64(first, second)};
}

template <class Arch>
xsimd::batch<int8_t, Arch>
deinterleave(xsimd::batch<int16_t, Arch> first,
             xsimd::batch<int16_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::sse2>) {
  return _mm_packs_epi16(first, second);
}

template <class Arch>
xsimd::batch<int16_t, Arch>
deinterleave(xsimd::batch<int32_t, Arch> first,
             xsimd::batch<int32_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::sse2>) {
  return _mm_packs_epi32(first, second);
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
madd(xsimd::batch<int16_t, Arch> x, xsimd::batch<int16_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::sse2>) {
  return _mm_madd_epi16(x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<uint8_t, Arch> a, xsimd::batch<int8_t, Arch> b,
     xsimd::kernel::requires_arch<xsimd::sse2>) {
  // Adapted from
  // https://stackoverflow.com/questions/19957709/how-to-achieve-8bit-madd-using-sse2
  // a = 0x00 0x01 0xFE 0x04 ...
  // b = 0x00 0x02 0x80 0x84 ...

  // To extend signed 8-bit value, MSB has to be set to 0xFF
  __m128i sign_mask_b = _mm_cmplt_epi8(b, _mm_setzero_si128());

  // sign_mask_b = 0x00 0x00 0xFF 0xFF ...

  // Unpack positives with 0x00, negatives with 0xFF
  __m128i a_epi16_l = _mm_unpacklo_epi8(a, _mm_setzero_si128());
  __m128i a_epi16_h = _mm_unpackhi_epi8(a, _mm_setzero_si128());
  __m128i b_epi16_l = _mm_unpacklo_epi8(b, sign_mask_b);
  __m128i b_epi16_h = _mm_unpackhi_epi8(b, sign_mask_b);

  // Here - valid 16-bit signed integers corresponding to the 8-bit input
  // a_epi16_l = 0x00 0x00 0x01 0x00 0xFE 0xFF 0x04 0x00 ...

  // Get the a[i] * b[i] + a[i+1] * b[i+1] for both low and high parts
  __m128i madd_epi32_l = _mm_madd_epi16(a_epi16_l, b_epi16_l);
  __m128i madd_epi32_h = _mm_madd_epi16(a_epi16_h, b_epi16_h);

  // Now go back from 32-bit values to 16-bit values & signed saturate
  return _mm_packs_epi32(madd_epi32_l, madd_epi32_h);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<int8_t, Arch> a, xsimd::batch<int8_t, Arch> b,
     xsimd::kernel::requires_arch<xsimd::sse2>) {
  // adapted
  // https://stackoverflow.com/questions/19957709/how-to-achieve-8bit-madd-using-sse2
  // a = 0x00 0x01 0xFE 0x04 ...
  // b = 0x00 0x02 0x80 0x84 ...

  // To extend signed 8-bit value, MSB has to be set to 0xFF
  __m128i sign_mask_a = _mm_cmplt_epi8(a, _mm_setzero_si128());
  __m128i sign_mask_b = _mm_cmplt_epi8(b, _mm_setzero_si128());

  // sign_mask_a = 0x00 0x00 0xFF 0x00 ...
  // sign_mask_b = 0x00 0x00 0xFF 0xFF ...

  // Unpack positives with 0x00, negatives with 0xFF
  __m128i a_epi16_l = _mm_unpacklo_epi8(a, sign_mask_a);
  __m128i a_epi16_h = _mm_unpackhi_epi8(a, sign_mask_a);
  __m128i b_epi16_l = _mm_unpacklo_epi8(b, sign_mask_b);
  __m128i b_epi16_h = _mm_unpackhi_epi8(b, sign_mask_b);

  // Here - valid 16-bit signed integers corresponding to the 8-bit input
  // a_epi16_l = 0x00 0x00 0x01 0x00 0xFE 0xFF 0x04 0x00 ...

  // Get the a[i] * b[i] + a[i+1] * b[i+1] for both low and high parts
  __m128i madd_epi32_l = _mm_madd_epi16(a_epi16_l, b_epi16_l);
  __m128i madd_epi32_h = _mm_madd_epi16(a_epi16_h, b_epi16_h);

  // Now go back from 32-bit values to 16-bit values & signed saturate
  return _mm_packs_epi32(madd_epi32_l, madd_epi32_h);
}

template <class Arch>
inline std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
PermuteSummer(xsimd::batch<int32_t, Arch> pack0123,
              xsimd::batch<int32_t, Arch> pack4567,
              xsimd::kernel::requires_arch<xsimd::sse2>) {
  return {pack0123, pack4567};
}

#endif

#if __ARM_ARCH >= 7
template <class Arch>
std::tuple<xsimd::batch<int8_t, Arch>, xsimd::batch<int8_t, Arch>>
interleave(xsimd::batch<int8_t, Arch> first, xsimd::batch<int8_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon>) {
  int8x8_t first_lo = vget_low_s8(first);
  int8x8_t second_lo = vget_low_s8(second);
  int8x8x2_t result_lo = vzip_s8(first_lo, second_lo);
  int8x8_t first_hi = vget_high_s8(first);
  int8x8_t second_hi = vget_high_s8(second);
  int8x8x2_t result_hi = vzip_s8(first_hi, second_hi);
  return {vcombine_s8(result_lo.val[0], result_lo.val[1]),
          vcombine_s8(result_hi.val[0], result_hi.val[1])};
}

template <class Arch>
std::tuple<xsimd::batch<int16_t, Arch>, xsimd::batch<int16_t, Arch>>
interleave(xsimd::batch<int16_t, Arch> first,
           xsimd::batch<int16_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon>) {
  int16x4_t first_lo = vget_low_s16(first);
  int16x4_t second_lo = vget_low_s16(second);
  int16x4x2_t result_lo = vzip_s16(first_lo, second_lo);
  int16x4_t first_hi = vget_high_s16(first);
  int16x4_t second_hi = vget_high_s16(second);
  int16x4x2_t result_hi = vzip_s16(first_hi, second_hi);
  return {vcombine_s16(result_lo.val[0], result_lo.val[1]),
          vcombine_s16(result_hi.val[0], result_hi.val[1])};
}

template <class Arch>
std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
interleave(xsimd::batch<int32_t, Arch> first,
           xsimd::batch<int32_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon>) {
  int32x2_t first_lo = vget_low_s32(first);
  int32x2_t second_lo = vget_low_s32(second);
  int32x2x2_t result_lo = vzip_s32(first_lo, second_lo);
  int32x2_t first_hi = vget_high_s32(first);
  int32x2_t second_hi = vget_high_s32(second);
  int32x2x2_t result_hi = vzip_s32(first_hi, second_hi);
  return {vcombine_s32(result_lo.val[0], result_lo.val[1]),
          vcombine_s32(result_hi.val[0], result_hi.val[1])};
}

template <class Arch>
std::tuple<xsimd::batch<int64_t, Arch>, xsimd::batch<int64_t, Arch>>
interleave(xsimd::batch<int64_t, Arch> first,
           xsimd::batch<int64_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon>) {
  int64x1_t first_lo = vget_low_s64(first);
  int64x1_t second_lo = vget_low_s64(second);
  int64x1_t first_hi = vget_high_s64(first);
  int64x1_t second_hi = vget_high_s64(second);
  return {vcombine_s64(first_lo, second_lo), vcombine_s64(first_hi, second_hi)};
}

template <class Arch>
xsimd::batch<int8_t, Arch>
deinterleave(xsimd::batch<int16_t, Arch> first,
             xsimd::batch<int16_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::neon>) {

  return vcombine_s8(vqmovn_s16(first), vqmovn_s16(second));
}

template <class Arch>
xsimd::batch<int16_t, Arch>
deinterleave(xsimd::batch<int32_t, Arch> first,
             xsimd::batch<int32_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::neon>) {
  return vcombine_s16(vqmovn_s32(first), vqmovn_s32(second));
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
madd(xsimd::batch<int16_t, Arch> x, xsimd::batch<int16_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::neon>) {

  int32x4_t low = vmull_s16(vget_low_s16(x), vget_low_s16(y));
  int32x4_t high = vmull_s16(vget_high_s16(x), vget_high_s16(y));

  int32x2_t low_sum = vpadd_s32(vget_low_s32(low), vget_high_s32(low));
  int32x2_t high_sum = vpadd_s32(vget_low_s32(high), vget_high_s32(high));

  return vcombine_s32(low_sum, high_sum);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::neon>) {

  // This would be much simpler if x86 would choose to zero extend OR sign
  // extend, not both. This could probably be optimized better.

  // Zero extend x
  int16x8_t x_odd =
      vreinterpretq_s16_u16(vshrq_n_u16(vreinterpretq_u16_u8(x), 8));
  int16x8_t x_even = vreinterpretq_s16_u16(
      vbicq_u16(vreinterpretq_u16_u8(x), vdupq_n_u16(0xff00)));

  // Sign extend by shifting left then shifting right.
  int16x8_t y_even = vshrq_n_s16(vshlq_n_s16(vreinterpretq_s16_s8(y), 8), 8);
  int16x8_t y_odd = vshrq_n_s16(vreinterpretq_s16_s8(y), 8);

  // multiply
  int16x8_t prod1 = vmulq_s16(x_even, y_even);
  int16x8_t prod2 = vmulq_s16(x_odd, y_odd);

  // saturated add
  return vqaddq_s16(prod1, prod2);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<int8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::neon>) {
  int16x8_t low = vmull_s8(vget_low_s8(x), vget_low_s8(y));
  int16x8_t high = vmull_s8(vget_high_s8(x), vget_high_s8(y));

  int16x4_t low_sum = vpadd_s16(vget_low_s16(low), vget_high_s16(low));
  int16x4_t high_sum = vpadd_s16(vget_low_s16(high), vget_high_s16(high));

  return vcombine_s16(low_sum, high_sum);
}

template <class Arch>
inline std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
PermuteSummer(xsimd::batch<int32_t, Arch> pack0123,
              xsimd::batch<int32_t, Arch> pack4567,
              xsimd::kernel::requires_arch<xsimd::neon>) {
  return {pack0123, pack4567};
}
#endif

#ifdef __aarch64__
template <class Arch>
std::tuple<xsimd::batch<int8_t, Arch>, xsimd::batch<int8_t, Arch>>
interleave(xsimd::batch<int8_t, Arch> first, xsimd::batch<int8_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon64>) {
  return {vzip1q_s8(first, second), vzip2q_s8(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int16_t, Arch>, xsimd::batch<int16_t, Arch>>
interleave(xsimd::batch<int16_t, Arch> first,
           xsimd::batch<int16_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon64>) {
  return {vzip1q_s16(first, second), vzip2q_s16(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>>
interleave(xsimd::batch<int32_t, Arch> first,
           xsimd::batch<int32_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon64>) {
  return {vzip1q_s32(first, second), vzip2q_s32(first, second)};
}

template <class Arch>
std::tuple<xsimd::batch<int64_t, Arch>, xsimd::batch<int64_t, Arch>>
interleave(xsimd::batch<int64_t, Arch> first,
           xsimd::batch<int64_t, Arch> second,
           xsimd::kernel::requires_arch<xsimd::neon64>) {
  return {vzip1q_s64(first, second), vzip2q_s64(first, second)};
}

template <class Arch>
xsimd::batch<int8_t, Arch>
deinterleave(xsimd::batch<int16_t, Arch> first,
             xsimd::batch<int16_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::neon64>) {
  return vcombine_s8(vqmovn_s16(first), vqmovn_s16(second));
}

template <class Arch>
xsimd::batch<int16_t, Arch>
deinterleave(xsimd::batch<int32_t, Arch> first,
             xsimd::batch<int32_t, Arch> second,
             xsimd::kernel::requires_arch<xsimd::neon64>) {
  return vcombine_s16(vqmovn_s32(first), vqmovn_s32(second));
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
madd(xsimd::batch<int16_t, Arch> x, xsimd::batch<int16_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::neon64>) {
  int32x4_t low = vmull_s16(vget_low_s16(x), vget_low_s16(y));
  return vmlal_high_s16(low, x, y);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::neon64>) {

  int16x8_t tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(x))),
                           vmovl_s8(vget_low_s8(y)));
  int16x8_t th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(x))),
                           vmovl_s8(vget_high_s8(y)));
  return vqaddq_s16(vuzp1q_s16(tl, th), vuzp2q_s16(tl, th));
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
maddw(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
      xsimd::kernel::requires_arch<xsimd::neon64>) {

  int16x8_t tl = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(x))),
                           vmovl_s8(vget_low_s8(y)));
  int16x8_t th = vmulq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(x))),
                           vmovl_s8(vget_high_s8(y)));
  int32x4_t pl = vpaddlq_s16(tl);
  int32x4_t ph = vpaddlq_s16(th);
  return vpaddq_s32(pl, ph);
}

template <class Arch>
inline xsimd::batch<int16_t, Arch>
madd(xsimd::batch<int8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
     xsimd::kernel::requires_arch<xsimd::neon64>) {
  int16x8_t low = vmull_s8(vget_low_s8(x), vget_low_s8(y));
  return vmlal_high_s8(low, x, y);
}

#endif

template <class Arch>
inline xsimd::batch<int32_t, Arch>
maddw(xsimd::batch<uint8_t, Arch> x, xsimd::batch<int8_t, Arch> y,
      xsimd::kernel::requires_arch<xsimd::generic>) {
  return madd(xsimd::batch<int16_t, Arch>(1), madd(x, y, Arch{}), Arch{});
}

} // namespace kernel

//
// Generic dispatcher for interleave, deinterleave madd and PermuteSummer
//

template <class T, class Arch>
std::tuple<xsimd::batch<T, Arch>, xsimd::batch<T, Arch>>
interleave(xsimd::batch<T, Arch> first, xsimd::batch<T, Arch> second) {
  return kernel::interleave(first, second, Arch{});
}

template <class Arch>
xsimd::batch<int8_t, Arch> deinterleave(xsimd::batch<int16_t, Arch> first,
                                        xsimd::batch<int16_t, Arch> second) {
  return kernel::deinterleave(first, second, Arch{});
}
template <class Arch>
xsimd::batch<int16_t, Arch> deinterleave(xsimd::batch<int32_t, Arch> first,
                                         xsimd::batch<int32_t, Arch> second) {
  return kernel::deinterleave(first, second, Arch{});
}

template <class Arch>
inline xsimd::batch<int32_t, Arch> madd(xsimd::batch<int16_t, Arch> x,
                                        xsimd::batch<int16_t, Arch> y) {
  return kernel::madd(x, y, Arch{});
}
template <class Arch>
inline xsimd::batch<int16_t, Arch> madd(xsimd::batch<int8_t, Arch> x,
                                        xsimd::batch<int8_t, Arch> y) {
  return kernel::madd(x, y, Arch{});
}
template <class Arch>
inline xsimd::batch<int16_t, Arch> madd(xsimd::batch<uint8_t, Arch> x,
                                        xsimd::batch<int8_t, Arch> y) {
  return kernel::madd(x, y, Arch{});
}
template <class Arch>
inline xsimd::batch<int32_t, Arch> maddw(xsimd::batch<uint8_t, Arch> x,
                                         xsimd::batch<int8_t, Arch> y) {
  return kernel::maddw(x, y, Arch{});
}

template <class Arch>
inline auto PermuteSummer(xsimd::batch<int32_t, Arch> pack0123,
                          xsimd::batch<int32_t, Arch> pack4567)
    -> decltype(kernel::PermuteSummer(pack0123, pack4567, Arch{})) {
  return kernel::PermuteSummer(pack0123, pack4567, Arch{});
}

template <class Arch>
inline xsimd::batch<int32_t, Arch> Pack0123(xsimd::batch<int32_t, Arch> sum0,
                                      xsimd::batch<int32_t, Arch> sum1,
                                      xsimd::batch<int32_t, Arch> sum2,
                                      xsimd::batch<int32_t, Arch> sum3) {
  std::tie(sum0, sum1) = interleave(sum0, sum1);
  auto pack01 = sum0 + sum1;
  std::tie(sum2, sum3) = interleave(sum2, sum3);
  auto pack23 = sum2 + sum3;

  auto packed = interleave(xsimd::bitwise_cast<int64_t>(pack01),
                           xsimd::bitwise_cast<int64_t>(pack23));
  return xsimd::bitwise_cast<int32_t>(std::get<0>(packed)) +
         xsimd::bitwise_cast<int32_t>(std::get<1>(packed));
}

template <class Arch>
static inline xsimd::batch<int32_t, Arch>
quantize(xsimd::batch<float, Arch> input,
         xsimd::batch<float, Arch> quant_mult) {
  return xsimd::nearbyint_as_int(input * quant_mult);
}

template <class Arch>
inline xsimd::batch<int32_t, Arch>
QuantizerGrab(const float *input, xsimd::batch<float, Arch> quant_mult_reg) {
  return quantize(xsimd::batch<float, Arch>::load_unaligned(input),
                  quant_mult_reg);
}

#ifdef __AVX512BW__
inline __m512 Concat(const __m256 first, const __m256 second) {
  // INTGEMM_AVX512DQ but that goes with INTGEMM_AVX512BW anyway.
  return _mm512_insertf32x8(_mm512_castps256_ps512(first), second, 1);
}

// Like QuantizerGrab, but allows 32-byte halves (i.e. 8 columns) to be
// controlled independently.
/* Only INTGEMM_AVX512F is necessary but due to GCC 5.4 bug we have to set
 * INTGEMM_AVX512BW */
inline __m512i QuantizerGrabHalves(const float *input0, const float *input1,
                                   const __m512 quant_mult_reg) {
  __m512 appended = Concat(_mm256_loadu_ps(input0), _mm256_loadu_ps(input1));
  appended = _mm512_mul_ps(appended, quant_mult_reg);
  return _mm512_cvtps_epi32(appended);
}
#else
template <class Arch>
inline xsimd::batch<int32_t, Arch>
QuantizerGrabHalves(const float *input0, const float *input1,
                    xsimd::batch<float, Arch> quant_mult_reg);
#endif

/* Read 8 floats at a time from input0, input1, input2, and input3.  Quantize
 * them to 8-bit by multiplying with quant_mult_reg then rounding. Concatenate
 * the result into one register and return it.
 */
class QuantizeTile8 {
  template <class Arch> struct Tiler {
    static constexpr uint32_t get(std::size_t i, std::size_t n) {
      size_t factor = xsimd::batch<float, Arch>::size / 4;
      return (i % factor) * 4 + i / factor;
    }
  };

public:
  template <class Arch>
  static inline xsimd::batch<int8_t, Arch>
  Consecutive(xsimd::batch<float, Arch> quant_mult, const float *input) {
    return Tile(quant_mult, input + 0 * xsimd::batch<float, Arch>::size,
                input + 1 * xsimd::batch<float, Arch>::size,
                input + 2 * xsimd::batch<float, Arch>::size,
                input + 3 * xsimd::batch<float, Arch>::size);
  }

  template <class Arch>
  static inline xsimd::batch<uint8_t, Arch>
  ConsecutiveU(xsimd::batch<float, Arch> quant_mult, const float *input) {
    return TileU(quant_mult, input + 0 * xsimd::batch<float, Arch>::size,
                 input + 1 * xsimd::batch<float, Arch>::size,
                 input + 2 * xsimd::batch<float, Arch>::size,
                 input + 3 * xsimd::batch<float, Arch>::size);
  }

  template <class Arch>
  static inline xsimd::batch<int8_t, Arch>
  ConsecutiveWithWrapping(xsimd::batch<float, Arch> quant_mult,
                          const float *input, size_t cols_left, size_t cols,
                          size_t row_step) {
    using batchf32 = xsimd::batch<float, Arch>;
    const float *inputs[4];
    for (size_t i = 0; i < std::size(inputs); ++i) {
      while (cols_left < batchf32::size) {
        input += cols * (row_step - 1);
        cols_left += cols;
      }
      inputs[i] = input;
      input += batchf32::size;
      cols_left -= batchf32::size;
    }
    return Tile(quant_mult, inputs[0], inputs[1], inputs[2], inputs[3]);
  }

  template <class Arch>
  static inline xsimd::batch<int8_t, Arch>
  ForReshape(xsimd::batch<float, Arch> quant_mult, const float *input,
             size_t cols) {
    using batchf32 = xsimd::batch<float, Arch>;
    using batch8 = xsimd::batch<int8_t, Arch>;
    using batch16 = xsimd::batch<int16_t, Arch>;
    using batch32 = xsimd::batch<int32_t, Arch>;
    using ubatch32 = xsimd::batch<uint32_t, Arch>;

    // Put higher rows in the second half of the register.  These will jumble
    // around in the same way then conveniently land in the right place.
    if constexpr (batchf32::size == 16) {
      const batch8 neg127(-127);
      // In reverse order: grabbing the first 32-bit values from each 128-bit
      // register, then the second 32-bit values, etc. Grab 4 registers at a
      // time in 32-bit format.
      batch32 g0 =
          QuantizerGrabHalves(input + 0 * cols, input + 2 * cols, quant_mult);
      batch32 g1 =
          QuantizerGrabHalves(input + 16 * cols, input + 18 * cols, quant_mult);
      batch32 g2 =
          QuantizerGrabHalves(input + 32 * cols, input + 34 * cols, quant_mult);
      batch32 g3 =
          QuantizerGrabHalves(input + 48 * cols, input + 50 * cols, quant_mult);

      // Pack 32-bit to 16-bit.
      batch16 packed0 = deinterleave(g0, g1);
      batch16 packed1 = deinterleave(g2, g3);
      // Pack 16-bit to 8-bit.
      batch8 packed = deinterleave(packed0, packed1);
      // Ban -128.
      packed = xsimd::max(packed, neg127);

      return xsimd::bitwise_cast<int8_t>(
          xsimd::swizzle(xsimd::bitwise_cast<int32_t>(packed),
                         xsimd::make_batch_constant<ubatch32, Tiler<Arch>>()));
    } else if constexpr (batchf32::size == 8)
      return Tile(quant_mult, input, input + 2 * cols, input + 16 * cols,
                  input + 18 * cols);
    else if constexpr (batchf32::size == 4)
      // Skip a row.
      return Tile(quant_mult, input, input + 4, input + 2 * cols,
                  input + 2 * cols + 4);
    else
      return {};
  }

  template <class Arch>
  static inline xsimd::batch<int8_t, Arch>
  Tile(xsimd::batch<float, Arch> quant_mult, const float *input0,
       const float *input1, const float *input2, const float *input3) {
    using batch8 = xsimd::batch<int8_t, Arch>;
    using batch16 = xsimd::batch<int16_t, Arch>;
    using batch32 = xsimd::batch<int32_t, Arch>;
    using ubatch32 = xsimd::batch<uint32_t, Arch>;

    const batch8 neg127(-127);
    // Grab 4 registers at a time in 32-bit format.
    batch32 g0 = QuantizerGrab(input0, quant_mult);
    batch32 g1 = QuantizerGrab(input1, quant_mult);
    batch32 g2 = QuantizerGrab(input2, quant_mult);
    batch32 g3 = QuantizerGrab(input3, quant_mult);
    // Pack 32-bit to 16-bit.
    batch16 packed0 = deinterleave(g0, g1);
    batch16 packed1 = deinterleave(g2, g3);
    // Pack 16-bit to 8-bit.
    batch8 packed = deinterleave(packed0, packed1);
    // Ban -128.
    packed = xsimd::max(packed, neg127);

    if constexpr (batch32::size == 4)
      return packed;
    // Currently in 0 1 2 3 8 9 10 11 16 17 18 19 24 25 26 27 4 5 6 7 12 13 14
    // 15 20 21 22 23 28 29 30 31 Or as 32-bit integers 0 2 4 6 1 3 5 7
    // Technically this could be removed so long as the rows are bigger than 16
    // and the values are only used for GEMM.
    return xsimd::bitwise_cast<int8_t>(
        xsimd::swizzle(xsimd::bitwise_cast<int32_t>(packed),
                       xsimd::make_batch_constant<ubatch32, Tiler<Arch>>()));
  }

private:
  // A version that produces uint8_ts
  template <class Arch>
  static inline xsimd::batch<uint8_t, Arch>
  TileU(xsimd::batch<float, Arch> quant_mult, const float *input0,
        const float *input1, const float *input2, const float *input3) {
    using batch8 = xsimd::batch<int8_t, Arch>;
    using batch16 = xsimd::batch<int16_t, Arch>;
    using batch32 = xsimd::batch<int32_t, Arch>;
    using ubatch32 = xsimd::batch<uint32_t, Arch>;

    const batch8 neg127 = -127;
    const batch8 pos127 = +127;
    // Grab 4 registers at a time in 32-bit format.
    batch32 g0 = QuantizerGrab(input0, quant_mult);
    batch32 g1 = QuantizerGrab(input1, quant_mult);
    batch32 g2 = QuantizerGrab(input2, quant_mult);
    batch32 g3 = QuantizerGrab(input3, quant_mult);
    // Pack 32-bit to 16-bit.
    batch16 packed0 = deinterleave(g0, g1);
    batch16 packed1 = deinterleave(g2, g3);
    // Pack 16-bit to 8-bit.
    batch8 packed = deinterleave(packed0, packed1);
    // Ban -128.
    packed = xsimd::max(packed, neg127); // Could be removed  if we use +128
    packed = packed + pos127;
    if (batch32::size == 4)
      return xsimd::bitwise_cast<uint8_t>(packed);
    // Currently in 0 1 2 3 8 9 10 11 16 17 18 19 24 25 26 27 4 5 6 7 12 13 14
    // 15 20 21 22 23 28 29 30 31 Or as 32-bit integers 0 2 4 6 1 3 5 7
    // Technically this could be removed so long as the rows are bigger than 16
    // and the values are only used for GEMM.
    return xsimd::bitwise_cast<uint8_t>(
        xsimd::swizzle(xsimd::bitwise_cast<int32_t>(packed),
                       xsimd::make_batch_constant<ubatch32, Tiler<Arch>>()));
  }
};

template <class Arch>
inline void Transpose16InLane(
    xsimd::batch<int8_t, Arch> &r0, xsimd::batch<int8_t, Arch> &r1,
    xsimd::batch<int8_t, Arch> &r2, xsimd::batch<int8_t, Arch> &r3,
    xsimd::batch<int8_t, Arch> &r4, xsimd::batch<int8_t, Arch> &r5,
    xsimd::batch<int8_t, Arch> &r6, xsimd::batch<int8_t, Arch> &r7) {
  /* r0: columns 0 1 2 3 4 5 6 7 from row 0
     r1: columns 0 1 2 3 4 5 6 7 from row 1*/
  auto r0_16 = xsimd::bitwise_cast<int16_t>(r0);
  auto r1_16 = xsimd::bitwise_cast<int16_t>(r1);
  auto r2_16 = xsimd::bitwise_cast<int16_t>(r2);
  auto r3_16 = xsimd::bitwise_cast<int16_t>(r3);
  auto r4_16 = xsimd::bitwise_cast<int16_t>(r4);
  auto r5_16 = xsimd::bitwise_cast<int16_t>(r5);
  auto r6_16 = xsimd::bitwise_cast<int16_t>(r6);
  auto r7_16 = xsimd::bitwise_cast<int16_t>(r7);

  std::tie(r0_16, r1_16) = interleave(r0_16, r1_16);
  std::tie(r2_16, r3_16) = interleave(r2_16, r3_16);
  std::tie(r4_16, r5_16) = interleave(r4_16, r5_16);
  std::tie(r6_16, r7_16) = interleave(r6_16, r7_16);
  /* r0: columns 0 0 1 1 2 2 3 3 from rows 0 and 1
     r1: columns 4 4 5 5 6 6 7 7 from rows 0 and 1
     r2: columns 0 0 1 1 2 2 3 3 from rows 2 and 3
     r3: columns 4 4 5 5 6 6 7 7 from rows 2 and 3
     r4: columns 0 0 1 1 2 2 3 3 from rows 4 and 5
     r5: columns 4 4 5 5 6 6 7 7 from rows 4 and 5
     r6: columns 0 0 1 1 2 2 3 3 from rows 6 and 7
     r7: columns 4 4 5 5 6 6 7 7 from rows 6 and 7*/
  auto r0_32 = xsimd::bitwise_cast<int32_t>(r0_16);
  auto r2_32 = xsimd::bitwise_cast<int32_t>(r2_16);
  auto r1_32 = xsimd::bitwise_cast<int32_t>(r1_16);
  auto r3_32 = xsimd::bitwise_cast<int32_t>(r3_16);
  auto r4_32 = xsimd::bitwise_cast<int32_t>(r4_16);
  auto r6_32 = xsimd::bitwise_cast<int32_t>(r6_16);
  auto r5_32 = xsimd::bitwise_cast<int32_t>(r5_16);
  auto r7_32 = xsimd::bitwise_cast<int32_t>(r7_16);

  std::tie(r0_32, r2_32) = interleave(r0_32, r2_32);
  std::tie(r1_32, r3_32) = interleave(r1_32, r3_32);
  std::tie(r4_32, r6_32) = interleave(r4_32, r6_32);
  std::tie(r5_32, r7_32) = interleave(r5_32, r7_32);
  /* r0: columns 0 0 0 0 1 1 1 1 from rows 0, 1, 2, and 3
     r1: columns 4 4 4 4 5 5 5 5 from rows 0, 1, 2, and 3
     r2: columns 2 2 2 2 3 3 3 3 from rows 0, 1, 2, and 3
     r3: columns 6 6 6 6 7 7 7 7 from rows 0, 1, 2, and 3
     r4: columns 0 0 0 0 1 1 1 1 from rows 4, 5, 6, and 7
     r5: columns 4 4 4 4 5 5 5 5 from rows 4, 5, 6, and 7
     r6: columns 2 2 2 2 3 3 3 3 from rows 4, 5, 6, and 7
     r7: columns 6 6 6 6 7 7 7 7 from rows 4, 5, 6, and 7*/

  auto r0_64 = xsimd::bitwise_cast<int64_t>(r0_32);
  auto r2_64 = xsimd::bitwise_cast<int64_t>(r2_32);
  auto r1_64 = xsimd::bitwise_cast<int64_t>(r1_32);
  auto r3_64 = xsimd::bitwise_cast<int64_t>(r3_32);
  auto r4_64 = xsimd::bitwise_cast<int64_t>(r4_32);
  auto r6_64 = xsimd::bitwise_cast<int64_t>(r6_32);
  auto r5_64 = xsimd::bitwise_cast<int64_t>(r5_32);
  auto r7_64 = xsimd::bitwise_cast<int64_t>(r7_32);

  std::tie(r0_64, r4_64) = interleave(r0_64, r4_64);
  std::tie(r1_64, r5_64) = interleave(r1_64, r5_64);
  std::tie(r2_64, r6_64) = interleave(r2_64, r6_64);
  std::tie(r3_64, r7_64) = interleave(r3_64, r7_64);

  r0 = xsimd::bitwise_cast<int8_t>(r0_64);
  r1 = xsimd::bitwise_cast<int8_t>(r1_64);
  r2 = xsimd::bitwise_cast<int8_t>(r2_64);
  r3 = xsimd::bitwise_cast<int8_t>(r3_64);
  r4 = xsimd::bitwise_cast<int8_t>(r4_64);
  r5 = xsimd::bitwise_cast<int8_t>(r5_64);
  r6 = xsimd::bitwise_cast<int8_t>(r6_64);
  r7 = xsimd::bitwise_cast<int8_t>(r7_64);
  /* r0: columns 0 0 0 0 0 0 0 0 from rows 0 through 7
     r1: columns 4 4 4 4 4 4 4 4 from rows 0 through 7
     r2: columns 2 2 2 2 2 2 2 2 from rows 0 through 7
     r3: columns 6 6 6 6 6 6 6 6 from rows 0 through 7
     r4: columns 1 1 1 1 1 1 1 1 from rows 0 through 7
     r5: columns 5 5 5 5 5 5 5 5 from rows 0 through 7*/
  /* Empirically gcc is able to remove these movs and just rename the outputs of
   * Interleave64. */
  std::swap(r1, r4);
  std::swap(r3, r6);
}

template <class Arch, typename IntegerTy>
void SelectColumnsOfB(const xsimd::batch<int8_t, Arch> *input,
                      xsimd::batch<int8_t, Arch> *output,
                      size_t rows_bytes /* number of bytes in a row */,
                      const IntegerTy *cols_begin, const IntegerTy *cols_end) {
  using batch8 = xsimd::batch<int8_t, Arch>;
  /* Do columns for multiples of 8.*/
  size_t register_rows = rows_bytes / batch8::size;
  const batch8 *starts[8];
  for (; cols_begin != cols_end; cols_begin += 8) {
    for (size_t k = 0; k < 8; ++k) {
      starts[k] =
          input + (cols_begin[k] & 7) + (cols_begin[k] & ~7) * register_rows;
    }
    for (size_t r = 0; r < register_rows; ++r) {
      for (size_t k = 0; k < 8; ++k) {
        *(output++) = *starts[k];
        starts[k] += 8;
      }
    }
  }
}

} // namespace

namespace callbacks {
template <class Arch>
xsimd::batch<float, Arch> Unquantize::operator()(xsimd::batch<int32_t, Arch> total, size_t, size_t,
                            size_t) {
  return xsimd::batch_cast<float>(total) * unquant_mult;
}

template <class Arch>
std::tuple<xsimd::batch<float, Arch>, xsimd::batch<float, Arch>> Unquantize::operator()(
    std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>> total,
    size_t, size_t, size_t) {
  return std::make_tuple(
      xsimd::batch_cast<float>(std::get<0>(total)) * unquant_mult,
      xsimd::batch_cast<float>(std::get<1>(total)) * unquant_mult);
}

template <class Arch>
xsimd::batch<float, Arch> AddBias::operator()(xsimd::batch<float, Arch> total, size_t,
                         size_t col_idx, size_t) {
  return total + xsimd::batch<float, Arch>::load_aligned(bias_addr + col_idx);
}

template <class Arch>
std::tuple<xsimd::batch<float, Arch>, xsimd::batch<float, Arch>>
AddBias::operator()(
    std::tuple<xsimd::batch<float, Arch>, xsimd::batch<float, Arch>> total,
    size_t, size_t col_idx, size_t) {
  return std::make_tuple(
      std::get<0>(total) + xsimd::batch<float, Arch>::load_aligned(bias_addr + col_idx + 0),
      std::get<1>(total) +
          xsimd::batch<float, Arch>::load_aligned(bias_addr + col_idx +
                              xsimd::batch<float, Arch>::size));
}

template <class Arch>
void Write::operator()(xsimd::batch<float, Arch> result, size_t row_idx,
                       size_t col_idx, size_t col_size) {
  result.store_aligned(output_addr + row_idx * col_size + col_idx);
}

template <class Arch>
void Write::operator()(xsimd::batch<int32_t, Arch> result, size_t row_idx,
                       size_t col_idx, size_t col_size) {
  xsimd::bitwise_cast<float>(result).store_aligned(
      output_addr + row_idx * col_size + col_idx);
}

template <class Arch>
void Write::operator()(
    std::tuple<xsimd::batch<float, Arch>, xsimd::batch<float, Arch>> result,
    size_t row_idx, size_t col_idx, size_t col_size) {
  std::get<0>(result).store_aligned(output_addr + row_idx * col_size + col_idx +
                                    0);
  std::get<1>(result).store_aligned(output_addr + row_idx * col_size + col_idx +
                                    xsimd::batch<float, Arch>::size);
}

template <class Arch>
void Write::operator()(
    std::tuple<xsimd::batch<int32_t, Arch>, xsimd::batch<int32_t, Arch>> result,
    size_t row_idx, size_t col_idx, size_t col_size) {
  xsimd::bitwise_cast<float>(std::get<0>(result))
      .store_aligned(output_addr + row_idx * col_size + col_idx + 0);
  xsimd::bitwise_cast<float>(std::get<1>(result))
      .store_aligned(output_addr + row_idx * col_size + col_idx +
                     xsimd::batch<int32_t, Arch>::size);
}

template <class T>
void UnquantizeAndWrite::operator()(T const &total, size_t row_idx,
                                    size_t col_idx, size_t col_size) {
  auto unquantized = unquantize(total, row_idx, col_idx, col_size);
  write(unquantized, row_idx, col_idx, col_size);
}

template <class T>
void UnquantizeAndAddBiasAndWrite::operator()(T const &total, size_t row_idx,
                                              size_t col_idx, size_t col_size) {
  auto unquantized = unquantize(total, row_idx, col_idx, col_size);
  auto bias_added = add_bias(unquantized, row_idx, col_idx, col_size);
  write(bias_added, row_idx, col_idx, col_size);
}
} // namespace callbacks

template <class Arch>
void Engine<Arch>::QuantizeU(const float *input, uint8_t *output,
                             float quant_mult, size_t size) {
  using batch8 = xsimd::batch<int8_t, Arch>;

  xsimd::batch<float, Arch> q(quant_mult);
  const float *end = input + size;
  for (; input != end; input += batch8::size, output += batch8::size) {
    auto tile = QuantizeTile8::ConsecutiveU(q, input);
    tile.store_aligned(output);
  }
}

template <class Arch>
void Engine<Arch>::Quantize(const float *const input, int8_t *const output,
                            float quant_mult, size_t size) {
  using batch8 = xsimd::batch<int8_t, Arch>;

  const std::size_t kBatch = batch8::size;
  const std::size_t fast_end = size & ~(kBatch - 1);

  xsimd::batch<float, Arch> q(quant_mult);
  for (std::size_t i = 0; i < fast_end; i += kBatch) {
    auto tile = QuantizeTile8::Consecutive(q, input + i);
    tile.store_aligned(output + i);
  }

  std::size_t overhang = size & (kBatch - 1);
  if (!overhang)
    return;
  /* Each does size(xsimd::batch<int8_t, Arch>) / 32 == kBatch / 4 floats at a
   * time. If we're allowed to read one of them, then we can read the whole
   * register.
   */
  const float *inputs[4];
  std::size_t i;
  for (i = 0; i < (overhang + (kBatch / 4) - 1) / (kBatch / 4); ++i) {
    inputs[i] = &input[fast_end + i * (kBatch / 4)];
  }
  /* These will be clipped off. */
  for (; i < 4; ++i) {
    inputs[i] = &input[fast_end];
  }
  auto result =
      QuantizeTile8::Tile(q, inputs[0], inputs[1], inputs[2], inputs[3]);
  std::memcpy(output + (size & ~(kBatch - 1)), &result, overhang);
}

template <class Arch>
template <typename IntegerTy>
void Engine<Arch>::SelectColumnsB(const int8_t *input, int8_t *output,
                                  size_t rows, const IntegerTy *cols_begin,
                                  const IntegerTy *cols_end) {
  using batch8 = xsimd::batch<int8_t, Arch>;
  SelectColumnsOfB(reinterpret_cast<const batch8 *>(input),
                   reinterpret_cast<batch8 *>(output), rows, cols_begin,
                   cols_end);
}

template <class Arch>
void Engine<Arch>::PrepareBTransposed(const float *input, int8_t *output,
                                      float quant_mult, size_t cols,
                                      size_t rows) {
  using batch8 = xsimd::batch<int8_t, Arch>;
  const size_t RegisterElemsInt = batch8::size;
  const size_t kColStride = 8;

  xsimd::batch<float, Arch> q(quant_mult);
  auto *output_it = reinterpret_cast<batch8 *>(output);
  size_t r = 0;
  size_t c = 0;
  while (r < rows) {
    for (size_t ri = 0; ri < 8; ++ri)
      *output_it++ = QuantizeTile8::ConsecutiveWithWrapping(
          q, input + (r + ri) * cols + c, cols - c, cols, 8);
    c += RegisterElemsInt;
    while (c >= cols) {
      r += kColStride;
      c -= cols;
    }
  }
}

template <class Arch>
void Engine<Arch>::PrepareBQuantizedTransposed(const int8_t *input,
                                               int8_t *output, size_t cols,
                                               size_t rows) {
  using batch8 = xsimd::batch<int8_t, Arch>;
  const size_t RegisterElems = batch8::size;
  const size_t kColStride = 8;

  auto *output_it = reinterpret_cast<batch8 *>(output);
  for (size_t r = 0; r < rows; r += kColStride)
    for (size_t c = 0; c < cols; c += RegisterElems)
      for (size_t ri = 0; ri < 8; ++ri)
        *output_it++ =
            *reinterpret_cast<const batch8 *>(input + (r + ri) * cols + c);
}

template <class Arch>
void Engine<Arch>::PrepareB(const float *input, int8_t *output_shadow,
                            float quant_mult, size_t rows, size_t cols) {
  using batch8 = xsimd::batch<int8_t, Arch>;

  xsimd::batch<float, Arch> q(quant_mult);
  /* Currently all multipliers have a stride of 8 columns.*/
  const size_t kColStride = 8;
  auto *output = reinterpret_cast<batch8 *>(output_shadow);
  for (size_t c = 0; c < cols; c += kColStride) {
    for (size_t r = 0; r < rows; r += sizeof(*output), output += 8) {
      output[0] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 0) + c, cols);
      output[1] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 1) + c, cols);
      output[2] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 4) + c, cols);
      output[3] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 5) + c, cols);
      output[4] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 8) + c, cols);
      output[5] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 9) + c, cols);
      output[6] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 12) + c, cols);
      output[7] =
          QuantizeTile8::ForReshape(q, input + cols * (r + 13) + c, cols);
      std::tie(output[0], output[1]) =
          interleave(xsimd::bitwise_cast<int8_t>(output[0]),
                     xsimd::bitwise_cast<int8_t>(output[1]));
      std::tie(output[2], output[3]) =
          interleave(xsimd::bitwise_cast<int8_t>(output[2]),
                     xsimd::bitwise_cast<int8_t>(output[3]));
      std::tie(output[4], output[5]) =
          interleave(xsimd::bitwise_cast<int8_t>(output[4]),
                     xsimd::bitwise_cast<int8_t>(output[5]));
      std::tie(output[6], output[7]) =
          interleave(xsimd::bitwise_cast<int8_t>(output[6]),
                     xsimd::bitwise_cast<int8_t>(output[7]));
      Transpose16InLane(output[0], output[1], output[2], output[3], output[4],
                        output[5], output[6], output[7]);
    }
  }
}

template <class Arch>
void Engine<Arch>::PrepareA(const float *input, int8_t *output,
                            float quant_mult, size_t rows, size_t cols) {
  Quantize(input, output, quant_mult, rows * cols);
}

template <class Arch>
void Engine<Arch>::Shift::PrepareA(const float *input, uint8_t *output,
                                   float quant_mult, size_t rows, size_t cols) {
  QuantizeU(input, output, quant_mult, rows * cols);
}

template <class Arch>
template <class Callback>
void Engine<Arch>::Shift::Multiply(const uint8_t *A, const int8_t *B,
                                   size_t A_rows, size_t width, size_t B_cols,
                                   Callback callback) {

  using batch8 = xsimd::batch<int8_t, Arch>;
  using ubatch8 = xsimd::batch<uint8_t, Arch>;
  using batch32 = xsimd::batch<int32_t, Arch>;

  const size_t simd_width = width / batch8::size;
  for (size_t B0_colidx = 0; B0_colidx < B_cols; B0_colidx += 8) {
    const auto *B0_col =
        reinterpret_cast<const batch8 *>(B) + simd_width * B0_colidx;
    /* Process one row of A at a time.  Doesn't seem to be faster to do multiple
     * rows of A at once.*/
    for (size_t A_rowidx = 0; A_rowidx < A_rows; ++A_rowidx) {
      const auto *A_row =
          reinterpret_cast<const ubatch8 *>(A + A_rowidx * width);
      /* These will be packed 16-bit integers containing sums for each row of B
         multiplied by the row of A. Iterate over shared (inner) dimension.*/
      /* Upcast to 32-bit and horizontally add. Seems a bit faster if this is
       * declared here.*/
      size_t k = 0;
      ubatch8 a = *(A_row + k);
      batch32 isum0 = maddw(a, *(B0_col + k * 8));
      batch32 isum1 = maddw(a, *(B0_col + k * 8 + 1));
      batch32 isum2 = maddw(a, *(B0_col + k * 8 + 2));
      batch32 isum3 = maddw(a, *(B0_col + k * 8 + 3));
      batch32 isum4 = maddw(a, *(B0_col + k * 8 + 4));
      batch32 isum5 = maddw(a, *(B0_col + k * 8 + 5));
      batch32 isum6 = maddw(a, *(B0_col + k * 8 + 6));
      batch32 isum7 = maddw(a, *(B0_col + k * 8 + 7));
      for (k = 1; k < simd_width; ++k) {
        a = *(A_row + k);
        /* Multiply 8-bit, horizontally add to packed 16-bit integers.*/
        /* Upcast to 32-bit and horizontally add.*/
        batch32 imult0 = maddw(a, *(B0_col + k * 8));
        batch32 imult1 = maddw(a, *(B0_col + k * 8 + 1));
        batch32 imult2 = maddw(a, *(B0_col + k * 8 + 2));
        batch32 imult3 = maddw(a, *(B0_col + k * 8 + 3));
        batch32 imult4 = maddw(a, *(B0_col + k * 8 + 4));
        batch32 imult5 = maddw(a, *(B0_col + k * 8 + 5));
        batch32 imult6 = maddw(a, *(B0_col + k * 8 + 6));
        batch32 imult7 = maddw(a, *(B0_col + k * 8 + 7));
        /*Add in 32bit*/
        isum0 += imult0;
        isum1 += imult1;
        isum2 += imult2;
        isum3 += imult3;
        isum4 += imult4;
        isum5 += imult5;
        isum6 += imult6;
        isum7 += imult7;
      }
      /* Reduce sums within 128-bit lanes.*/
      auto pack0123 = Pack0123(isum0, isum1, isum2, isum3);
      auto pack4567 = Pack0123(isum4, isum5, isum6, isum7);
      /*The specific implementation may need to reduce further.*/
      auto total = PermuteSummer(pack0123, pack4567);
      callback(total, A_rowidx, B0_colidx, B_cols);
    }
  }
}

template <class Arch>
template <class Callback>
void Engine<Arch>::Shift::PrepareBias(const int8_t *B, size_t width,
                                      size_t B_cols, Callback C) {
  using batch8 = xsimd::batch<int8_t, Arch>;
  const size_t simd_width = width / batch8::size;
  xsimd::batch<uint8_t, Arch> a(1);
  for (size_t j = 0; j < B_cols; j += 8) {
    /*Process one row of A at a time.  Doesn't seem to be faster to do multiple
     * rows of A at once.*/
    const int8_t *B_j = B + j * width;

    /* Rather than initializing as zeros and adding, just initialize the
     * first.*/
    /* These will be packed 16-bit integers containing sums for each column of
     * B multiplied by the row of A.*/
    /* Upcast to 32-bit and horizontally add. Seems a bit faster if this is
     * declared here.*/
    auto isum0 = maddw(a, batch8::load_aligned(&B_j[0 * batch8::size]));
    auto isum1 = maddw(a, batch8::load_aligned(&B_j[1 * batch8::size]));
    auto isum2 = maddw(a, batch8::load_aligned(&B_j[2 * batch8::size]));
    auto isum3 = maddw(a, batch8::load_aligned(&B_j[3 * batch8::size]));
    auto isum4 = maddw(a, batch8::load_aligned(&B_j[4 * batch8::size]));
    auto isum5 = maddw(a, batch8::load_aligned(&B_j[5 * batch8::size]));
    auto isum6 = maddw(a, batch8::load_aligned(&B_j[6 * batch8::size]));
    auto isum7 = maddw(a, batch8::load_aligned(&B_j[7 * batch8::size]));

    B_j += 8 * batch8::size;

    for (size_t k = 1; k < simd_width; ++k, B_j += 8 * batch8::size) {
      isum0 += maddw(a, batch8::load_aligned(&B_j[0 * batch8::size]));
      isum1 += maddw(a, batch8::load_aligned(&B_j[1 * batch8::size]));
      isum2 += maddw(a, batch8::load_aligned(&B_j[2 * batch8::size]));
      isum3 += maddw(a, batch8::load_aligned(&B_j[3 * batch8::size]));
      isum4 += maddw(a, batch8::load_aligned(&B_j[4 * batch8::size]));
      isum5 += maddw(a, batch8::load_aligned(&B_j[5 * batch8::size]));
      isum6 += maddw(a, batch8::load_aligned(&B_j[6 * batch8::size]));
      isum7 += maddw(a, batch8::load_aligned(&B_j[7 * batch8::size]));
    }

    auto pack0123 = Pack0123(isum0, isum1, isum2, isum3);
    auto pack4567 = Pack0123(isum4, isum5, isum6, isum7);

    auto total = PermuteSummer(pack0123, pack4567);
    C(total, 0, j, B_cols);
  }
}

} // namespace gemmology

#endif
