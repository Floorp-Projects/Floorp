// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// 512-bit AVX512 vectors and operations.
// External include guard in highway.h - see comment there.

// WARNING: most operations do not cross 128-bit block boundaries. In
// particular, "Broadcast", pack and zip behavior may be surprising.

// Must come before HWY_DIAGNOSTICS and HWY_COMPILER_CLANGCL
#include "hwy/base.h"

// Avoid uninitialized warnings in GCC's avx512fintrin.h - see
// https://github.com/google/highway/issues/710)
HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_GCC_ACTUAL
HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wuninitialized")
HWY_DIAGNOSTICS_OFF(disable : 4703 6001 26494, ignored "-Wmaybe-uninitialized")
#endif

#include <immintrin.h>  // AVX2+

#if HWY_COMPILER_CLANGCL
// Including <immintrin.h> should be enough, but Clang's headers helpfully skip
// including these headers when _MSC_VER is defined, like when using clang-cl.
// Include these directly here.
// clang-format off
#include <smmintrin.h>

#include <avxintrin.h>
#include <avx2intrin.h>
#include <f16cintrin.h>
#include <fmaintrin.h>

#include <avx512fintrin.h>
#include <avx512vlintrin.h>
#include <avx512bwintrin.h>
#include <avx512dqintrin.h>
#include <avx512vlbwintrin.h>
#include <avx512vldqintrin.h>
#include <avx512bitalgintrin.h>
#include <avx512vlbitalgintrin.h>
#include <avx512vpopcntdqintrin.h>
#include <avx512vpopcntdqvlintrin.h>
// clang-format on
#endif  // HWY_COMPILER_CLANGCL

#include <stddef.h>
#include <stdint.h>

#if HWY_IS_MSAN
#include <sanitizer/msan_interface.h>
#endif

// For half-width vectors. Already includes base.h and shared-inl.h.
#include "hwy/ops/x86_256-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace detail {

template <typename T>
struct Raw512 {
  using type = __m512i;
};
template <>
struct Raw512<float> {
  using type = __m512;
};
template <>
struct Raw512<double> {
  using type = __m512d;
};

// Template arg: sizeof(lane type)
template <size_t size>
struct RawMask512 {};
template <>
struct RawMask512<1> {
  using type = __mmask64;
};
template <>
struct RawMask512<2> {
  using type = __mmask32;
};
template <>
struct RawMask512<4> {
  using type = __mmask16;
};
template <>
struct RawMask512<8> {
  using type = __mmask8;
};

}  // namespace detail

template <typename T>
class Vec512 {
  using Raw = typename detail::Raw512<T>::type;

 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec512& operator*=(const Vec512 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec512& operator/=(const Vec512 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec512& operator+=(const Vec512 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec512& operator-=(const Vec512 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec512& operator&=(const Vec512 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec512& operator|=(const Vec512 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec512& operator^=(const Vec512 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

// Mask register: one bit per lane.
template <typename T>
struct Mask512 {
  typename detail::RawMask512<sizeof(T)>::type raw;
};

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m512i BitCastToInteger(__m512i v) { return v; }
HWY_INLINE __m512i BitCastToInteger(__m512 v) { return _mm512_castps_si512(v); }
HWY_INLINE __m512i BitCastToInteger(__m512d v) {
  return _mm512_castpd_si512(v);
}

template <typename T>
HWY_INLINE Vec512<uint8_t> BitCastToByte(Vec512<T> v) {
  return Vec512<uint8_t>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger512 {
  HWY_INLINE __m512i operator()(__m512i v) { return v; }
};
template <>
struct BitCastFromInteger512<float> {
  HWY_INLINE __m512 operator()(__m512i v) { return _mm512_castsi512_ps(v); }
};
template <>
struct BitCastFromInteger512<double> {
  HWY_INLINE __m512d operator()(__m512i v) { return _mm512_castsi512_pd(v); }
};

template <typename T>
HWY_INLINE Vec512<T> BitCastFromByte(Full512<T> /* tag */, Vec512<uint8_t> v) {
  return Vec512<T>{BitCastFromInteger512<T>()(v.raw)};
}

}  // namespace detail

template <typename T, typename FromT>
HWY_API Vec512<T> BitCast(Full512<T> d, Vec512<FromT> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

// Returns an all-zero vector.
template <typename T>
HWY_API Vec512<T> Zero(Full512<T> /* tag */) {
  return Vec512<T>{_mm512_setzero_si512()};
}
HWY_API Vec512<float> Zero(Full512<float> /* tag */) {
  return Vec512<float>{_mm512_setzero_ps()};
}
HWY_API Vec512<double> Zero(Full512<double> /* tag */) {
  return Vec512<double>{_mm512_setzero_pd()};
}

// Returns a vector with all lanes set to "t".
HWY_API Vec512<uint8_t> Set(Full512<uint8_t> /* tag */, const uint8_t t) {
  return Vec512<uint8_t>{_mm512_set1_epi8(static_cast<char>(t))};  // NOLINT
}
HWY_API Vec512<uint16_t> Set(Full512<uint16_t> /* tag */, const uint16_t t) {
  return Vec512<uint16_t>{_mm512_set1_epi16(static_cast<short>(t))};  // NOLINT
}
HWY_API Vec512<uint32_t> Set(Full512<uint32_t> /* tag */, const uint32_t t) {
  return Vec512<uint32_t>{_mm512_set1_epi32(static_cast<int>(t))};
}
HWY_API Vec512<uint64_t> Set(Full512<uint64_t> /* tag */, const uint64_t t) {
  return Vec512<uint64_t>{
      _mm512_set1_epi64(static_cast<long long>(t))};  // NOLINT
}
HWY_API Vec512<int8_t> Set(Full512<int8_t> /* tag */, const int8_t t) {
  return Vec512<int8_t>{_mm512_set1_epi8(static_cast<char>(t))};  // NOLINT
}
HWY_API Vec512<int16_t> Set(Full512<int16_t> /* tag */, const int16_t t) {
  return Vec512<int16_t>{_mm512_set1_epi16(static_cast<short>(t))};  // NOLINT
}
HWY_API Vec512<int32_t> Set(Full512<int32_t> /* tag */, const int32_t t) {
  return Vec512<int32_t>{_mm512_set1_epi32(t)};
}
HWY_API Vec512<int64_t> Set(Full512<int64_t> /* tag */, const int64_t t) {
  return Vec512<int64_t>{
      _mm512_set1_epi64(static_cast<long long>(t))};  // NOLINT
}
HWY_API Vec512<float> Set(Full512<float> /* tag */, const float t) {
  return Vec512<float>{_mm512_set1_ps(t)};
}
HWY_API Vec512<double> Set(Full512<double> /* tag */, const double t) {
  return Vec512<double>{_mm512_set1_pd(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T>
HWY_API Vec512<T> Undefined(Full512<T> /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return Vec512<T>{_mm512_undefined_epi32()};
}
HWY_API Vec512<float> Undefined(Full512<float> /* tag */) {
  return Vec512<float>{_mm512_undefined_ps()};
}
HWY_API Vec512<double> Undefined(Full512<double> /* tag */) {
  return Vec512<double>{_mm512_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ================================================== LOGICAL

// ------------------------------ Not

template <typename T>
HWY_API Vec512<T> Not(const Vec512<T> v) {
  using TU = MakeUnsigned<T>;
  const __m512i vu = BitCast(Full512<TU>(), v).raw;
  return BitCast(Full512<T>(),
                 Vec512<TU>{_mm512_ternarylogic_epi32(vu, vu, vu, 0x55)});
}

// ------------------------------ And

template <typename T>
HWY_API Vec512<T> And(const Vec512<T> a, const Vec512<T> b) {
  return Vec512<T>{_mm512_and_si512(a.raw, b.raw)};
}

HWY_API Vec512<float> And(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_and_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> And(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T>
HWY_API Vec512<T> AndNot(const Vec512<T> not_mask, const Vec512<T> mask) {
  return Vec512<T>{_mm512_andnot_si512(not_mask.raw, mask.raw)};
}
HWY_API Vec512<float> AndNot(const Vec512<float> not_mask,
                             const Vec512<float> mask) {
  return Vec512<float>{_mm512_andnot_ps(not_mask.raw, mask.raw)};
}
HWY_API Vec512<double> AndNot(const Vec512<double> not_mask,
                              const Vec512<double> mask) {
  return Vec512<double>{_mm512_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T>
HWY_API Vec512<T> Or(const Vec512<T> a, const Vec512<T> b) {
  return Vec512<T>{_mm512_or_si512(a.raw, b.raw)};
}

HWY_API Vec512<float> Or(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_or_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Or(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec512<T> Xor(const Vec512<T> a, const Vec512<T> b) {
  return Vec512<T>{_mm512_xor_si512(a.raw, b.raw)};
}

HWY_API Vec512<float> Xor(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_xor_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Xor(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Or3

template <typename T>
HWY_API Vec512<T> Or3(Vec512<T> o1, Vec512<T> o2, Vec512<T> o3) {
  const Full512<T> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m512i ret = _mm512_ternarylogic_epi64(
      BitCast(du, o1).raw, BitCast(du, o2).raw, BitCast(du, o3).raw, 0xFE);
  return BitCast(d, VU{ret});
}

// ------------------------------ OrAnd

template <typename T>
HWY_API Vec512<T> OrAnd(Vec512<T> o, Vec512<T> a1, Vec512<T> a2) {
  const Full512<T> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m512i ret = _mm512_ternarylogic_epi64(
      BitCast(du, o).raw, BitCast(du, a1).raw, BitCast(du, a2).raw, 0xF8);
  return BitCast(d, VU{ret});
}

// ------------------------------ IfVecThenElse

template <typename T>
HWY_API Vec512<T> IfVecThenElse(Vec512<T> mask, Vec512<T> yes, Vec512<T> no) {
  const Full512<T> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{_mm512_ternarylogic_epi64(BitCast(du, mask).raw,
                                                 BitCast(du, yes).raw,
                                                 BitCast(du, no).raw, 0xCA)});
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T>
HWY_API Vec512<T> operator&(const Vec512<T> a, const Vec512<T> b) {
  return And(a, b);
}

template <typename T>
HWY_API Vec512<T> operator|(const Vec512<T> a, const Vec512<T> b) {
  return Or(a, b);
}

template <typename T>
HWY_API Vec512<T> operator^(const Vec512<T> a, const Vec512<T> b) {
  return Xor(a, b);
}

// ------------------------------ PopulationCount

// 8/16 require BITALG, 32/64 require VPOPCNTDQ.
#if HWY_TARGET == HWY_AVX3_DL

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

namespace detail {

template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi8(v.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi16(v.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi32(v.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi64(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> PopulationCount(Vec512<T> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

#endif  // HWY_TARGET == HWY_AVX3_DL

// ================================================== SIGN

// ------------------------------ CopySign

template <typename T>
HWY_API Vec512<T> CopySign(const Vec512<T> magn, const Vec512<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const Full512<T> d;
  const auto msb = SignBit(d);

  const Rebind<MakeUnsigned<T>, decltype(d)> du;
  // Truth table for msb, magn, sign | bitwise msb ? sign : mag
  //                  0    0     0   |  0
  //                  0    0     1   |  0
  //                  0    1     0   |  1
  //                  0    1     1   |  1
  //                  1    0     0   |  0
  //                  1    0     1   |  1
  //                  1    1     0   |  0
  //                  1    1     1   |  1
  // The lane size does not matter because we are not using predication.
  const __m512i out = _mm512_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, decltype(Zero(du)){out});
}

template <typename T>
HWY_API Vec512<T> CopySignToAbs(const Vec512<T> abs, const Vec512<T> sign) {
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
}

// ================================================== MASK

// ------------------------------ FirstN

// Possibilities for constructing a bitmask of N ones:
// - kshift* only consider the lowest byte of the shift count, so they would
//   not correctly handle large n.
// - Scalar shifts >= 64 are UB.
// - BZHI has the desired semantics; we assume AVX-512 implies BMI2. However,
//   we need 64-bit masks for sizeof(T) == 1, so special-case 32-bit builds.

#if HWY_ARCH_X86_32
namespace detail {

// 32 bit mask is sufficient for lane size >= 2.
template <typename T, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_INLINE Mask512<T> FirstN(size_t n) {
  Mask512<T> m;
  const uint32_t all = ~uint32_t{0};
  // BZHI only looks at the lower 8 bits of n!
  m.raw = static_cast<decltype(m.raw)>((n > 255) ? all : _bzhi_u32(all, n));
  return m;
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask512<T> FirstN(size_t n) {
  const uint64_t bits = n < 64 ? ((1ULL << n) - 1) : ~uint64_t{0};
  return Mask512<T>{static_cast<__mmask64>(bits)};
}

}  // namespace detail
#endif  // HWY_ARCH_X86_32

template <typename T>
HWY_API Mask512<T> FirstN(const Full512<T> /*tag*/, size_t n) {
#if HWY_ARCH_X86_64
  Mask512<T> m;
  const uint64_t all = ~uint64_t{0};
  // BZHI only looks at the lower 8 bits of n!
  m.raw = static_cast<decltype(m.raw)>((n > 255) ? all : _bzhi_u64(all, n));
  return m;
#else
  return detail::FirstN<T>(n);
#endif  // HWY_ARCH_X86_64
}

// ------------------------------ IfThenElse

// Returns mask ? b : a.

namespace detail {

// Templates for signed/unsigned integer of a particular size.
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<1> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi8(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<2> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi16(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<4> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi32(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<8> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi64(no.raw, mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> IfThenElse(const Mask512<T> mask, const Vec512<T> yes,
                             const Vec512<T> no) {
  return detail::IfThenElse(hwy::SizeTag<sizeof(T)>(), mask, yes, no);
}
HWY_API Vec512<float> IfThenElse(const Mask512<float> mask,
                                 const Vec512<float> yes,
                                 const Vec512<float> no) {
  return Vec512<float>{_mm512_mask_mov_ps(no.raw, mask.raw, yes.raw)};
}
HWY_API Vec512<double> IfThenElse(const Mask512<double> mask,
                                  const Vec512<double> yes,
                                  const Vec512<double> no) {
  return Vec512<double>{_mm512_mask_mov_pd(no.raw, mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<1> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi8(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<2> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi16(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<4> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi32(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<8> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi64(mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> IfThenElseZero(const Mask512<T> mask, const Vec512<T> yes) {
  return detail::IfThenElseZero(hwy::SizeTag<sizeof(T)>(), mask, yes);
}
HWY_API Vec512<float> IfThenElseZero(const Mask512<float> mask,
                                     const Vec512<float> yes) {
  return Vec512<float>{_mm512_maskz_mov_ps(mask.raw, yes.raw)};
}
HWY_API Vec512<double> IfThenElseZero(const Mask512<double> mask,
                                      const Vec512<double> yes) {
  return Vec512<double>{_mm512_maskz_mov_pd(mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<1> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  // xor_epi8/16 are missing, but we have sub, which is just as fast for u8/16.
  return Vec512<T>{_mm512_mask_sub_epi8(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<2> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_sub_epi16(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<4> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_xor_epi32(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<8> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_xor_epi64(no.raw, mask.raw, no.raw, no.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> IfThenZeroElse(const Mask512<T> mask, const Vec512<T> no) {
  return detail::IfThenZeroElse(hwy::SizeTag<sizeof(T)>(), mask, no);
}
HWY_API Vec512<float> IfThenZeroElse(const Mask512<float> mask,
                                     const Vec512<float> no) {
  return Vec512<float>{_mm512_mask_xor_ps(no.raw, mask.raw, no.raw, no.raw)};
}
HWY_API Vec512<double> IfThenZeroElse(const Mask512<double> mask,
                                      const Vec512<double> no) {
  return Vec512<double>{_mm512_mask_xor_pd(no.raw, mask.raw, no.raw, no.raw)};
}

template <typename T>
HWY_API Vec512<T> IfNegativeThenElse(Vec512<T> v, Vec512<T> yes, Vec512<T> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  // AVX3 MaskFromVec only looks at the MSB
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec512<T> ZeroIfNegative(const Vec512<T> v) {
  // AVX3 MaskFromVec only looks at the MSB
  return IfThenZeroElse(MaskFromVec(v), v);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
HWY_API Vec512<uint8_t> operator+(const Vec512<uint8_t> a,
                                  const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_add_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> operator+(const Vec512<uint16_t> a,
                                   const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_add_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> operator+(const Vec512<uint32_t> a,
                                   const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_add_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> operator+(const Vec512<uint64_t> a,
                                   const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_add_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> operator+(const Vec512<int8_t> a,
                                 const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_add_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> operator+(const Vec512<int16_t> a,
                                  const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_add_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> operator+(const Vec512<int32_t> a,
                                  const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_add_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> operator+(const Vec512<int64_t> a,
                                  const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_add_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> operator+(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_add_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator+(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
HWY_API Vec512<uint8_t> operator-(const Vec512<uint8_t> a,
                                  const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> operator-(const Vec512<uint16_t> a,
                                   const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> operator-(const Vec512<uint32_t> a,
                                   const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> operator-(const Vec512<uint64_t> a,
                                   const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_sub_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> operator-(const Vec512<int8_t> a,
                                 const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> operator-(const Vec512<int16_t> a,
                                  const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> operator-(const Vec512<int32_t> a,
                                  const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> operator-(const Vec512<int64_t> a,
                                  const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_sub_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> operator-(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_sub_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator-(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_sub_pd(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
HWY_API Vec512<uint64_t> SumsOf8(const Vec512<uint8_t> v) {
  return Vec512<uint64_t>{_mm512_sad_epu8(v.raw, _mm512_setzero_si512())};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec512<uint8_t> SaturatedAdd(const Vec512<uint8_t> a,
                                     const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_adds_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> SaturatedAdd(const Vec512<uint16_t> a,
                                      const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_adds_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> SaturatedAdd(const Vec512<int8_t> a,
                                    const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_adds_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> SaturatedAdd(const Vec512<int16_t> a,
                                     const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_adds_epi16(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec512<uint8_t> SaturatedSub(const Vec512<uint8_t> a,
                                     const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_subs_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> SaturatedSub(const Vec512<uint16_t> a,
                                      const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_subs_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> SaturatedSub(const Vec512<int8_t> a,
                                    const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_subs_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> SaturatedSub(const Vec512<int16_t> a,
                                     const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_subs_epi16(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
HWY_API Vec512<uint8_t> AverageRound(const Vec512<uint8_t> a,
                                     const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_avg_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> AverageRound(const Vec512<uint16_t> a,
                                      const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Abs (Sub)

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec512<int8_t> Abs(const Vec512<int8_t> v) {
#if HWY_COMPILER_MSVC
  // Workaround for incorrect codegen? (untested due to internal compiler error)
  const auto zero = Zero(Full512<int8_t>());
  return Vec512<int8_t>{_mm512_max_epi8(v.raw, (zero - v).raw)};
#else
  return Vec512<int8_t>{_mm512_abs_epi8(v.raw)};
#endif
}
HWY_API Vec512<int16_t> Abs(const Vec512<int16_t> v) {
  return Vec512<int16_t>{_mm512_abs_epi16(v.raw)};
}
HWY_API Vec512<int32_t> Abs(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_abs_epi32(v.raw)};
}
HWY_API Vec512<int64_t> Abs(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_abs_epi64(v.raw)};
}

// These aren't native instructions, they also involve AND with constant.
HWY_API Vec512<float> Abs(const Vec512<float> v) {
  return Vec512<float>{_mm512_abs_ps(v.raw)};
}
HWY_API Vec512<double> Abs(const Vec512<double> v) {
  return Vec512<double>{_mm512_abs_pd(v.raw)};
}
// ------------------------------ ShiftLeft

template <int kBits>
HWY_API Vec512<uint16_t> ShiftLeft(const Vec512<uint16_t> v) {
  return Vec512<uint16_t>{_mm512_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint32_t> ShiftLeft(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint64_t> ShiftLeft(const Vec512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_slli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int16_t> ShiftLeft(const Vec512<int16_t> v) {
  return Vec512<int16_t>{_mm512_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int32_t> ShiftLeft(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int64_t> ShiftLeft(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec512<T> ShiftLeft(const Vec512<T> v) {
  const Full512<T> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeft<kBits>(BitCast(d16, v)));
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits>
HWY_API Vec512<uint16_t> ShiftRight(const Vec512<uint16_t> v) {
  return Vec512<uint16_t>{_mm512_srli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint32_t> ShiftRight(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_srli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint64_t> ShiftRight(const Vec512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_srli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint8_t> ShiftRight(const Vec512<uint8_t> v) {
  const Full512<uint8_t> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec512<uint8_t> shifted{ShiftRight<kBits>(Vec512<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits>
HWY_API Vec512<int16_t> ShiftRight(const Vec512<int16_t> v) {
  return Vec512<int16_t>{_mm512_srai_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int32_t> ShiftRight(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_srai_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int64_t> ShiftRight(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_srai_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int8_t> ShiftRight(const Vec512<int8_t> v) {
  const Full512<int8_t> di;
  const Full512<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ RotateRight

template <int kBits>
HWY_API Vec512<uint32_t> RotateRight(const Vec512<uint32_t> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
  return Vec512<uint32_t>{_mm512_ror_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint64_t> RotateRight(const Vec512<uint64_t> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
  return Vec512<uint64_t>{_mm512_ror_epi64(v.raw, kBits)};
}

// ------------------------------ ShiftLeftSame

HWY_API Vec512<uint16_t> ShiftLeftSame(const Vec512<uint16_t> v,
                                       const int bits) {
  return Vec512<uint16_t>{_mm512_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint32_t> ShiftLeftSame(const Vec512<uint32_t> v,
                                       const int bits) {
  return Vec512<uint32_t>{_mm512_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint64_t> ShiftLeftSame(const Vec512<uint64_t> v,
                                       const int bits) {
  return Vec512<uint64_t>{_mm512_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int16_t> ShiftLeftSame(const Vec512<int16_t> v, const int bits) {
  return Vec512<int16_t>{_mm512_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int32_t> ShiftLeftSame(const Vec512<int32_t> v, const int bits) {
  return Vec512<int32_t>{_mm512_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int64_t> ShiftLeftSame(const Vec512<int64_t> v, const int bits) {
  return Vec512<int64_t>{_mm512_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec512<T> ShiftLeftSame(const Vec512<T> v, const int bits) {
  const Full512<T> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeftSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

// ------------------------------ ShiftRightSame

HWY_API Vec512<uint16_t> ShiftRightSame(const Vec512<uint16_t> v,
                                        const int bits) {
  return Vec512<uint16_t>{_mm512_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint32_t> ShiftRightSame(const Vec512<uint32_t> v,
                                        const int bits) {
  return Vec512<uint32_t>{_mm512_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint64_t> ShiftRightSame(const Vec512<uint64_t> v,
                                        const int bits) {
  return Vec512<uint64_t>{_mm512_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<uint8_t> ShiftRightSame(Vec512<uint8_t> v, const int bits) {
  const Full512<uint8_t> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftRightSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<uint8_t>(0xFF >> bits));
}

HWY_API Vec512<int16_t> ShiftRightSame(const Vec512<int16_t> v,
                                       const int bits) {
  return Vec512<int16_t>{_mm512_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int32_t> ShiftRightSame(const Vec512<int32_t> v,
                                       const int bits) {
  return Vec512<int32_t>{_mm512_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<int64_t> ShiftRightSame(const Vec512<int64_t> v,
                                       const int bits) {
  return Vec512<int64_t>{_mm512_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int8_t> ShiftRightSame(Vec512<int8_t> v, const int bits) {
  const Full512<int8_t> di;
  const Full512<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign =
      BitCast(di, Set(du, static_cast<uint8_t>(0x80 >> bits)));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Shl

HWY_API Vec512<uint16_t> operator<<(const Vec512<uint16_t> v,
                                    const Vec512<uint16_t> bits) {
  return Vec512<uint16_t>{_mm512_sllv_epi16(v.raw, bits.raw)};
}

HWY_API Vec512<uint32_t> operator<<(const Vec512<uint32_t> v,
                                    const Vec512<uint32_t> bits) {
  return Vec512<uint32_t>{_mm512_sllv_epi32(v.raw, bits.raw)};
}

HWY_API Vec512<uint64_t> operator<<(const Vec512<uint64_t> v,
                                    const Vec512<uint64_t> bits) {
  return Vec512<uint64_t>{_mm512_sllv_epi64(v.raw, bits.raw)};
}

// Signed left shift is the same as unsigned.
template <typename T, HWY_IF_SIGNED(T)>
HWY_API Vec512<T> operator<<(const Vec512<T> v, const Vec512<T> bits) {
  const Full512<T> di;
  const Full512<MakeUnsigned<T>> du;
  return BitCast(di, BitCast(du, v) << BitCast(du, bits));
}

// ------------------------------ Shr

HWY_API Vec512<uint16_t> operator>>(const Vec512<uint16_t> v,
                                    const Vec512<uint16_t> bits) {
  return Vec512<uint16_t>{_mm512_srlv_epi16(v.raw, bits.raw)};
}

HWY_API Vec512<uint32_t> operator>>(const Vec512<uint32_t> v,
                                    const Vec512<uint32_t> bits) {
  return Vec512<uint32_t>{_mm512_srlv_epi32(v.raw, bits.raw)};
}

HWY_API Vec512<uint64_t> operator>>(const Vec512<uint64_t> v,
                                    const Vec512<uint64_t> bits) {
  return Vec512<uint64_t>{_mm512_srlv_epi64(v.raw, bits.raw)};
}

HWY_API Vec512<int16_t> operator>>(const Vec512<int16_t> v,
                                   const Vec512<int16_t> bits) {
  return Vec512<int16_t>{_mm512_srav_epi16(v.raw, bits.raw)};
}

HWY_API Vec512<int32_t> operator>>(const Vec512<int32_t> v,
                                   const Vec512<int32_t> bits) {
  return Vec512<int32_t>{_mm512_srav_epi32(v.raw, bits.raw)};
}

HWY_API Vec512<int64_t> operator>>(const Vec512<int64_t> v,
                                   const Vec512<int64_t> bits) {
  return Vec512<int64_t>{_mm512_srav_epi64(v.raw, bits.raw)};
}

// ------------------------------ Minimum

// Unsigned
HWY_API Vec512<uint8_t> Min(const Vec512<uint8_t> a, const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_min_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> Min(const Vec512<uint16_t> a,
                             const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_min_epu16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> Min(const Vec512<uint32_t> a,
                             const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_min_epu32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> Min(const Vec512<uint64_t> a,
                             const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_min_epu64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> Min(const Vec512<int8_t> a, const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_min_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> Min(const Vec512<int16_t> a, const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_min_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> Min(const Vec512<int32_t> a, const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_min_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> Min(const Vec512<int64_t> a, const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_min_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> Min(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_min_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Min(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_min_pd(a.raw, b.raw)};
}

// ------------------------------ Maximum

// Unsigned
HWY_API Vec512<uint8_t> Max(const Vec512<uint8_t> a, const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_max_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> Max(const Vec512<uint16_t> a,
                             const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_max_epu16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> Max(const Vec512<uint32_t> a,
                             const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_max_epu32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> Max(const Vec512<uint64_t> a,
                             const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_max_epu64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> Max(const Vec512<int8_t> a, const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_max_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> Max(const Vec512<int16_t> a, const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_max_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> Max(const Vec512<int32_t> a, const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_max_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> Max(const Vec512<int64_t> a, const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_max_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> Max(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_max_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Max(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_max_pd(a.raw, b.raw)};
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec512<uint16_t> operator*(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> operator*(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_mullo_epi32(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int16_t> operator*(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> operator*(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_mullo_epi32(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec512<uint16_t> MulHigh(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_mulhi_epu16(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> MulHigh(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_mulhi_epi16(a.raw, b.raw)};
}

HWY_API Vec512<int16_t> MulFixedPoint15(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_mulhrs_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec512<int64_t> MulEven(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Vec512<int64_t>{_mm512_mul_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> MulEven(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Vec512<uint64_t>{_mm512_mul_epu32(a.raw, b.raw)};
}

// ------------------------------ Neg (Sub)

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec512<T> Neg(const Vec512<T> v) {
  return Xor(v, SignBit(Full512<T>()));
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec512<T> Neg(const Vec512<T> v) {
  return Zero(Full512<T>()) - v;
}

// ------------------------------ Floating-point mul / div

HWY_API Vec512<float> operator*(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_mul_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator*(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_mul_pd(a.raw, b.raw)};
}

HWY_API Vec512<float> operator/(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_div_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator/(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_div_pd(a.raw, b.raw)};
}

// Approximate reciprocal
HWY_API Vec512<float> ApproximateReciprocal(const Vec512<float> v) {
  return Vec512<float>{_mm512_rcp14_ps(v.raw)};
}

// Absolute value of difference.
HWY_API Vec512<float> AbsDiff(const Vec512<float> a, const Vec512<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
HWY_API Vec512<float> MulAdd(const Vec512<float> mul, const Vec512<float> x,
                             const Vec512<float> add) {
  return Vec512<float>{_mm512_fmadd_ps(mul.raw, x.raw, add.raw)};
}
HWY_API Vec512<double> MulAdd(const Vec512<double> mul, const Vec512<double> x,
                              const Vec512<double> add) {
  return Vec512<double>{_mm512_fmadd_pd(mul.raw, x.raw, add.raw)};
}

// Returns add - mul * x
HWY_API Vec512<float> NegMulAdd(const Vec512<float> mul, const Vec512<float> x,
                                const Vec512<float> add) {
  return Vec512<float>{_mm512_fnmadd_ps(mul.raw, x.raw, add.raw)};
}
HWY_API Vec512<double> NegMulAdd(const Vec512<double> mul,
                                 const Vec512<double> x,
                                 const Vec512<double> add) {
  return Vec512<double>{_mm512_fnmadd_pd(mul.raw, x.raw, add.raw)};
}

// Returns mul * x - sub
HWY_API Vec512<float> MulSub(const Vec512<float> mul, const Vec512<float> x,
                             const Vec512<float> sub) {
  return Vec512<float>{_mm512_fmsub_ps(mul.raw, x.raw, sub.raw)};
}
HWY_API Vec512<double> MulSub(const Vec512<double> mul, const Vec512<double> x,
                              const Vec512<double> sub) {
  return Vec512<double>{_mm512_fmsub_pd(mul.raw, x.raw, sub.raw)};
}

// Returns -mul * x - sub
HWY_API Vec512<float> NegMulSub(const Vec512<float> mul, const Vec512<float> x,
                                const Vec512<float> sub) {
  return Vec512<float>{_mm512_fnmsub_ps(mul.raw, x.raw, sub.raw)};
}
HWY_API Vec512<double> NegMulSub(const Vec512<double> mul,
                                 const Vec512<double> x,
                                 const Vec512<double> sub) {
  return Vec512<double>{_mm512_fnmsub_pd(mul.raw, x.raw, sub.raw)};
}

// ------------------------------ Floating-point square root

// Full precision square root
HWY_API Vec512<float> Sqrt(const Vec512<float> v) {
  return Vec512<float>{_mm512_sqrt_ps(v.raw)};
}
HWY_API Vec512<double> Sqrt(const Vec512<double> v) {
  return Vec512<double>{_mm512_sqrt_pd(v.raw)};
}

// Approximate reciprocal square root
HWY_API Vec512<float> ApproximateReciprocalSqrt(const Vec512<float> v) {
  return Vec512<float>{_mm512_rsqrt14_ps(v.raw)};
}

// ------------------------------ Floating-point rounding

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

// Toward nearest integer, tie to even
HWY_API Vec512<float> Round(const Vec512<float> v) {
  return Vec512<float>{_mm512_roundscale_ps(
      v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Round(const Vec512<double> v) {
  return Vec512<double>{_mm512_roundscale_pd(
      v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
HWY_API Vec512<float> Trunc(const Vec512<float> v) {
  return Vec512<float>{
      _mm512_roundscale_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Trunc(const Vec512<double> v) {
  return Vec512<double>{
      _mm512_roundscale_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
HWY_API Vec512<float> Ceil(const Vec512<float> v) {
  return Vec512<float>{
      _mm512_roundscale_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Ceil(const Vec512<double> v) {
  return Vec512<double>{
      _mm512_roundscale_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
HWY_API Vec512<float> Floor(const Vec512<float> v) {
  return Vec512<float>{
      _mm512_roundscale_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Floor(const Vec512<double> v) {
  return Vec512<double>{
      _mm512_roundscale_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

HWY_DIAGNOSTICS(pop)

// ================================================== COMPARE

// Comparisons set a mask bit to 1 if the condition is true, else 0.

template <typename TFrom, typename TTo>
HWY_API Mask512<TTo> RebindMask(Full512<TTo> /*tag*/, Mask512<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask512<TTo>{m.raw};
}

namespace detail {

template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<1> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi8_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<2> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi16_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<4> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi32_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<8> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi64_mask(v.raw, bit.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask512<T> TestBit(const Vec512<T> v, const Vec512<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return detail::TestBit(hwy::SizeTag<sizeof(T)>(), v, bit);
}

// ------------------------------ Equality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask512<float> operator==(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

HWY_API Mask512<double> operator==(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask512<float> operator!=(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

HWY_API Mask512<double> operator!=(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

HWY_API Mask512<uint8_t> operator>(Vec512<uint8_t> a, Vec512<uint8_t> b) {
  return Mask512<uint8_t>{_mm512_cmpgt_epu8_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint16_t> operator>(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Mask512<uint16_t>{_mm512_cmpgt_epu16_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint32_t> operator>(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Mask512<uint32_t>{_mm512_cmpgt_epu32_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint64_t> operator>(Vec512<uint64_t> a, Vec512<uint64_t> b) {
  return Mask512<uint64_t>{_mm512_cmpgt_epu64_mask(a.raw, b.raw)};
}

HWY_API Mask512<int8_t> operator>(Vec512<int8_t> a, Vec512<int8_t> b) {
  return Mask512<int8_t>{_mm512_cmpgt_epi8_mask(a.raw, b.raw)};
}
HWY_API Mask512<int16_t> operator>(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Mask512<int16_t>{_mm512_cmpgt_epi16_mask(a.raw, b.raw)};
}
HWY_API Mask512<int32_t> operator>(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Mask512<int32_t>{_mm512_cmpgt_epi32_mask(a.raw, b.raw)};
}
HWY_API Mask512<int64_t> operator>(Vec512<int64_t> a, Vec512<int64_t> b) {
  return Mask512<int64_t>{_mm512_cmpgt_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask512<float> operator>(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_GT_OQ)};
}
HWY_API Mask512<double> operator>(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

HWY_API Mask512<float> operator>=(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_GE_OQ)};
}
HWY_API Mask512<double> operator>=(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_GE_OQ)};
}

// ------------------------------ Reversed comparisons

template <typename T>
HWY_API Mask512<T> operator<(Vec512<T> a, Vec512<T> b) {
  return b > a;
}

template <typename T>
HWY_API Mask512<T> operator<=(Vec512<T> a, Vec512<T> b) {
  return b >= a;
}

// ------------------------------ Mask

namespace detail {

template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<1> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi8_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<2> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi16_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<4> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi32_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<8> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi64_mask(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask512<T> MaskFromVec(const Vec512<T> v) {
  return detail::MaskFromVec(hwy::SizeTag<sizeof(T)>(), v);
}
// There do not seem to be native floating-point versions of these instructions.
HWY_API Mask512<float> MaskFromVec(const Vec512<float> v) {
  return Mask512<float>{MaskFromVec(BitCast(Full512<int32_t>(), v)).raw};
}
HWY_API Mask512<double> MaskFromVec(const Vec512<double> v) {
  return Mask512<double>{MaskFromVec(BitCast(Full512<int64_t>(), v)).raw};
}

HWY_API Vec512<uint8_t> VecFromMask(const Mask512<uint8_t> v) {
  return Vec512<uint8_t>{_mm512_movm_epi8(v.raw)};
}
HWY_API Vec512<int8_t> VecFromMask(const Mask512<int8_t> v) {
  return Vec512<int8_t>{_mm512_movm_epi8(v.raw)};
}

HWY_API Vec512<uint16_t> VecFromMask(const Mask512<uint16_t> v) {
  return Vec512<uint16_t>{_mm512_movm_epi16(v.raw)};
}
HWY_API Vec512<int16_t> VecFromMask(const Mask512<int16_t> v) {
  return Vec512<int16_t>{_mm512_movm_epi16(v.raw)};
}

HWY_API Vec512<uint32_t> VecFromMask(const Mask512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_movm_epi32(v.raw)};
}
HWY_API Vec512<int32_t> VecFromMask(const Mask512<int32_t> v) {
  return Vec512<int32_t>{_mm512_movm_epi32(v.raw)};
}
HWY_API Vec512<float> VecFromMask(const Mask512<float> v) {
  return Vec512<float>{_mm512_castsi512_ps(_mm512_movm_epi32(v.raw))};
}

HWY_API Vec512<uint64_t> VecFromMask(const Mask512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_movm_epi64(v.raw)};
}
HWY_API Vec512<int64_t> VecFromMask(const Mask512<int64_t> v) {
  return Vec512<int64_t>{_mm512_movm_epi64(v.raw)};
}
HWY_API Vec512<double> VecFromMask(const Mask512<double> v) {
  return Vec512<double>{_mm512_castsi512_pd(_mm512_movm_epi64(v.raw))};
}

template <typename T>
HWY_API Vec512<T> VecFromMask(Full512<T> /* tag */, const Mask512<T> v) {
  return VecFromMask(v);
}

// ------------------------------ Mask logical

namespace detail {

template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<1> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask64(m.raw)};
#else
  return Mask512<T>{~m.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<2> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask32(m.raw)};
#else
  return Mask512<T>{~m.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<4> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask16(m.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(~m.raw & 0xFFFF)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<8> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask8(m.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(~m.raw & 0xFF)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{~a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{~a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(~a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(~a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw | b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw | b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(a.raw | b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(a.raw | b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw ^ b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw ^ b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(a.raw ^ b.raw)};
#endif
}

}  // namespace detail

template <typename T>
HWY_API Mask512<T> Not(const Mask512<T> m) {
  return detail::Not(hwy::SizeTag<sizeof(T)>(), m);
}

template <typename T>
HWY_API Mask512<T> And(const Mask512<T> a, Mask512<T> b) {
  return detail::And(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> AndNot(const Mask512<T> a, Mask512<T> b) {
  return detail::AndNot(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> Or(const Mask512<T> a, Mask512<T> b) {
  return detail::Or(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> Xor(const Mask512<T> a, Mask512<T> b) {
  return detail::Xor(hwy::SizeTag<sizeof(T)>(), a, b);
}

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

HWY_API Vec512<int8_t> BroadcastSignBit(const Vec512<int8_t> v) {
  return VecFromMask(v < Zero(Full512<int8_t>()));
}

HWY_API Vec512<int16_t> BroadcastSignBit(const Vec512<int16_t> v) {
  return ShiftRight<15>(v);
}

HWY_API Vec512<int32_t> BroadcastSignBit(const Vec512<int32_t> v) {
  return ShiftRight<31>(v);
}

HWY_API Vec512<int64_t> BroadcastSignBit(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_srai_epi64(v.raw, 63)};
}

// ------------------------------ Floating-point classification (Not)

HWY_API Mask512<float> IsNaN(const Vec512<float> v) {
  return Mask512<float>{_mm512_fpclass_ps_mask(v.raw, 0x81)};
}
HWY_API Mask512<double> IsNaN(const Vec512<double> v) {
  return Mask512<double>{_mm512_fpclass_pd_mask(v.raw, 0x81)};
}

HWY_API Mask512<float> IsInf(const Vec512<float> v) {
  return Mask512<float>{_mm512_fpclass_ps_mask(v.raw, 0x18)};
}
HWY_API Mask512<double> IsInf(const Vec512<double> v) {
  return Mask512<double>{_mm512_fpclass_pd_mask(v.raw, 0x18)};
}

// Returns whether normal/subnormal/zero. fpclass doesn't have a flag for
// positive, so we have to check for inf/NaN and negate.
HWY_API Mask512<float> IsFinite(const Vec512<float> v) {
  return Not(Mask512<float>{_mm512_fpclass_ps_mask(v.raw, 0x99)});
}
HWY_API Mask512<double> IsFinite(const Vec512<double> v) {
  return Not(Mask512<double>{_mm512_fpclass_pd_mask(v.raw, 0x99)});
}

// ================================================== MEMORY

// ------------------------------ Load

template <typename T>
HWY_API Vec512<T> Load(Full512<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec512<T>{_mm512_load_si512(aligned)};
}
HWY_API Vec512<float> Load(Full512<float> /* tag */,
                           const float* HWY_RESTRICT aligned) {
  return Vec512<float>{_mm512_load_ps(aligned)};
}
HWY_API Vec512<double> Load(Full512<double> /* tag */,
                            const double* HWY_RESTRICT aligned) {
  return Vec512<double>{_mm512_load_pd(aligned)};
}

template <typename T>
HWY_API Vec512<T> LoadU(Full512<T> /* tag */, const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_loadu_si512(p)};
}
HWY_API Vec512<float> LoadU(Full512<float> /* tag */,
                            const float* HWY_RESTRICT p) {
  return Vec512<float>{_mm512_loadu_ps(p)};
}
HWY_API Vec512<double> LoadU(Full512<double> /* tag */,
                             const double* HWY_RESTRICT p) {
  return Vec512<double>{_mm512_loadu_pd(p)};
}

// ------------------------------ MaskedLoad

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, Full512<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi8(m.raw, p)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, Full512<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi16(m.raw, p)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, Full512<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi32(m.raw, p)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, Full512<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi64(m.raw, p)};
}

HWY_API Vec512<float> MaskedLoad(Mask512<float> m, Full512<float> /* tag */,
                                 const float* HWY_RESTRICT p) {
  return Vec512<float>{_mm512_maskz_loadu_ps(m.raw, p)};
}

HWY_API Vec512<double> MaskedLoad(Mask512<double> m, Full512<double> /* tag */,
                                  const double* HWY_RESTRICT p) {
  return Vec512<double>{_mm512_maskz_loadu_pd(m.raw, p)};
}

// ------------------------------ LoadDup128

// Loads 128 bit and duplicates into both 128-bit halves. This avoids the
// 3-cycle cost of moving data between 128-bit halves and avoids port 5.
template <typename T>
HWY_API Vec512<T> LoadDup128(Full512<T> /* tag */,
                             const T* const HWY_RESTRICT p) {
  const auto x4 = LoadU(Full128<T>(), p);
  return Vec512<T>{_mm512_broadcast_i32x4(x4.raw)};
}
HWY_API Vec512<float> LoadDup128(Full512<float> /* tag */,
                                 const float* const HWY_RESTRICT p) {
  const __m128 x4 = _mm_loadu_ps(p);
  return Vec512<float>{_mm512_broadcast_f32x4(x4)};
}

HWY_API Vec512<double> LoadDup128(Full512<double> /* tag */,
                                  const double* const HWY_RESTRICT p) {
  const __m128d x2 = _mm_loadu_pd(p);
  return Vec512<double>{_mm512_broadcast_f64x2(x2)};
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(const Vec512<T> v, Full512<T> /* tag */,
                   T* HWY_RESTRICT aligned) {
  _mm512_store_si512(reinterpret_cast<__m512i*>(aligned), v.raw);
}
HWY_API void Store(const Vec512<float> v, Full512<float> /* tag */,
                   float* HWY_RESTRICT aligned) {
  _mm512_store_ps(aligned, v.raw);
}
HWY_API void Store(const Vec512<double> v, Full512<double> /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm512_store_pd(aligned, v.raw);
}

template <typename T>
HWY_API void StoreU(const Vec512<T> v, Full512<T> /* tag */,
                    T* HWY_RESTRICT p) {
  _mm512_storeu_si512(reinterpret_cast<__m512i*>(p), v.raw);
}
HWY_API void StoreU(const Vec512<float> v, Full512<float> /* tag */,
                    float* HWY_RESTRICT p) {
  _mm512_storeu_ps(p, v.raw);
}
HWY_API void StoreU(const Vec512<double> v, Full512<double>,
                    double* HWY_RESTRICT p) {
  _mm512_storeu_pd(p, v.raw);
}

// ------------------------------ BlendedStore

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, Full512<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi8(p, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, Full512<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi16(p, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, Full512<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi32(p, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, Full512<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi64(p, m.raw, v.raw);
}

HWY_API void BlendedStore(Vec512<float> v, Mask512<float> m,
                          Full512<float> /* tag */, float* HWY_RESTRICT p) {
  _mm512_mask_storeu_ps(p, m.raw, v.raw);
}

HWY_API void BlendedStore(Vec512<double> v, Mask512<double> m,
                          Full512<double> /* tag */, double* HWY_RESTRICT p) {
  _mm512_mask_storeu_pd(p, m.raw, v.raw);
}

// ------------------------------ Non-temporal stores

template <typename T>
HWY_API void Stream(const Vec512<T> v, Full512<T> /* tag */,
                    T* HWY_RESTRICT aligned) {
  _mm512_stream_si512(reinterpret_cast<__m512i*>(aligned), v.raw);
}
HWY_API void Stream(const Vec512<float> v, Full512<float> /* tag */,
                    float* HWY_RESTRICT aligned) {
  _mm512_stream_ps(aligned, v.raw);
}
HWY_API void Stream(const Vec512<double> v, Full512<double>,
                    double* HWY_RESTRICT aligned) {
  _mm512_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

namespace detail {

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<4> /* tag */, Vec512<T> v,
                              Full512<T> /* tag */, T* HWY_RESTRICT base,
                              const Vec512<int32_t> offset) {
  _mm512_i32scatter_epi32(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<4> /* tag */, Vec512<T> v,
                             Full512<T> /* tag */, T* HWY_RESTRICT base,
                             const Vec512<int32_t> index) {
  _mm512_i32scatter_epi32(base, index.raw, v.raw, 4);
}

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<8> /* tag */, Vec512<T> v,
                              Full512<T> /* tag */, T* HWY_RESTRICT base,
                              const Vec512<int64_t> offset) {
  _mm512_i64scatter_epi64(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<8> /* tag */, Vec512<T> v,
                             Full512<T> /* tag */, T* HWY_RESTRICT base,
                             const Vec512<int64_t> index) {
  _mm512_i64scatter_epi64(base, index.raw, v.raw, 8);
}

}  // namespace detail

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec512<T> v, Full512<T> d, T* HWY_RESTRICT base,
                           const Vec512<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::ScatterOffset(hwy::SizeTag<sizeof(T)>(), v, d, base, offset);
}
template <typename T, typename Index>
HWY_API void ScatterIndex(Vec512<T> v, Full512<T> d, T* HWY_RESTRICT base,
                          const Vec512<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::ScatterIndex(hwy::SizeTag<sizeof(T)>(), v, d, base, index);
}

HWY_API void ScatterOffset(Vec512<float> v, Full512<float> /* tag */,
                           float* HWY_RESTRICT base,
                           const Vec512<int32_t> offset) {
  _mm512_i32scatter_ps(base, offset.raw, v.raw, 1);
}
HWY_API void ScatterIndex(Vec512<float> v, Full512<float> /* tag */,
                          float* HWY_RESTRICT base,
                          const Vec512<int32_t> index) {
  _mm512_i32scatter_ps(base, index.raw, v.raw, 4);
}

HWY_API void ScatterOffset(Vec512<double> v, Full512<double> /* tag */,
                           double* HWY_RESTRICT base,
                           const Vec512<int64_t> offset) {
  _mm512_i64scatter_pd(base, offset.raw, v.raw, 1);
}
HWY_API void ScatterIndex(Vec512<double> v, Full512<double> /* tag */,
                          double* HWY_RESTRICT base,
                          const Vec512<int64_t> index) {
  _mm512_i64scatter_pd(base, index.raw, v.raw, 8);
}

// ------------------------------ Gather

namespace detail {

template <typename T>
HWY_INLINE Vec512<T> GatherOffset(hwy::SizeTag<4> /* tag */,
                                  Full512<T> /* tag */,
                                  const T* HWY_RESTRICT base,
                                  const Vec512<int32_t> offset) {
  return Vec512<T>{_mm512_i32gather_epi32(offset.raw, base, 1)};
}
template <typename T>
HWY_INLINE Vec512<T> GatherIndex(hwy::SizeTag<4> /* tag */,
                                 Full512<T> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec512<int32_t> index) {
  return Vec512<T>{_mm512_i32gather_epi32(index.raw, base, 4)};
}

template <typename T>
HWY_INLINE Vec512<T> GatherOffset(hwy::SizeTag<8> /* tag */,
                                  Full512<T> /* tag */,
                                  const T* HWY_RESTRICT base,
                                  const Vec512<int64_t> offset) {
  return Vec512<T>{_mm512_i64gather_epi64(offset.raw, base, 1)};
}
template <typename T>
HWY_INLINE Vec512<T> GatherIndex(hwy::SizeTag<8> /* tag */,
                                 Full512<T> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec512<int64_t> index) {
  return Vec512<T>{_mm512_i64gather_epi64(index.raw, base, 8)};
}

}  // namespace detail

template <typename T, typename Offset>
HWY_API Vec512<T> GatherOffset(Full512<T> d, const T* HWY_RESTRICT base,
                               const Vec512<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::GatherOffset(hwy::SizeTag<sizeof(T)>(), d, base, offset);
}
template <typename T, typename Index>
HWY_API Vec512<T> GatherIndex(Full512<T> d, const T* HWY_RESTRICT base,
                              const Vec512<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::GatherIndex(hwy::SizeTag<sizeof(T)>(), d, base, index);
}

HWY_API Vec512<float> GatherOffset(Full512<float> /* tag */,
                                   const float* HWY_RESTRICT base,
                                   const Vec512<int32_t> offset) {
  return Vec512<float>{_mm512_i32gather_ps(offset.raw, base, 1)};
}
HWY_API Vec512<float> GatherIndex(Full512<float> /* tag */,
                                  const float* HWY_RESTRICT base,
                                  const Vec512<int32_t> index) {
  return Vec512<float>{_mm512_i32gather_ps(index.raw, base, 4)};
}

HWY_API Vec512<double> GatherOffset(Full512<double> /* tag */,
                                    const double* HWY_RESTRICT base,
                                    const Vec512<int64_t> offset) {
  return Vec512<double>{_mm512_i64gather_pd(offset.raw, base, 1)};
}
HWY_API Vec512<double> GatherIndex(Full512<double> /* tag */,
                                   const double* HWY_RESTRICT base,
                                   const Vec512<int64_t> index) {
  return Vec512<double>{_mm512_i64gather_pd(index.raw, base, 8)};
}

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE

// ------------------------------ LowerHalf

template <typename T>
HWY_API Vec256<T> LowerHalf(Full256<T> /* tag */, Vec512<T> v) {
  return Vec256<T>{_mm512_castsi512_si256(v.raw)};
}
HWY_API Vec256<float> LowerHalf(Full256<float> /* tag */, Vec512<float> v) {
  return Vec256<float>{_mm512_castps512_ps256(v.raw)};
}
HWY_API Vec256<double> LowerHalf(Full256<double> /* tag */, Vec512<double> v) {
  return Vec256<double>{_mm512_castpd512_pd256(v.raw)};
}

template <typename T>
HWY_API Vec256<T> LowerHalf(Vec512<T> v) {
  return LowerHalf(Full256<T>(), v);
}

// ------------------------------ UpperHalf

template <typename T>
HWY_API Vec256<T> UpperHalf(Full256<T> /* tag */, Vec512<T> v) {
  return Vec256<T>{_mm512_extracti32x8_epi32(v.raw, 1)};
}
HWY_API Vec256<float> UpperHalf(Full256<float> /* tag */, Vec512<float> v) {
  return Vec256<float>{_mm512_extractf32x8_ps(v.raw, 1)};
}
HWY_API Vec256<double> UpperHalf(Full256<double> /* tag */, Vec512<double> v) {
  return Vec256<double>{_mm512_extractf64x4_pd(v.raw, 1)};
}

// ------------------------------ ExtractLane (Store)
template <typename T>
HWY_API T ExtractLane(const Vec512<T> v, size_t i) {
  const Full512<T> d;
  HWY_DASSERT(i < Lanes(d));
  alignas(64) T lanes[64 / sizeof(T)];
  Store(v, d, lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (Store)
template <typename T>
HWY_API Vec512<T> InsertLane(const Vec512<T> v, size_t i, T t) {
  const Full512<T> d;
  HWY_DASSERT(i < Lanes(d));
  alignas(64) T lanes[64 / sizeof(T)];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec512<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ZeroExtendVector

template <typename T>
HWY_API Vec512<T> ZeroExtendVector(Full512<T> /* tag */, Vec256<T> lo) {
#if HWY_HAVE_ZEXT  // See definition/comment in x86_256-inl.h.
  return Vec512<T>{_mm512_zextsi256_si512(lo.raw)};
#else
  return Vec512<T>{_mm512_inserti32x8(_mm512_setzero_si512(), lo.raw, 0)};
#endif
}
HWY_API Vec512<float> ZeroExtendVector(Full512<float> /* tag */,
                                       Vec256<float> lo) {
#if HWY_HAVE_ZEXT
  return Vec512<float>{_mm512_zextps256_ps512(lo.raw)};
#else
  return Vec512<float>{_mm512_insertf32x8(_mm512_setzero_ps(), lo.raw, 0)};
#endif
}
HWY_API Vec512<double> ZeroExtendVector(Full512<double> /* tag */,
                                        Vec256<double> lo) {
#if HWY_HAVE_ZEXT
  return Vec512<double>{_mm512_zextpd256_pd512(lo.raw)};
#else
  return Vec512<double>{_mm512_insertf64x4(_mm512_setzero_pd(), lo.raw, 0)};
#endif
}

// ------------------------------ Combine

template <typename T>
HWY_API Vec512<T> Combine(Full512<T> d, Vec256<T> hi, Vec256<T> lo) {
  const auto lo512 = ZeroExtendVector(d, lo);
  return Vec512<T>{_mm512_inserti32x8(lo512.raw, hi.raw, 1)};
}
HWY_API Vec512<float> Combine(Full512<float> d, Vec256<float> hi,
                              Vec256<float> lo) {
  const auto lo512 = ZeroExtendVector(d, lo);
  return Vec512<float>{_mm512_insertf32x8(lo512.raw, hi.raw, 1)};
}
HWY_API Vec512<double> Combine(Full512<double> d, Vec256<double> hi,
                               Vec256<double> lo) {
  const auto lo512 = ZeroExtendVector(d, lo);
  return Vec512<double>{_mm512_insertf64x4(lo512.raw, hi.raw, 1)};
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, typename T>
HWY_API Vec512<T> ShiftLeftBytes(Full512<T> /* tag */, const Vec512<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec512<T>{_mm512_bslli_epi128(v.raw, kBytes)};
}

template <int kBytes, typename T>
HWY_API Vec512<T> ShiftLeftBytes(const Vec512<T> v) {
  return ShiftLeftBytes<kBytes>(Full512<T>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, typename T>
HWY_API Vec512<T> ShiftLeftLanes(Full512<T> d, const Vec512<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T>
HWY_API Vec512<T> ShiftLeftLanes(const Vec512<T> v) {
  return ShiftLeftLanes<kLanes>(Full512<T>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, typename T>
HWY_API Vec512<T> ShiftRightBytes(Full512<T> /* tag */, const Vec512<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec512<T>{_mm512_bsrli_epi128(v.raw, kBytes)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T>
HWY_API Vec512<T> ShiftRightLanes(Full512<T> d, const Vec512<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, typename T, class V = Vec512<T>>
HWY_API V CombineShiftRightBytes(Full512<T> d, V hi, V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec512<uint8_t>{_mm512_alignr_epi8(
                        BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)});
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane>
HWY_API Vec512<uint16_t> Broadcast(const Vec512<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m512i lo = _mm512_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec512<uint16_t>{_mm512_unpacklo_epi64(lo, lo)};
  } else {
    const __m512i hi =
        _mm512_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec512<uint16_t>{_mm512_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec512<uint32_t> Broadcast(const Vec512<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0x55 * kLane);
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, perm)};
}
template <int kLane>
HWY_API Vec512<uint64_t> Broadcast(const Vec512<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = kLane ? _MM_PERM_DCDC : _MM_PERM_BABA;
  return Vec512<uint64_t>{_mm512_shuffle_epi32(v.raw, perm)};
}

// Signed
template <int kLane>
HWY_API Vec512<int16_t> Broadcast(const Vec512<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m512i lo = _mm512_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec512<int16_t>{_mm512_unpacklo_epi64(lo, lo)};
  } else {
    const __m512i hi =
        _mm512_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec512<int16_t>{_mm512_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec512<int32_t> Broadcast(const Vec512<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0x55 * kLane);
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, perm)};
}
template <int kLane>
HWY_API Vec512<int64_t> Broadcast(const Vec512<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = kLane ? _MM_PERM_DCDC : _MM_PERM_BABA;
  return Vec512<int64_t>{_mm512_shuffle_epi32(v.raw, perm)};
}

// Float
template <int kLane>
HWY_API Vec512<float> Broadcast(const Vec512<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0x55 * kLane);
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, perm)};
}
template <int kLane>
HWY_API Vec512<double> Broadcast(const Vec512<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0xFF * kLane);
  return Vec512<double>{_mm512_shuffle_pd(v.raw, v.raw, perm)};
}

// ------------------------------ Hard-coded shuffles

// Notation: let Vec512<int32_t> have lanes 7,6,5,4,3,2,1,0 (0 is
// least-significant). Shuffle0321 rotates four-lane blocks one lane to the
// right (the previous least-significant lane is now most-significant =>
// 47650321). These could also be implemented via CombineShiftRightBytes but
// the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Shuffle2301(const Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CDAB)};
}
HWY_API Vec512<float> Shuffle2301(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_CDAB)};
}

namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Shuffle2301(const Vec512<T> a, const Vec512<T> b) {
  const Full512<T> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec512<float>{_mm512_shuffle_ps(BitCast(df, a).raw, BitCast(df, b).raw,
                                         _MM_PERM_CDAB)});
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Shuffle1230(const Vec512<T> a, const Vec512<T> b) {
  const Full512<T> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec512<float>{_mm512_shuffle_ps(BitCast(df, a).raw, BitCast(df, b).raw,
                                         _MM_PERM_BCDA)});
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Shuffle3012(const Vec512<T> a, const Vec512<T> b) {
  const Full512<T> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec512<float>{_mm512_shuffle_ps(BitCast(df, a).raw, BitCast(df, b).raw,
                                         _MM_PERM_DABC)});
}

}  // namespace detail

// Swap 64-bit halves
HWY_API Vec512<uint32_t> Shuffle1032(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<int32_t> Shuffle1032(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<float> Shuffle1032(const Vec512<float> v) {
  // Shorter encoding than _mm512_permute_ps.
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<uint64_t> Shuffle01(const Vec512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<int64_t> Shuffle01(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<double> Shuffle01(const Vec512<double> v) {
  // Shorter encoding than _mm512_permute_pd.
  return Vec512<double>{_mm512_shuffle_pd(v.raw, v.raw, _MM_PERM_BBBB)};
}

// Rotate right 32 bits
HWY_API Vec512<uint32_t> Shuffle0321(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ADCB)};
}
HWY_API Vec512<int32_t> Shuffle0321(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ADCB)};
}
HWY_API Vec512<float> Shuffle0321(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_ADCB)};
}
// Rotate left 32 bits
HWY_API Vec512<uint32_t> Shuffle2103(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CBAD)};
}
HWY_API Vec512<int32_t> Shuffle2103(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CBAD)};
}
HWY_API Vec512<float> Shuffle2103(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_CBAD)};
}

// Reverse
HWY_API Vec512<uint32_t> Shuffle0123(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ABCD)};
}
HWY_API Vec512<int32_t> Shuffle0123(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ABCD)};
}
HWY_API Vec512<float> Shuffle0123(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_ABCD)};
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T>
struct Indices512 {
  __m512i raw;
};

template <typename T, typename TI>
HWY_API Indices512<T> IndicesFromVec(Full512<T> /* tag */, Vec512<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Full512<TI> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(64 / sizeof(T))))));
#endif
  return Indices512<T>{vec.raw};
}

template <typename T, typename TI>
HWY_API Indices512<T> SetTableIndices(const Full512<T> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> TableLookupLanes(Vec512<T> v, Indices512<T> idx) {
  return Vec512<T>{_mm512_permutexvar_epi32(idx.raw, v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> TableLookupLanes(Vec512<T> v, Indices512<T> idx) {
  return Vec512<T>{_mm512_permutexvar_epi64(idx.raw, v.raw)};
}

HWY_API Vec512<float> TableLookupLanes(Vec512<float> v, Indices512<float> idx) {
  return Vec512<float>{_mm512_permutexvar_ps(idx.raw, v.raw)};
}

HWY_API Vec512<double> TableLookupLanes(Vec512<double> v,
                                        Indices512<double> idx) {
  return Vec512<double>{_mm512_permutexvar_pd(idx.raw, v.raw)};
}

// ------------------------------ Reverse

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> Reverse(Full512<T> d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) constexpr int16_t kReverse[32] = {
      31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
      15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
  const Vec512<int16_t> idx = Load(di, kReverse);
  return BitCast(d, Vec512<int16_t>{
                        _mm512_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Reverse(Full512<T> d, const Vec512<T> v) {
  alignas(64) constexpr int32_t kReverse[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                                7,  6,  5,  4,  3,  2,  1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> Reverse(Full512<T> d, const Vec512<T> v) {
  alignas(64) constexpr int64_t kReverse[8] = {7, 6, 5, 4, 3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

// ------------------------------ Reverse2

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> Reverse2(Full512<T> d, const Vec512<T> v) {
  const Full512<uint32_t> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Reverse2(Full512<T> /* tag */, const Vec512<T> v) {
  return Shuffle2301(v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> Reverse2(Full512<T> /* tag */, const Vec512<T> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> Reverse4(Full512<T> d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) constexpr int16_t kReverse4[32] = {
      3,  2,  1,  0,  7,  6,  5,  4,  11, 10, 9,  8,  15, 14, 13, 12,
      19, 18, 17, 16, 23, 22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28};
  const Vec512<int16_t> idx = Load(di, kReverse4);
  return BitCast(d, Vec512<int16_t>{
                        _mm512_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Reverse4(Full512<T> /* tag */, const Vec512<T> v) {
  return Shuffle0123(v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> Reverse4(Full512<T> /* tag */, const Vec512<T> v) {
  return Vec512<T>{_mm512_permutex_epi64(v.raw, _MM_SHUFFLE(0, 1, 2, 3))};
}
HWY_API Vec512<double> Reverse4(Full512<double> /* tag */, Vec512<double> v) {
  return Vec512<double>{_mm512_permutex_pd(v.raw, _MM_SHUFFLE(0, 1, 2, 3))};
}

// ------------------------------ Reverse8

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> Reverse8(Full512<T> d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) constexpr int16_t kReverse8[32] = {
      7,  6,  5,  4,  3,  2,  1,  0,  15, 14, 13, 12, 11, 10, 9,  8,
      23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24};
  const Vec512<int16_t> idx = Load(di, kReverse8);
  return BitCast(d, Vec512<int16_t>{
                        _mm512_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Reverse8(Full512<T> d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) constexpr int32_t kReverse8[16] = {7,  6,  5,  4,  3,  2,  1, 0,
                                                 15, 14, 13, 12, 11, 10, 9, 8};
  const Vec512<int32_t> idx = Load(di, kReverse8);
  return BitCast(d, Vec512<int32_t>{
                        _mm512_permutexvar_epi32(idx.raw, BitCast(di, v).raw)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> Reverse8(Full512<T> d, const Vec512<T> v) {
  return Reverse(d, v);
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

HWY_API Vec512<uint8_t> InterleaveLower(const Vec512<uint8_t> a,
                                        const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> InterleaveLower(const Vec512<uint16_t> a,
                                         const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> InterleaveLower(const Vec512<uint32_t> a,
                                         const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> InterleaveLower(const Vec512<uint64_t> a,
                                         const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec512<int8_t> InterleaveLower(const Vec512<int8_t> a,
                                       const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> InterleaveLower(const Vec512<int16_t> a,
                                        const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> InterleaveLower(const Vec512<int32_t> a,
                                        const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> InterleaveLower(const Vec512<int64_t> a,
                                        const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec512<float> InterleaveLower(const Vec512<float> a,
                                      const Vec512<float> b) {
  return Vec512<float>{_mm512_unpacklo_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> InterleaveLower(const Vec512<double> a,
                                       const Vec512<double> b) {
  return Vec512<double>{_mm512_unpacklo_pd(a.raw, b.raw)};
}

// ------------------------------ InterleaveUpper

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec512<uint8_t> InterleaveUpper(const Vec512<uint8_t> a,
                                        const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> InterleaveUpper(const Vec512<uint16_t> a,
                                         const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> InterleaveUpper(const Vec512<uint32_t> a,
                                         const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> InterleaveUpper(const Vec512<uint64_t> a,
                                         const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec512<int8_t> InterleaveUpper(const Vec512<int8_t> a,
                                       const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> InterleaveUpper(const Vec512<int16_t> a,
                                        const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> InterleaveUpper(const Vec512<int32_t> a,
                                        const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> InterleaveUpper(const Vec512<int64_t> a,
                                        const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec512<float> InterleaveUpper(const Vec512<float> a,
                                      const Vec512<float> b) {
  return Vec512<float>{_mm512_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> InterleaveUpper(const Vec512<double> a,
                                       const Vec512<double> b) {
  return Vec512<double>{_mm512_unpackhi_pd(a.raw, b.raw)};
}

}  // namespace detail

template <typename T, class V = Vec512<T>>
HWY_API V InterleaveUpper(Full512<T> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec512<TW> ZipLower(Vec512<T> a, Vec512<T> b) {
  return BitCast(Full512<TW>(), InterleaveLower(a, b));
}
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec512<TW> ZipLower(Full512<TW> /* d */, Vec512<T> a, Vec512<T> b) {
  return BitCast(Full512<TW>(), InterleaveLower(a, b));
}

template <typename T, typename TW = MakeWide<T>>
HWY_API Vec512<TW> ZipUpper(Full512<TW> d, Vec512<T> a, Vec512<T> b) {
  return BitCast(Full512<TW>(), InterleaveUpper(d, a, b));
}

// ------------------------------ Concat* halves

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec512<T> ConcatLowerLower(Full512<T> /* tag */, const Vec512<T> hi,
                                   const Vec512<T> lo) {
  return Vec512<T>{_mm512_shuffle_i32x4(lo.raw, hi.raw, _MM_PERM_BABA)};
}
HWY_API Vec512<float> ConcatLowerLower(Full512<float> /* tag */,
                                       const Vec512<float> hi,
                                       const Vec512<float> lo) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, _MM_PERM_BABA)};
}
HWY_API Vec512<double> ConcatLowerLower(Full512<double> /* tag */,
                                        const Vec512<double> hi,
                                        const Vec512<double> lo) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, _MM_PERM_BABA)};
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <typename T>
HWY_API Vec512<T> ConcatUpperUpper(Full512<T> /* tag */, const Vec512<T> hi,
                                   const Vec512<T> lo) {
  return Vec512<T>{_mm512_shuffle_i32x4(lo.raw, hi.raw, _MM_PERM_DCDC)};
}
HWY_API Vec512<float> ConcatUpperUpper(Full512<float> /* tag */,
                                       const Vec512<float> hi,
                                       const Vec512<float> lo) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, _MM_PERM_DCDC)};
}
HWY_API Vec512<double> ConcatUpperUpper(Full512<double> /* tag */,
                                        const Vec512<double> hi,
                                        const Vec512<double> lo) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, _MM_PERM_DCDC)};
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves / swap blocks)
template <typename T>
HWY_API Vec512<T> ConcatLowerUpper(Full512<T> /* tag */, const Vec512<T> hi,
                                   const Vec512<T> lo) {
  return Vec512<T>{_mm512_shuffle_i32x4(lo.raw, hi.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<float> ConcatLowerUpper(Full512<float> /* tag */,
                                       const Vec512<float> hi,
                                       const Vec512<float> lo) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<double> ConcatLowerUpper(Full512<double> /* tag */,
                                        const Vec512<double> hi,
                                        const Vec512<double> lo) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, _MM_PERM_BADC)};
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <typename T>
HWY_API Vec512<T> ConcatUpperLower(Full512<T> /* tag */, const Vec512<T> hi,
                                   const Vec512<T> lo) {
  // There are no imm8 blend in AVX512. Use blend16 because 32-bit masks
  // are efficiently loaded from 32-bit regs.
  const __mmask32 mask = /*_cvtu32_mask32 */ (0x0000FFFF);
  return Vec512<T>{_mm512_mask_blend_epi16(mask, hi.raw, lo.raw)};
}
HWY_API Vec512<float> ConcatUpperLower(Full512<float> /* tag */,
                                       const Vec512<float> hi,
                                       const Vec512<float> lo) {
  const __mmask16 mask = /*_cvtu32_mask16 */ (0x00FF);
  return Vec512<float>{_mm512_mask_blend_ps(mask, hi.raw, lo.raw)};
}
HWY_API Vec512<double> ConcatUpperLower(Full512<double> /* tag */,
                                        const Vec512<double> hi,
                                        const Vec512<double> lo) {
  const __mmask8 mask = /*_cvtu32_mask8 */ (0x0F);
  return Vec512<double>{_mm512_mask_blend_pd(mask, hi.raw, lo.raw)};
}

// ------------------------------ ConcatOdd

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec512<T> ConcatOdd(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET == HWY_AVX3_DL
  alignas(64) constexpr uint8_t kIdx[64] = {
      1,   3,   5,   7,   9,   11,  13,  15,  17,  19,  21,  23,  25,
      27,  29,  31,  33,  35,  37,  39,  41,  43,  45,  47,  49,  51,
      53,  55,  57,  59,  61,  63,  65,  67,  69,  71,  73,  75,  77,
      79,  81,  83,  85,  87,  89,  91,  93,  95,  97,  99,  101, 103,
      105, 107, 109, 111, 113, 115, 117, 119, 121, 123, 125, 127};
  return BitCast(d,
                 Vec512<uint8_t>{_mm512_mask2_permutex2var_epi8(
                     BitCast(du, lo).raw, Load(du, kIdx).raw,
                     __mmask64{0xFFFFFFFFFFFFFFFFull}, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Right-shift 8 bits per u16 so we can pack.
  const Vec512<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec512<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  const Vec512<uint64_t> u8{_mm512_packus_epi16(uL.raw, uH.raw)};
  // Undo block interleave: lower half = even u64 lanes, upper = odd u64 lanes.
  const Full512<uint64_t> du64;
  alignas(64) constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 1, 3, 5, 7};
  return BitCast(d, TableLookupLanes(u8, SetTableIndices(du64, kIdx)));
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> ConcatOdd(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint16_t kIdx[32] = {
      1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,
      33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63};
  return BitCast(d, Vec512<uint16_t>{_mm512_mask2_permutex2var_epi16(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask32{0xFFFFFFFFu}, BitCast(du, hi).raw)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> ConcatOdd(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint32_t kIdx[16] = {1,  3,  5,  7,  9,  11, 13, 15,
                                             17, 19, 21, 23, 25, 27, 29, 31};
  return BitCast(d, Vec512<uint32_t>{_mm512_mask2_permutex2var_epi32(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask16{0xFFFF}, BitCast(du, hi).raw)});
}

HWY_API Vec512<float> ConcatOdd(Full512<float> d, Vec512<float> hi,
                                Vec512<float> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint32_t kIdx[16] = {1,  3,  5,  7,  9,  11, 13, 15,
                                             17, 19, 21, 23, 25, 27, 29, 31};
  return Vec512<float>{_mm512_mask2_permutex2var_ps(lo.raw, Load(du, kIdx).raw,
                                                    __mmask16{0xFFFF}, hi.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> ConcatOdd(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint64_t kIdx[8] = {1, 3, 5, 7, 9, 11, 13, 15};
  return BitCast(d, Vec512<uint64_t>{_mm512_mask2_permutex2var_epi64(
                        BitCast(du, lo).raw, Load(du, kIdx).raw, __mmask8{0xFF},
                        BitCast(du, hi).raw)});
}

HWY_API Vec512<double> ConcatOdd(Full512<double> d, Vec512<double> hi,
                                 Vec512<double> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint64_t kIdx[8] = {1, 3, 5, 7, 9, 11, 13, 15};
  return Vec512<double>{_mm512_mask2_permutex2var_pd(lo.raw, Load(du, kIdx).raw,
                                                     __mmask8{0xFF}, hi.raw)};
}

// ------------------------------ ConcatEven

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec512<T> ConcatEven(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET == HWY_AVX3_DL
  alignas(64) constexpr uint8_t kIdx[64] = {
      0,   2,   4,   6,   8,   10,  12,  14,  16,  18,  20,  22,  24,
      26,  28,  30,  32,  34,  36,  38,  40,  42,  44,  46,  48,  50,
      52,  54,  56,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,
      78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98,  100, 102,
      104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126};
  return BitCast(d,
                 Vec512<uint32_t>{_mm512_mask2_permutex2var_epi8(
                     BitCast(du, lo).raw, Load(du, kIdx).raw,
                     __mmask64{0xFFFFFFFFFFFFFFFFull}, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec512<uint16_t> mask = Set(dw, 0x00FF);
  const Vec512<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec512<uint16_t> uL = And(BitCast(dw, lo), mask);
  const Vec512<uint64_t> u8{_mm512_packus_epi16(uL.raw, uH.raw)};
  // Undo block interleave: lower half = even u64 lanes, upper = odd u64 lanes.
  const Full512<uint64_t> du64;
  alignas(64) constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 1, 3, 5, 7};
  return BitCast(d, TableLookupLanes(u8, SetTableIndices(du64, kIdx)));
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> ConcatEven(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint16_t kIdx[32] = {
      0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62};
  return BitCast(d, Vec512<uint32_t>{_mm512_mask2_permutex2var_epi16(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask32{0xFFFFFFFFu}, BitCast(du, hi).raw)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> ConcatEven(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint32_t kIdx[16] = {0,  2,  4,  6,  8,  10, 12, 14,
                                             16, 18, 20, 22, 24, 26, 28, 30};
  return BitCast(d, Vec512<uint32_t>{_mm512_mask2_permutex2var_epi32(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask16{0xFFFF}, BitCast(du, hi).raw)});
}

HWY_API Vec512<float> ConcatEven(Full512<float> d, Vec512<float> hi,
                                 Vec512<float> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint32_t kIdx[16] = {0,  2,  4,  6,  8,  10, 12, 14,
                                             16, 18, 20, 22, 24, 26, 28, 30};
  return Vec512<float>{_mm512_mask2_permutex2var_ps(lo.raw, Load(du, kIdx).raw,
                                                    __mmask16{0xFFFF}, hi.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> ConcatEven(Full512<T> d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  return BitCast(d, Vec512<uint64_t>{_mm512_mask2_permutex2var_epi64(
                        BitCast(du, lo).raw, Load(du, kIdx).raw, __mmask8{0xFF},
                        BitCast(du, hi).raw)});
}

HWY_API Vec512<double> ConcatEven(Full512<double> d, Vec512<double> hi,
                                  Vec512<double> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  return Vec512<double>{_mm512_mask2_permutex2var_pd(lo.raw, Load(du, kIdx).raw,
                                                     __mmask8{0xFF}, hi.raw)};
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> DupEven(Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CCAA)};
}
HWY_API Vec512<float> DupEven(Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_CCAA)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> DupEven(const Vec512<T> v) {
  return InterleaveLower(Full512<T>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> DupOdd(Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_epi32(v.raw, _MM_PERM_DDBB)};
}
HWY_API Vec512<float> DupOdd(Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_DDBB)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> DupOdd(const Vec512<T> v) {
  return InterleaveUpper(Full512<T>(), v, v);
}

// ------------------------------ OddEven

template <typename T>
HWY_API Vec512<T> OddEven(const Vec512<T> a, const Vec512<T> b) {
  constexpr size_t s = sizeof(T);
  constexpr int shift = s == 1 ? 0 : s == 2 ? 32 : s == 4 ? 48 : 56;
  return IfThenElse(Mask512<T>{0x5555555555555555ull >> shift}, b, a);
}

// ------------------------------ OddEvenBlocks

template <typename T>
HWY_API Vec512<T> OddEvenBlocks(Vec512<T> odd, Vec512<T> even) {
  return Vec512<T>{_mm512_mask_blend_epi64(__mmask8{0x33u}, odd.raw, even.raw)};
}

HWY_API Vec512<float> OddEvenBlocks(Vec512<float> odd, Vec512<float> even) {
  return Vec512<float>{
      _mm512_mask_blend_ps(__mmask16{0x0F0Fu}, odd.raw, even.raw)};
}

HWY_API Vec512<double> OddEvenBlocks(Vec512<double> odd, Vec512<double> even) {
  return Vec512<double>{
      _mm512_mask_blend_pd(__mmask8{0x33u}, odd.raw, even.raw)};
}

// ------------------------------ SwapAdjacentBlocks

template <typename T>
HWY_API Vec512<T> SwapAdjacentBlocks(Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_i32x4(v.raw, v.raw, _MM_PERM_CDAB)};
}

HWY_API Vec512<float> SwapAdjacentBlocks(Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_f32x4(v.raw, v.raw, _MM_PERM_CDAB)};
}

HWY_API Vec512<double> SwapAdjacentBlocks(Vec512<double> v) {
  return Vec512<double>{_mm512_shuffle_f64x2(v.raw, v.raw, _MM_PERM_CDAB)};
}

// ------------------------------ ReverseBlocks

template <typename T>
HWY_API Vec512<T> ReverseBlocks(Full512<T> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_i32x4(v.raw, v.raw, _MM_PERM_ABCD)};
}
HWY_API Vec512<float> ReverseBlocks(Full512<float> /* tag */, Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_f32x4(v.raw, v.raw, _MM_PERM_ABCD)};
}
HWY_API Vec512<double> ReverseBlocks(Full512<double> /* tag */,
                                     Vec512<double> v) {
  return Vec512<double>{_mm512_shuffle_f64x2(v.raw, v.raw, _MM_PERM_ABCD)};
}

// ------------------------------ TableLookupBytes (ZeroExtendVector)

// Both full
template <typename T, typename TI>
HWY_API Vec512<TI> TableLookupBytes(Vec512<T> bytes, Vec512<TI> indices) {
  return Vec512<TI>{_mm512_shuffle_epi8(bytes.raw, indices.raw)};
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(Vec512<T> bytes, Vec128<TI, NI> from) {
  const Full512<TI> d512;
  const Half<decltype(d512)> d256;
  const Half<decltype(d256)> d128;
  // First expand to full 128, then 256, then 512.
  const Vec128<TI> from_full{from.raw};
  const auto from_512 =
      ZeroExtendVector(d512, ZeroExtendVector(d256, from_full));
  const auto tbl_full = TableLookupBytes(bytes, from_512);
  // Shrink to 256, then 128, then partial.
  return Vec128<TI, NI>{LowerHalf(d128, LowerHalf(d256, tbl_full)).raw};
}
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(Vec512<T> bytes, Vec256<TI> from) {
  const auto from_512 = ZeroExtendVector(Full512<TI>(), from);
  return LowerHalf(Full256<TI>(), TableLookupBytes(bytes, from_512));
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec512<TI> TableLookupBytes(Vec128<T, N> bytes, Vec512<TI> from) {
  const Full512<TI> d512;
  const Half<decltype(d512)> d256;
  const Half<decltype(d256)> d128;
  // First expand to full 128, then 256, then 512.
  const Vec128<T> bytes_full{bytes.raw};
  const auto bytes_512 =
      ZeroExtendVector(d512, ZeroExtendVector(d256, bytes_full));
  return TableLookupBytes(bytes_512, from);
}
template <typename T, typename TI>
HWY_API Vec512<TI> TableLookupBytes(Vec256<T> bytes, Vec512<TI> from) {
  const auto bytes_512 = ZeroExtendVector(Full512<T>(), bytes);
  return TableLookupBytes(bytes_512, from);
}

// Partial both are handled by x86_128/256.

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then Zip* would be faster.
HWY_API Vec512<uint16_t> PromoteTo(Full512<uint16_t> /* tag */,
                                   Vec256<uint8_t> v) {
  return Vec512<uint16_t>{_mm512_cvtepu8_epi16(v.raw)};
}
HWY_API Vec512<uint32_t> PromoteTo(Full512<uint32_t> /* tag */,
                                   Vec128<uint8_t> v) {
  return Vec512<uint32_t>{_mm512_cvtepu8_epi32(v.raw)};
}
HWY_API Vec512<int16_t> PromoteTo(Full512<int16_t> /* tag */,
                                  Vec256<uint8_t> v) {
  return Vec512<int16_t>{_mm512_cvtepu8_epi16(v.raw)};
}
HWY_API Vec512<int32_t> PromoteTo(Full512<int32_t> /* tag */,
                                  Vec128<uint8_t> v) {
  return Vec512<int32_t>{_mm512_cvtepu8_epi32(v.raw)};
}
HWY_API Vec512<uint32_t> PromoteTo(Full512<uint32_t> /* tag */,
                                   Vec256<uint16_t> v) {
  return Vec512<uint32_t>{_mm512_cvtepu16_epi32(v.raw)};
}
HWY_API Vec512<int32_t> PromoteTo(Full512<int32_t> /* tag */,
                                  Vec256<uint16_t> v) {
  return Vec512<int32_t>{_mm512_cvtepu16_epi32(v.raw)};
}
HWY_API Vec512<uint64_t> PromoteTo(Full512<uint64_t> /* tag */,
                                   Vec256<uint32_t> v) {
  return Vec512<uint64_t>{_mm512_cvtepu32_epi64(v.raw)};
}

// Signed: replicate sign bit.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then ZipUpper/lo followed by
// signed shift would be faster.
HWY_API Vec512<int16_t> PromoteTo(Full512<int16_t> /* tag */,
                                  Vec256<int8_t> v) {
  return Vec512<int16_t>{_mm512_cvtepi8_epi16(v.raw)};
}
HWY_API Vec512<int32_t> PromoteTo(Full512<int32_t> /* tag */,
                                  Vec128<int8_t> v) {
  return Vec512<int32_t>{_mm512_cvtepi8_epi32(v.raw)};
}
HWY_API Vec512<int32_t> PromoteTo(Full512<int32_t> /* tag */,
                                  Vec256<int16_t> v) {
  return Vec512<int32_t>{_mm512_cvtepi16_epi32(v.raw)};
}
HWY_API Vec512<int64_t> PromoteTo(Full512<int64_t> /* tag */,
                                  Vec256<int32_t> v) {
  return Vec512<int64_t>{_mm512_cvtepi32_epi64(v.raw)};
}

// Float
HWY_API Vec512<float> PromoteTo(Full512<float> /* tag */,
                                const Vec256<float16_t> v) {
  return Vec512<float>{_mm512_cvtph_ps(v.raw)};
}

HWY_API Vec512<float> PromoteTo(Full512<float> df32,
                                const Vec256<bfloat16_t> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

HWY_API Vec512<double> PromoteTo(Full512<double> /* tag */, Vec256<float> v) {
  return Vec512<double>{_mm512_cvtps_pd(v.raw)};
}

HWY_API Vec512<double> PromoteTo(Full512<double> /* tag */, Vec256<int32_t> v) {
  return Vec512<double>{_mm512_cvtepi32_pd(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

HWY_API Vec256<uint16_t> DemoteTo(Full256<uint16_t> /* tag */,
                                  const Vec512<int32_t> v) {
  const Vec512<uint16_t> u16{_mm512_packus_epi32(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(Full512<uint64_t>(), kLanes);
  const Vec512<uint16_t> even{_mm512_permutexvar_epi64(idx64.raw, u16.raw)};
  return LowerHalf(even);
}

HWY_API Vec256<int16_t> DemoteTo(Full256<int16_t> /* tag */,
                                 const Vec512<int32_t> v) {
  const Vec512<int16_t> i16{_mm512_packs_epi32(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(Full512<uint64_t>(), kLanes);
  const Vec512<int16_t> even{_mm512_permutexvar_epi64(idx64.raw, i16.raw)};
  return LowerHalf(even);
}

HWY_API Vec128<uint8_t, 16> DemoteTo(Full128<uint8_t> /* tag */,
                                     const Vec512<int32_t> v) {
  const Vec512<uint16_t> u16{_mm512_packus_epi32(v.raw, v.raw)};
  // packus treats the input as signed; we want unsigned. Clear the MSB to get
  // unsigned saturation to u8.
  const Vec512<int16_t> i16{
      _mm512_and_si512(u16.raw, _mm512_set1_epi16(0x7FFF))};
  const Vec512<uint8_t> u8{_mm512_packus_epi16(i16.raw, i16.raw)};

  alignas(16) static constexpr uint32_t kLanes[4] = {0, 4, 8, 12};
  const auto idx32 = LoadDup128(Full512<uint32_t>(), kLanes);
  const Vec512<uint8_t> fixed{_mm512_permutexvar_epi32(idx32.raw, u8.raw)};
  return LowerHalf(LowerHalf(fixed));
}

HWY_API Vec256<uint8_t> DemoteTo(Full256<uint8_t> /* tag */,
                                 const Vec512<int16_t> v) {
  const Vec512<uint8_t> u8{_mm512_packus_epi16(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(Full512<uint64_t>(), kLanes);
  const Vec512<uint8_t> even{_mm512_permutexvar_epi64(idx64.raw, u8.raw)};
  return LowerHalf(even);
}

HWY_API Vec128<int8_t, 16> DemoteTo(Full128<int8_t> /* tag */,
                                    const Vec512<int32_t> v) {
  const Vec512<int16_t> i16{_mm512_packs_epi32(v.raw, v.raw)};
  const Vec512<int8_t> i8{_mm512_packs_epi16(i16.raw, i16.raw)};

  alignas(16) static constexpr uint32_t kLanes[16] = {0, 4, 8, 12, 0, 4, 8, 12,
                                                      0, 4, 8, 12, 0, 4, 8, 12};
  const auto idx32 = LoadDup128(Full512<uint32_t>(), kLanes);
  const Vec512<int8_t> fixed{_mm512_permutexvar_epi32(idx32.raw, i8.raw)};
  return LowerHalf(LowerHalf(fixed));
}

HWY_API Vec256<int8_t> DemoteTo(Full256<int8_t> /* tag */,
                                const Vec512<int16_t> v) {
  const Vec512<int8_t> u8{_mm512_packs_epi16(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(Full512<uint64_t>(), kLanes);
  const Vec512<int8_t> even{_mm512_permutexvar_epi64(idx64.raw, u8.raw)};
  return LowerHalf(even);
}

HWY_API Vec256<float16_t> DemoteTo(Full256<float16_t> /* tag */,
                                   const Vec512<float> v) {
  // Work around warnings in the intrinsic definitions (passing -1 as a mask).
  HWY_DIAGNOSTICS(push)
  HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")
  return Vec256<float16_t>{_mm512_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
  HWY_DIAGNOSTICS(pop)
}

HWY_API Vec256<bfloat16_t> DemoteTo(Full256<bfloat16_t> dbf16,
                                    const Vec512<float> v) {
  // TODO(janwas): _mm512_cvtneps_pbh once we have avx512bf16.
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

HWY_API Vec512<bfloat16_t> ReorderDemote2To(Full512<bfloat16_t> dbf16,
                                            Vec512<float> a, Vec512<float> b) {
  // TODO(janwas): _mm512_cvtne2ps_pbh once we have avx512bf16.
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec512<uint32_t> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

HWY_API Vec256<float> DemoteTo(Full256<float> /* tag */,
                               const Vec512<double> v) {
  return Vec256<float>{_mm512_cvtpd_ps(v.raw)};
}

HWY_API Vec256<int32_t> DemoteTo(Full256<int32_t> /* tag */,
                                 const Vec512<double> v) {
  const auto clamped = detail::ClampF64ToI32Max(Full512<double>(), v);
  return Vec256<int32_t>{_mm512_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
HWY_API Vec128<uint8_t, 16> U8FromU32(const Vec512<uint32_t> v) {
  const Full512<uint32_t> d32;
  // In each 128 bit block, gather the lower byte of 4 uint32_t lanes into the
  // lowest 4 bytes.
  alignas(16) static constexpr uint32_t k8From32[4] = {0x0C080400u, ~0u, ~0u,
                                                       ~0u};
  const auto quads = TableLookupBytes(v, LoadDup128(d32, k8From32));
  // Gather the lowest 4 bytes of 4 128-bit blocks.
  alignas(16) static constexpr uint32_t kIndex32[4] = {0, 4, 8, 12};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(LoadDup128(d32, kIndex32).raw, quads.raw)};
  return LowerHalf(LowerHalf(bytes));
}

// ------------------------------ Truncations

HWY_API Vec128<uint8_t, 8> TruncateTo(Simd<uint8_t, 8, 0> d,
                                      const Vec512<uint64_t> v) {
#if HWY_TARGET == HWY_AVX3_DL
  (void)d;
  const Full512<uint8_t> d8;
  alignas(16) static constexpr uint8_t k8From64[16] = {
    0, 8, 16, 24, 32, 40, 48, 56, 0, 8, 16, 24, 32, 40, 48, 56};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi8(LoadDup128(d8, k8From64).raw, v.raw)};
  return LowerHalf(LowerHalf(LowerHalf(bytes)));
#else
  const Full512<uint32_t> d32;
  alignas(64) constexpr uint32_t kEven[16] = {0, 2, 4, 6, 8, 10, 12, 14,
                                              0, 2, 4, 6, 8, 10, 12, 14};
  const Vec512<uint32_t> even{
      _mm512_permutexvar_epi32(Load(d32, kEven).raw, v.raw)};
  return TruncateTo(d, LowerHalf(even));
#endif
}

HWY_API Vec128<uint16_t, 8> TruncateTo(Simd<uint16_t, 8, 0> /* tag */,
                                       const Vec512<uint64_t> v) {
  const Full512<uint16_t> d16;
  alignas(16) static constexpr uint16_t k16From64[8] = {
      0, 4, 8, 12, 16, 20, 24, 28};
  const Vec512<uint16_t> bytes{
      _mm512_permutexvar_epi16(LoadDup128(d16, k16From64).raw, v.raw)};
  return LowerHalf(LowerHalf(bytes));
}

HWY_API Vec256<uint32_t> TruncateTo(Simd<uint32_t, 8, 0> /* tag */,
                                    const Vec512<uint64_t> v) {
  const Full512<uint32_t> d32;
  alignas(64) constexpr uint32_t kEven[16] = {0, 2, 4, 6, 8, 10, 12, 14,
                                              0, 2, 4, 6, 8, 10, 12, 14};
  const Vec512<uint32_t> even{
      _mm512_permutexvar_epi32(Load(d32, kEven).raw, v.raw)};
  return LowerHalf(even);
}

HWY_API Vec128<uint8_t, 16> TruncateTo(Simd<uint8_t, 16, 0> /* tag */,
                                       const Vec512<uint32_t> v) {
#if HWY_TARGET == HWY_AVX3_DL
  const Full512<uint8_t> d8;
  alignas(16) static constexpr uint8_t k8From32[16] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(LoadDup128(d8, k8From32).raw, v.raw)};
#else
  const Full512<uint32_t> d32;
  // In each 128 bit block, gather the lower byte of 4 uint32_t lanes into the
  // lowest 4 bytes.
  alignas(16) static constexpr uint32_t k8From32[4] = {0x0C080400u, ~0u, ~0u,
                                                       ~0u};
  const auto quads = TableLookupBytes(v, LoadDup128(d32, k8From32));
  // Gather the lowest 4 bytes of 4 128-bit blocks.
  alignas(16) static constexpr uint32_t kIndex32[4] = {0, 4, 8, 12};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(LoadDup128(d32, kIndex32).raw, quads.raw)};
#endif
  return LowerHalf(LowerHalf(bytes));
}

HWY_API Vec256<uint16_t> TruncateTo(Simd<uint16_t, 16, 0> /* tag */,
                                    const Vec512<uint32_t> v) {
  const Full512<uint16_t> d16;
  alignas(64) static constexpr uint16_t k16From32[32] = {
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
  const Vec512<uint16_t> bytes{
      _mm512_permutexvar_epi16(Load(d16, k16From32).raw, v.raw)};
  return LowerHalf(bytes);
}

HWY_API Vec256<uint8_t> TruncateTo(Simd<uint8_t, 32, 0> /* tag */,
                                   const Vec512<uint16_t> v) {
#if HWY_TARGET == HWY_AVX3_DL
  const Full512<uint8_t> d8;
  alignas(64) static constexpr uint8_t k8From16[64] = {
     0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
    32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
     0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
    32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi8(Load(d8, k8From16).raw, v.raw)};
#else
  const Full512<uint32_t> d32;
  alignas(16) static constexpr uint32_t k16From32[4] = {
      0x06040200u, 0x0E0C0A08u, 0x06040200u, 0x0E0C0A08u};
  const auto quads = TableLookupBytes(v, LoadDup128(d32, k16From32));
  alignas(64) static constexpr uint32_t kIndex32[16] = {
      0, 1, 4, 5, 8, 9, 12, 13, 0, 1, 4, 5, 8, 9, 12, 13};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(Load(d32, kIndex32).raw, quads.raw)};
#endif
  return LowerHalf(bytes);
}

// ------------------------------ Convert integer <=> floating point

HWY_API Vec512<float> ConvertTo(Full512<float> /* tag */,
                                const Vec512<int32_t> v) {
  return Vec512<float>{_mm512_cvtepi32_ps(v.raw)};
}

HWY_API Vec512<double> ConvertTo(Full512<double> /* tag */,
                                 const Vec512<int64_t> v) {
  return Vec512<double>{_mm512_cvtepi64_pd(v.raw)};
}

// Truncates (rounds toward zero).
HWY_API Vec512<int32_t> ConvertTo(Full512<int32_t> d, const Vec512<float> v) {
  return detail::FixConversionOverflow(d, v, _mm512_cvttps_epi32(v.raw));
}
HWY_API Vec512<int64_t> ConvertTo(Full512<int64_t> di, const Vec512<double> v) {
  return detail::FixConversionOverflow(di, v, _mm512_cvttpd_epi64(v.raw));
}

HWY_API Vec512<int32_t> NearestInt(const Vec512<float> v) {
  const Full512<int32_t> di;
  return detail::FixConversionOverflow(di, v, _mm512_cvtps_epi32(v.raw));
}

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec512<uint8_t> AESRound(Vec512<uint8_t> state,
                                 Vec512<uint8_t> round_key) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec512<uint8_t>{_mm512_aesenc_epi128(state.raw, round_key.raw)};
#else
  const Full512<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, AESRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec512<uint8_t> AESLastRound(Vec512<uint8_t> state,
                                     Vec512<uint8_t> round_key) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec512<uint8_t>{_mm512_aesenclast_epi128(state.raw, round_key.raw)};
#else
  const Full512<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(d,
                 AESLastRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESLastRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec512<uint64_t> CLMulLower(Vec512<uint64_t> va, Vec512<uint64_t> vb) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec512<uint64_t>{_mm512_clmulepi64_epi128(va.raw, vb.raw, 0x00)};
#else
  alignas(64) uint64_t a[8];
  alignas(64) uint64_t b[8];
  const Full512<uint64_t> d;
  const Full128<uint64_t> d128;
  Store(va, d, a);
  Store(vb, d, b);
  for (size_t i = 0; i < 8; i += 2) {
    const auto mul = CLMulLower(Load(d128, a + i), Load(d128, b + i));
    Store(mul, d128, a + i);
  }
  return Load(d, a);
#endif
}

HWY_API Vec512<uint64_t> CLMulUpper(Vec512<uint64_t> va, Vec512<uint64_t> vb) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec512<uint64_t>{_mm512_clmulepi64_epi128(va.raw, vb.raw, 0x11)};
#else
  alignas(64) uint64_t a[8];
  alignas(64) uint64_t b[8];
  const Full512<uint64_t> d;
  const Full128<uint64_t> d128;
  Store(va, d, a);
  Store(vb, d, b);
  for (size_t i = 0; i < 8; i += 2) {
    const auto mul = CLMulUpper(Load(d128, a + i), Load(d128, b + i));
    Store(mul, d128, a + i);
  }
  return Load(d, a);
#endif
}

#endif  // HWY_DISABLE_PCLMUL_AES

// ================================================== MISC

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, typename T2>
Vec512<T> Iota(const Full512<T> d, const T2 first) {
  HWY_ALIGN T lanes[64 / sizeof(T)];
  for (size_t i = 0; i < 64 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ------------------------------ Mask testing

// Beware: the suffix indicates the number of mask bits, not lane size!

namespace detail {

template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<1> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask64_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<2> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<4> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<8> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}

}  // namespace detail

template <typename T>
HWY_API bool AllFalse(const Full512<T> /* tag */, const Mask512<T> mask) {
  return detail::AllFalse(hwy::SizeTag<sizeof(T)>(), mask);
}

namespace detail {

template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<1> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask64_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFFFFFFFFFFFFFull;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<2> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFFFFFull;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<4> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFull;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<8> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFull;
#endif
}

}  // namespace detail

template <typename T>
HWY_API bool AllTrue(const Full512<T> /* tag */, const Mask512<T> mask) {
  return detail::AllTrue(hwy::SizeTag<sizeof(T)>(), mask);
}

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T>
HWY_API Mask512<T> LoadMaskBits(const Full512<T> /* tag */,
                                const uint8_t* HWY_RESTRICT bits) {
  Mask512<T> mask;
  CopyBytes<8 / sizeof(T)>(bits, &mask.raw);
  // N >= 8 (= 512 / 64), so no need to mask invalid bits.
  return mask;
}

// `p` points to at least 8 writable bytes.
template <typename T>
HWY_API size_t StoreMaskBits(const Full512<T> /* tag */, const Mask512<T> mask,
                             uint8_t* bits) {
  const size_t kNumBytes = 8 / sizeof(T);
  CopyBytes<kNumBytes>(&mask.raw, bits);
  // N >= 8 (= 512 / 64), so no need to mask invalid bits.
  return kNumBytes;
}

template <typename T>
HWY_API size_t CountTrue(const Full512<T> /* tag */, const Mask512<T> mask) {
  return PopCount(static_cast<uint64_t>(mask.raw));
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_API intptr_t FindFirstTrue(const Full512<T> /* tag */,
                               const Mask512<T> mask) {
  return mask.raw ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask.raw)) : -1;
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API intptr_t FindFirstTrue(const Full512<T> /* tag */,
                               const Mask512<T> mask) {
  return mask.raw ? intptr_t(Num0BitsBelowLS1Bit_Nonzero64(mask.raw)) : -1;
}

// ------------------------------ Compress

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec512<T> Compress(Vec512<T> v, Mask512<T> mask) {
  return Vec512<T>{_mm512_maskz_compress_epi32(mask.raw, v.raw)};
}

HWY_API Vec512<float> Compress(Vec512<float> v, Mask512<float> mask) {
  return Vec512<float>{_mm512_maskz_compress_ps(mask.raw, v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> Compress(Vec512<T> v, Mask512<T> mask) {
  // See CompressIsPartition. u64 is faster than u32.
  alignas(16) constexpr uint64_t packed_array[256] = {
      // PrintCompress32x8Tables
      0x76543210, 0x76543210, 0x76543201, 0x76543210, 0x76543102, 0x76543120,
      0x76543021, 0x76543210, 0x76542103, 0x76542130, 0x76542031, 0x76542310,
      0x76541032, 0x76541320, 0x76540321, 0x76543210, 0x76532104, 0x76532140,
      0x76532041, 0x76532410, 0x76531042, 0x76531420, 0x76530421, 0x76534210,
      0x76521043, 0x76521430, 0x76520431, 0x76524310, 0x76510432, 0x76514320,
      0x76504321, 0x76543210, 0x76432105, 0x76432150, 0x76432051, 0x76432510,
      0x76431052, 0x76431520, 0x76430521, 0x76435210, 0x76421053, 0x76421530,
      0x76420531, 0x76425310, 0x76410532, 0x76415320, 0x76405321, 0x76453210,
      0x76321054, 0x76321540, 0x76320541, 0x76325410, 0x76310542, 0x76315420,
      0x76305421, 0x76354210, 0x76210543, 0x76215430, 0x76205431, 0x76254310,
      0x76105432, 0x76154320, 0x76054321, 0x76543210, 0x75432106, 0x75432160,
      0x75432061, 0x75432610, 0x75431062, 0x75431620, 0x75430621, 0x75436210,
      0x75421063, 0x75421630, 0x75420631, 0x75426310, 0x75410632, 0x75416320,
      0x75406321, 0x75463210, 0x75321064, 0x75321640, 0x75320641, 0x75326410,
      0x75310642, 0x75316420, 0x75306421, 0x75364210, 0x75210643, 0x75216430,
      0x75206431, 0x75264310, 0x75106432, 0x75164320, 0x75064321, 0x75643210,
      0x74321065, 0x74321650, 0x74320651, 0x74326510, 0x74310652, 0x74316520,
      0x74306521, 0x74365210, 0x74210653, 0x74216530, 0x74206531, 0x74265310,
      0x74106532, 0x74165320, 0x74065321, 0x74653210, 0x73210654, 0x73216540,
      0x73206541, 0x73265410, 0x73106542, 0x73165420, 0x73065421, 0x73654210,
      0x72106543, 0x72165430, 0x72065431, 0x72654310, 0x71065432, 0x71654320,
      0x70654321, 0x76543210, 0x65432107, 0x65432170, 0x65432071, 0x65432710,
      0x65431072, 0x65431720, 0x65430721, 0x65437210, 0x65421073, 0x65421730,
      0x65420731, 0x65427310, 0x65410732, 0x65417320, 0x65407321, 0x65473210,
      0x65321074, 0x65321740, 0x65320741, 0x65327410, 0x65310742, 0x65317420,
      0x65307421, 0x65374210, 0x65210743, 0x65217430, 0x65207431, 0x65274310,
      0x65107432, 0x65174320, 0x65074321, 0x65743210, 0x64321075, 0x64321750,
      0x64320751, 0x64327510, 0x64310752, 0x64317520, 0x64307521, 0x64375210,
      0x64210753, 0x64217530, 0x64207531, 0x64275310, 0x64107532, 0x64175320,
      0x64075321, 0x64753210, 0x63210754, 0x63217540, 0x63207541, 0x63275410,
      0x63107542, 0x63175420, 0x63075421, 0x63754210, 0x62107543, 0x62175430,
      0x62075431, 0x62754310, 0x61075432, 0x61754320, 0x60754321, 0x67543210,
      0x54321076, 0x54321760, 0x54320761, 0x54327610, 0x54310762, 0x54317620,
      0x54307621, 0x54376210, 0x54210763, 0x54217630, 0x54207631, 0x54276310,
      0x54107632, 0x54176320, 0x54076321, 0x54763210, 0x53210764, 0x53217640,
      0x53207641, 0x53276410, 0x53107642, 0x53176420, 0x53076421, 0x53764210,
      0x52107643, 0x52176430, 0x52076431, 0x52764310, 0x51076432, 0x51764320,
      0x50764321, 0x57643210, 0x43210765, 0x43217650, 0x43207651, 0x43276510,
      0x43107652, 0x43176520, 0x43076521, 0x43765210, 0x42107653, 0x42176530,
      0x42076531, 0x42765310, 0x41076532, 0x41765320, 0x40765321, 0x47653210,
      0x32107654, 0x32176540, 0x32076541, 0x32765410, 0x31076542, 0x31765420,
      0x30765421, 0x37654210, 0x21076543, 0x21765430, 0x20765431, 0x27654310,
      0x10765432, 0x17654320, 0x07654321, 0x76543210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 3) -
  // _mm512_permutexvar_epi64 will ignore the upper bits.
  const Full512<T> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(64) constexpr uint64_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  const auto indices = Indices512<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

// 16-bit may use the 32-bit Compress and must be defined after it.
//
// Ignore IDE redefinition error - this is not actually defined in x86_256 if
// we are including x86_512-inl.h.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> mask) {
  const Full256<T> d;
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  const Vec256<uint16_t> cu{_mm256_maskz_compress_epi16(mask.raw, vu.raw)};
#else
  // Promote to i32 (512-bit vector!) so we can use the native Compress.
  const auto vw = PromoteTo(Rebind<int32_t, decltype(d)>(), vu);
  const Mask512<int32_t> mask32{static_cast<__mmask16>(mask.raw)};
  const auto cu = DemoteTo(du, Compress(vw, mask32));
#endif  // HWY_TARGET == HWY_AVX3_DL

  return BitCast(d, cu);
}

// Expands to 32-bit, compresses, concatenate demoted halves.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> Compress(Vec512<T> v, const Mask512<T> mask) {
  const Full512<T> d;
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  const Vec512<uint16_t> cu{_mm512_maskz_compress_epi16(mask.raw, vu.raw)};
#else
  const Repartition<int32_t, decltype(d)> dw;
  const Half<decltype(du)> duh;
  const auto promoted0 = PromoteTo(dw, LowerHalf(duh, vu));
  const auto promoted1 = PromoteTo(dw, UpperHalf(duh, vu));

  const uint32_t mask_bits{mask.raw};
  const Mask512<int32_t> mask0{static_cast<__mmask16>(mask_bits & 0xFFFF)};
  const Mask512<int32_t> mask1{static_cast<__mmask16>(mask_bits >> 16)};
  const auto compressed0 = Compress(promoted0, mask0);
  const auto compressed1 = Compress(promoted1, mask1);

  const auto demoted0 = ZeroExtendVector(du, DemoteTo(duh, compressed0));
  const auto demoted1 = ZeroExtendVector(du, DemoteTo(duh, compressed1));

  // Concatenate into single vector by shifting upper with writemask.
  const size_t num0 = CountTrue(dw, mask0);
  const __mmask32 m_upper = ~((1u << num0) - 1);
  alignas(64) uint16_t iota[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const auto idx = LoadU(du, iota + 32 - num0);
  const Vec512<uint16_t> cu{_mm512_mask_permutexvar_epi16(
      demoted0.raw, m_upper, idx.raw, demoted1.raw)};
#endif  // HWY_TARGET == HWY_AVX3_DL

  return BitCast(d, cu);
}

// ------------------------------ CompressNot

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec512<T> CompressNot(Vec512<T> v, const Mask512<T> mask) {
  return Compress(v, Not(mask));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec512<T> CompressNot(Vec512<T> v, Mask512<T> mask) {
  // See CompressIsPartition. u64 is faster than u32.
  alignas(16) constexpr uint64_t packed_array[256] = {
      // PrintCompressNot32x8Tables
      0x76543210, 0x07654321, 0x17654320, 0x10765432, 0x27654310, 0x20765431,
      0x21765430, 0x21076543, 0x37654210, 0x30765421, 0x31765420, 0x31076542,
      0x32765410, 0x32076541, 0x32176540, 0x32107654, 0x47653210, 0x40765321,
      0x41765320, 0x41076532, 0x42765310, 0x42076531, 0x42176530, 0x42107653,
      0x43765210, 0x43076521, 0x43176520, 0x43107652, 0x43276510, 0x43207651,
      0x43217650, 0x43210765, 0x57643210, 0x50764321, 0x51764320, 0x51076432,
      0x52764310, 0x52076431, 0x52176430, 0x52107643, 0x53764210, 0x53076421,
      0x53176420, 0x53107642, 0x53276410, 0x53207641, 0x53217640, 0x53210764,
      0x54763210, 0x54076321, 0x54176320, 0x54107632, 0x54276310, 0x54207631,
      0x54217630, 0x54210763, 0x54376210, 0x54307621, 0x54317620, 0x54310762,
      0x54327610, 0x54320761, 0x54321760, 0x54321076, 0x67543210, 0x60754321,
      0x61754320, 0x61075432, 0x62754310, 0x62075431, 0x62175430, 0x62107543,
      0x63754210, 0x63075421, 0x63175420, 0x63107542, 0x63275410, 0x63207541,
      0x63217540, 0x63210754, 0x64753210, 0x64075321, 0x64175320, 0x64107532,
      0x64275310, 0x64207531, 0x64217530, 0x64210753, 0x64375210, 0x64307521,
      0x64317520, 0x64310752, 0x64327510, 0x64320751, 0x64321750, 0x64321075,
      0x65743210, 0x65074321, 0x65174320, 0x65107432, 0x65274310, 0x65207431,
      0x65217430, 0x65210743, 0x65374210, 0x65307421, 0x65317420, 0x65310742,
      0x65327410, 0x65320741, 0x65321740, 0x65321074, 0x65473210, 0x65407321,
      0x65417320, 0x65410732, 0x65427310, 0x65420731, 0x65421730, 0x65421073,
      0x65437210, 0x65430721, 0x65431720, 0x65431072, 0x65432710, 0x65432071,
      0x65432170, 0x65432107, 0x76543210, 0x70654321, 0x71654320, 0x71065432,
      0x72654310, 0x72065431, 0x72165430, 0x72106543, 0x73654210, 0x73065421,
      0x73165420, 0x73106542, 0x73265410, 0x73206541, 0x73216540, 0x73210654,
      0x74653210, 0x74065321, 0x74165320, 0x74106532, 0x74265310, 0x74206531,
      0x74216530, 0x74210653, 0x74365210, 0x74306521, 0x74316520, 0x74310652,
      0x74326510, 0x74320651, 0x74321650, 0x74321065, 0x75643210, 0x75064321,
      0x75164320, 0x75106432, 0x75264310, 0x75206431, 0x75216430, 0x75210643,
      0x75364210, 0x75306421, 0x75316420, 0x75310642, 0x75326410, 0x75320641,
      0x75321640, 0x75321064, 0x75463210, 0x75406321, 0x75416320, 0x75410632,
      0x75426310, 0x75420631, 0x75421630, 0x75421063, 0x75436210, 0x75430621,
      0x75431620, 0x75431062, 0x75432610, 0x75432061, 0x75432160, 0x75432106,
      0x76543210, 0x76054321, 0x76154320, 0x76105432, 0x76254310, 0x76205431,
      0x76215430, 0x76210543, 0x76354210, 0x76305421, 0x76315420, 0x76310542,
      0x76325410, 0x76320541, 0x76321540, 0x76321054, 0x76453210, 0x76405321,
      0x76415320, 0x76410532, 0x76425310, 0x76420531, 0x76421530, 0x76421053,
      0x76435210, 0x76430521, 0x76431520, 0x76431052, 0x76432510, 0x76432051,
      0x76432150, 0x76432105, 0x76543210, 0x76504321, 0x76514320, 0x76510432,
      0x76524310, 0x76520431, 0x76521430, 0x76521043, 0x76534210, 0x76530421,
      0x76531420, 0x76531042, 0x76532410, 0x76532041, 0x76532140, 0x76532104,
      0x76543210, 0x76540321, 0x76541320, 0x76541032, 0x76542310, 0x76542031,
      0x76542130, 0x76542103, 0x76543210, 0x76543021, 0x76543120, 0x76543102,
      0x76543210, 0x76543201, 0x76543210, 0x76543210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 3) -
  // _mm512_permutexvar_epi64 will ignore the upper bits.
  const Full512<T> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(64) constexpr uint64_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  const auto indices = Indices512<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

HWY_API Vec512<uint64_t> CompressBlocksNot(Vec512<uint64_t> v,
                                           Mask512<uint64_t> mask) {
  return CompressNot(v, mask);
}

// ------------------------------ CompressBits
template <typename T>
HWY_API Vec512<T> CompressBits(Vec512<T> v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(Full512<T>(), bits));
}

// ------------------------------ CompressStore

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CompressStore(Vec512<T> v, Mask512<T> mask, Full512<T> d,
                             T* HWY_RESTRICT unaligned) {
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

  const uint64_t mask_bits{mask.raw};

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  _mm512_mask_compressstoreu_epi16(unaligned, mask.raw, vu.raw);
#else
  const Repartition<int32_t, decltype(d)> dw;
  const Half<decltype(du)> duh;
  const auto promoted0 = PromoteTo(dw, LowerHalf(duh, vu));
  const auto promoted1 = PromoteTo(dw, UpperHalf(duh, vu));

  const uint64_t maskL = mask_bits & 0xFFFF;
  const uint64_t maskH = mask_bits >> 16;
  const Mask512<int32_t> mask0{static_cast<__mmask16>(maskL)};
  const Mask512<int32_t> mask1{static_cast<__mmask16>(maskH)};
  const auto compressed0 = Compress(promoted0, mask0);
  const auto compressed1 = Compress(promoted1, mask1);

  const Half<decltype(d)> dh;
  const auto demoted0 = BitCast(dh, DemoteTo(duh, compressed0));
  const auto demoted1 = BitCast(dh, DemoteTo(duh, compressed1));

  // Store 256-bit halves
  StoreU(demoted0, dh, unaligned);
  StoreU(demoted1, dh, unaligned + PopCount(maskL));
#endif

  return PopCount(mask_bits);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API size_t CompressStore(Vec512<T> v, Mask512<T> mask, Full512<T> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw});
// Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API size_t CompressStore(Vec512<T> v, Mask512<T> mask, Full512<T> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw});
// Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

HWY_API size_t CompressStore(Vec512<float> v, Mask512<float> mask,
                             Full512<float> /* tag */,
                             float* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw});
// Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(float));
#endif
  return count;
}

HWY_API size_t CompressStore(Vec512<double> v, Mask512<double> mask,
                             Full512<double> /* tag */,
                             double* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw});
// Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(double));
#endif
  return count;
}

// ------------------------------ CompressBlendedStore
template <typename T>
HWY_API size_t CompressBlendedStore(Vec512<T> v, Mask512<T> m, Full512<T> d,
                                    T* HWY_RESTRICT unaligned) {
  // AVX-512 already does the blending at no extra cost (latency 11,
  // rthroughput 2 - same as compress plus store).
  if (HWY_TARGET == HWY_AVX3_DL || sizeof(T) != 2) {
    return CompressStore(v, m, d, unaligned);
  } else {
    const size_t count = CountTrue(d, m);
    BlendedStore(Compress(v, m), FirstN(d, count), d, unaligned);
// Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
    __msan_unpoison(unaligned, count * sizeof(T));
#endif
    return count;
  }
}

// ------------------------------ CompressBitsStore
template <typename T>
HWY_API size_t CompressBitsStore(Vec512<T> v, const uint8_t* HWY_RESTRICT bits,
                                 Full512<T> d, T* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

// ------------------------------ LoadInterleaved4

// Actually implemented in generic_ops, we just overload LoadTransposedBlocks4.
namespace detail {

// Type-safe wrapper.
template <_MM_PERM_ENUM kPerm, typename T>
Vec512<T> Shuffle128(const Vec512<T> lo, const Vec512<T> hi) {
  return Vec512<T>{_mm512_shuffle_i64x2(lo.raw, hi.raw, kPerm)};
}
template <_MM_PERM_ENUM kPerm>
Vec512<float> Shuffle128(const Vec512<float> lo, const Vec512<float> hi) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, kPerm)};
}
template <_MM_PERM_ENUM kPerm>
Vec512<double> Shuffle128(const Vec512<double> lo, const Vec512<double> hi) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, kPerm)};
}

// Input (128-bit blocks):
// 3 2 1 0 (<- first block in unaligned)
// 7 6 5 4
// b a 9 8
// Output:
// 9 6 3 0 (LSB of A)
// a 7 4 1
// b 8 5 2
template <typename T>
HWY_API void LoadTransposedBlocks3(Full512<T> d,
                                   const T* HWY_RESTRICT unaligned,
                                   Vec512<T>& A, Vec512<T>& B, Vec512<T>& C) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> v3210 = LoadU(d, unaligned + 0 * N);
  const Vec512<T> v7654 = LoadU(d, unaligned + 1 * N);
  const Vec512<T> vba98 = LoadU(d, unaligned + 2 * N);

  const Vec512<T> v5421 = detail::Shuffle128<_MM_PERM_BACB>(v3210, v7654);
  const Vec512<T> va976 = detail::Shuffle128<_MM_PERM_CBDC>(v7654, vba98);

  A = detail::Shuffle128<_MM_PERM_CADA>(v3210, va976);
  B = detail::Shuffle128<_MM_PERM_DBCA>(v5421, va976);
  C = detail::Shuffle128<_MM_PERM_DADB>(v5421, vba98);
}

// Input (128-bit blocks):
// 3 2 1 0 (<- first block in unaligned)
// 7 6 5 4
// b a 9 8
// f e d c
// Output:
// c 8 4 0 (LSB of A)
// d 9 5 1
// e a 6 2
// f b 7 3
template <typename T>
HWY_API void LoadTransposedBlocks4(Full512<T> d,
                                   const T* HWY_RESTRICT unaligned,
                                   Vec512<T>& A, Vec512<T>& B, Vec512<T>& C,
                                   Vec512<T>& D) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> v3210 = LoadU(d, unaligned + 0 * N);
  const Vec512<T> v7654 = LoadU(d, unaligned + 1 * N);
  const Vec512<T> vba98 = LoadU(d, unaligned + 2 * N);
  const Vec512<T> vfedc = LoadU(d, unaligned + 3 * N);

  const Vec512<T> v5410 = detail::Shuffle128<_MM_PERM_BABA>(v3210, v7654);
  const Vec512<T> vdc98 = detail::Shuffle128<_MM_PERM_BABA>(vba98, vfedc);
  const Vec512<T> v7632 = detail::Shuffle128<_MM_PERM_DCDC>(v3210, v7654);
  const Vec512<T> vfeba = detail::Shuffle128<_MM_PERM_DCDC>(vba98, vfedc);
  A = detail::Shuffle128<_MM_PERM_CACA>(v5410, vdc98);
  B = detail::Shuffle128<_MM_PERM_DBDB>(v5410, vdc98);
  C = detail::Shuffle128<_MM_PERM_CACA>(v7632, vfeba);
  D = detail::Shuffle128<_MM_PERM_DBDB>(v7632, vfeba);
}

}  // namespace detail

// ------------------------------ StoreInterleaved2

// Implemented in generic_ops, we just overload StoreTransposedBlocks2/3/4.

namespace detail {

// Input (128-bit blocks):
// 6 4 2 0 (LSB of i)
// 7 5 3 1
// Output:
// 3 2 1 0
// 7 6 5 4
template <typename T>
HWY_API void StoreTransposedBlocks2(const Vec512<T> i, const Vec512<T> j,
                                    const Full512<T> d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 64 / sizeof(T);
  const auto j1_j0_i1_i0 = detail::Shuffle128<_MM_PERM_BABA>(i, j);
  const auto j3_j2_i3_i2 = detail::Shuffle128<_MM_PERM_DCDC>(i, j);
  const auto j1_i1_j0_i0 =
      detail::Shuffle128<_MM_PERM_DBCA>(j1_j0_i1_i0, j1_j0_i1_i0);
  const auto j3_i3_j2_i2 =
      detail::Shuffle128<_MM_PERM_DBCA>(j3_j2_i3_i2, j3_j2_i3_i2);
  StoreU(j1_i1_j0_i0, d, unaligned + 0 * N);
  StoreU(j3_i3_j2_i2, d, unaligned + 1 * N);
}

// Input (128-bit blocks):
// 9 6 3 0 (LSB of i)
// a 7 4 1
// b 8 5 2
// Output:
// 3 2 1 0
// 7 6 5 4
// b a 9 8
template <typename T>
HWY_API void StoreTransposedBlocks3(const Vec512<T> i, const Vec512<T> j,
                                    const Vec512<T> k, Full512<T> d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> j2_j0_i2_i0 = detail::Shuffle128<_MM_PERM_CACA>(i, j);
  const Vec512<T> i3_i1_k2_k0 = detail::Shuffle128<_MM_PERM_DBCA>(k, i);
  const Vec512<T> j3_j1_k3_k1 = detail::Shuffle128<_MM_PERM_DBDB>(k, j);

  const Vec512<T> out0 =  // i1 k0 j0 i0
      detail::Shuffle128<_MM_PERM_CACA>(j2_j0_i2_i0, i3_i1_k2_k0);
  const Vec512<T> out1 =  // j2 i2 k1 j1
      detail::Shuffle128<_MM_PERM_DBAC>(j3_j1_k3_k1, j2_j0_i2_i0);
  const Vec512<T> out2 =  // k3 j3 i3 k2
      detail::Shuffle128<_MM_PERM_BDDB>(i3_i1_k2_k0, j3_j1_k3_k1);

  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
}

// Input (128-bit blocks):
// c 8 4 0 (LSB of i)
// d 9 5 1
// e a 6 2
// f b 7 3
// Output:
// 3 2 1 0
// 7 6 5 4
// b a 9 8
// f e d c
template <typename T>
HWY_API void StoreTransposedBlocks4(const Vec512<T> i, const Vec512<T> j,
                                    const Vec512<T> k, const Vec512<T> l,
                                    Full512<T> d, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> j1_j0_i1_i0 = detail::Shuffle128<_MM_PERM_BABA>(i, j);
  const Vec512<T> l1_l0_k1_k0 = detail::Shuffle128<_MM_PERM_BABA>(k, l);
  const Vec512<T> j3_j2_i3_i2 = detail::Shuffle128<_MM_PERM_DCDC>(i, j);
  const Vec512<T> l3_l2_k3_k2 = detail::Shuffle128<_MM_PERM_DCDC>(k, l);
  const Vec512<T> out0 =
      detail::Shuffle128<_MM_PERM_CACA>(j1_j0_i1_i0, l1_l0_k1_k0);
  const Vec512<T> out1 =
      detail::Shuffle128<_MM_PERM_DBDB>(j1_j0_i1_i0, l1_l0_k1_k0);
  const Vec512<T> out2 =
      detail::Shuffle128<_MM_PERM_CACA>(j3_j2_i3_i2, l3_l2_k3_k2);
  const Vec512<T> out3 =
      detail::Shuffle128<_MM_PERM_DBDB>(j3_j2_i3_i2, l3_l2_k3_k2);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
  StoreU(out3, d, unaligned + 3 * N);
}

}  // namespace detail

// ------------------------------ MulEven/Odd (Shuffle2301, InterleaveLower)

HWY_INLINE Vec512<uint64_t> MulEven(const Vec512<uint64_t> a,
                                    const Vec512<uint64_t> b) {
  const DFromV<decltype(a)> du64;
  const RepartitionToNarrow<decltype(du64)> du32;
  const auto maskL = Set(du64, 0xFFFFFFFFULL);
  const auto a32 = BitCast(du32, a);
  const auto b32 = BitCast(du32, b);
  // Inputs for MulEven: we only need the lower 32 bits
  const auto aH = Shuffle2301(a32);
  const auto bH = Shuffle2301(b32);

  // Knuth double-word multiplication. We use 32x32 = 64 MulEven and only need
  // the even (lower 64 bits of every 128-bit block) results. See
  // https://github.com/hcs0/Hackers-Delight/blob/master/muldwu.c.tat
  const auto aLbL = MulEven(a32, b32);
  const auto w3 = aLbL & maskL;

  const auto t2 = MulEven(aH, b32) + ShiftRight<32>(aLbL);
  const auto w2 = t2 & maskL;
  const auto w1 = ShiftRight<32>(t2);

  const auto t = MulEven(a32, bH) + w2;
  const auto k = ShiftRight<32>(t);

  const auto mulH = MulEven(aH, bH) + w1 + k;
  const auto mulL = ShiftLeft<32>(t) + w3;
  return InterleaveLower(mulL, mulH);
}

HWY_INLINE Vec512<uint64_t> MulOdd(const Vec512<uint64_t> a,
                                   const Vec512<uint64_t> b) {
  const DFromV<decltype(a)> du64;
  const RepartitionToNarrow<decltype(du64)> du32;
  const auto maskL = Set(du64, 0xFFFFFFFFULL);
  const auto a32 = BitCast(du32, a);
  const auto b32 = BitCast(du32, b);
  // Inputs for MulEven: we only need bits [95:64] (= upper half of input)
  const auto aH = Shuffle2301(a32);
  const auto bH = Shuffle2301(b32);

  // Same as above, but we're using the odd results (upper 64 bits per block).
  const auto aLbL = MulEven(a32, b32);
  const auto w3 = aLbL & maskL;

  const auto t2 = MulEven(aH, b32) + ShiftRight<32>(aLbL);
  const auto w2 = t2 & maskL;
  const auto w1 = ShiftRight<32>(t2);

  const auto t = MulEven(a32, bH) + w2;
  const auto k = ShiftRight<32>(t);

  const auto mulH = MulEven(aH, bH) + w1 + k;
  const auto mulL = ShiftLeft<32>(t) + w3;
  return InterleaveUpper(du64, mulL, mulH);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

HWY_API Vec512<float> ReorderWidenMulAccumulate(Full512<float> df32,
                                                Vec512<bfloat16_t> a,
                                                Vec512<bfloat16_t> b,
                                                const Vec512<float> sum0,
                                                Vec512<float>& sum1) {
  // TODO(janwas): _mm512_dpbf16_ps when available
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const Vec512<uint16_t> zero = Zero(du16);
  // Lane order within sum0/1 is undefined, hence we can avoid the
  // longer-latency lane-crossing PromoteTo.
  const Vec512<uint32_t> a0 = ZipLower(du32, zero, BitCast(du16, a));
  const Vec512<uint32_t> a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const Vec512<uint32_t> b0 = ZipLower(du32, zero, BitCast(du16, b));
  const Vec512<uint32_t> b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ------------------------------ Reductions

// Returns the sum in each lane.
HWY_API Vec512<int32_t> SumOfLanes(Full512<int32_t> d, Vec512<int32_t> v) {
  return Set(d, _mm512_reduce_add_epi32(v.raw));
}
HWY_API Vec512<int64_t> SumOfLanes(Full512<int64_t> d, Vec512<int64_t> v) {
  return Set(d, _mm512_reduce_add_epi64(v.raw));
}
HWY_API Vec512<uint32_t> SumOfLanes(Full512<uint32_t> d, Vec512<uint32_t> v) {
  return Set(d, static_cast<uint32_t>(_mm512_reduce_add_epi32(v.raw)));
}
HWY_API Vec512<uint64_t> SumOfLanes(Full512<uint64_t> d, Vec512<uint64_t> v) {
  return Set(d, static_cast<uint64_t>(_mm512_reduce_add_epi64(v.raw)));
}
HWY_API Vec512<float> SumOfLanes(Full512<float> d, Vec512<float> v) {
  return Set(d, _mm512_reduce_add_ps(v.raw));
}
HWY_API Vec512<double> SumOfLanes(Full512<double> d, Vec512<double> v) {
  return Set(d, _mm512_reduce_add_pd(v.raw));
}

// Returns the minimum in each lane.
HWY_API Vec512<int32_t> MinOfLanes(Full512<int32_t> d, Vec512<int32_t> v) {
  return Set(d, _mm512_reduce_min_epi32(v.raw));
}
HWY_API Vec512<int64_t> MinOfLanes(Full512<int64_t> d, Vec512<int64_t> v) {
  return Set(d, _mm512_reduce_min_epi64(v.raw));
}
HWY_API Vec512<uint32_t> MinOfLanes(Full512<uint32_t> d, Vec512<uint32_t> v) {
  return Set(d, _mm512_reduce_min_epu32(v.raw));
}
HWY_API Vec512<uint64_t> MinOfLanes(Full512<uint64_t> d, Vec512<uint64_t> v) {
  return Set(d, _mm512_reduce_min_epu64(v.raw));
}
HWY_API Vec512<float> MinOfLanes(Full512<float> d, Vec512<float> v) {
  return Set(d, _mm512_reduce_min_ps(v.raw));
}
HWY_API Vec512<double> MinOfLanes(Full512<double> d, Vec512<double> v) {
  return Set(d, _mm512_reduce_min_pd(v.raw));
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> MinOfLanes(Full512<T> d, Vec512<T> v) {
  const Repartition<int32_t, decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(d, Or(min, ShiftLeft<16>(min)));
}

// Returns the maximum in each lane.
HWY_API Vec512<int32_t> MaxOfLanes(Full512<int32_t> d, Vec512<int32_t> v) {
  return Set(d, _mm512_reduce_max_epi32(v.raw));
}
HWY_API Vec512<int64_t> MaxOfLanes(Full512<int64_t> d, Vec512<int64_t> v) {
  return Set(d, _mm512_reduce_max_epi64(v.raw));
}
HWY_API Vec512<uint32_t> MaxOfLanes(Full512<uint32_t> d, Vec512<uint32_t> v) {
  return Set(d, _mm512_reduce_max_epu32(v.raw));
}
HWY_API Vec512<uint64_t> MaxOfLanes(Full512<uint64_t> d, Vec512<uint64_t> v) {
  return Set(d, _mm512_reduce_max_epu64(v.raw));
}
HWY_API Vec512<float> MaxOfLanes(Full512<float> d, Vec512<float> v) {
  return Set(d, _mm512_reduce_max_ps(v.raw));
}
HWY_API Vec512<double> MaxOfLanes(Full512<double> d, Vec512<double> v) {
  return Set(d, _mm512_reduce_max_pd(v.raw));
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec512<T> MaxOfLanes(Full512<T> d, Vec512<T> v) {
  const Repartition<int32_t, decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(d, Or(min, ShiftLeft<16>(min)));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// Note that the GCC warnings are not suppressed if we only wrap the *intrin.h -
// the warning seems to be issued at the call site of intrinsics, i.e. our code.
HWY_DIAGNOSTICS(pop)
