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

// 256-bit vectors and AVX2 instructions, plus some AVX512-VL operations when
// compiling for that target.
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

// Must come before HWY_COMPILER_CLANGCL
#include <immintrin.h>  // AVX2+

#if HWY_COMPILER_CLANGCL
// Including <immintrin.h> should be enough, but Clang's headers helpfully skip
// including these headers when _MSC_VER is defined, like when using clang-cl.
// Include these directly here.
#include <avxintrin.h>
// avxintrin defines __m256i and must come before avx2intrin.
#include <avx2intrin.h>
#include <bmi2intrin.h>  // _pext_u64
#include <f16cintrin.h>
#include <fmaintrin.h>
#include <smmintrin.h>
#endif  // HWY_COMPILER_CLANGCL

#include <stddef.h>
#include <stdint.h>

#if HWY_IS_MSAN
#include <sanitizer/msan_interface.h>
#endif

// For half-width vectors. Already includes base.h and shared-inl.h.
#include "hwy/ops/x86_128-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

template <typename T>
struct Raw256 {
  using type = __m256i;
};
template <>
struct Raw256<float> {
  using type = __m256;
};
template <>
struct Raw256<double> {
  using type = __m256d;
};

}  // namespace detail

template <typename T>
class Vec256 {
  using Raw = typename detail::Raw256<T>::type;

 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec256& operator*=(const Vec256 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec256& operator/=(const Vec256 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec256& operator+=(const Vec256 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec256& operator-=(const Vec256 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec256& operator&=(const Vec256 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec256& operator|=(const Vec256 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec256& operator^=(const Vec256 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

#if HWY_TARGET <= HWY_AVX3

namespace detail {

// Template arg: sizeof(lane type)
template <size_t size>
struct RawMask256 {};
template <>
struct RawMask256<1> {
  using type = __mmask32;
};
template <>
struct RawMask256<2> {
  using type = __mmask16;
};
template <>
struct RawMask256<4> {
  using type = __mmask8;
};
template <>
struct RawMask256<8> {
  using type = __mmask8;
};

}  // namespace detail

template <typename T>
struct Mask256 {
  using Raw = typename detail::RawMask256<sizeof(T)>::type;

  static Mask256<T> FromBits(uint64_t mask_bits) {
    return Mask256<T>{static_cast<Raw>(mask_bits)};
  }

  Raw raw;
};

#else  // AVX2

// FF..FF or 0.
template <typename T>
struct Mask256 {
  typename detail::Raw256<T>::type raw;
};

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m256i BitCastToInteger(__m256i v) { return v; }
HWY_INLINE __m256i BitCastToInteger(__m256 v) { return _mm256_castps_si256(v); }
HWY_INLINE __m256i BitCastToInteger(__m256d v) {
  return _mm256_castpd_si256(v);
}

template <typename T>
HWY_INLINE Vec256<uint8_t> BitCastToByte(Vec256<T> v) {
  return Vec256<uint8_t>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger256 {
  HWY_INLINE __m256i operator()(__m256i v) { return v; }
};
template <>
struct BitCastFromInteger256<float> {
  HWY_INLINE __m256 operator()(__m256i v) { return _mm256_castsi256_ps(v); }
};
template <>
struct BitCastFromInteger256<double> {
  HWY_INLINE __m256d operator()(__m256i v) { return _mm256_castsi256_pd(v); }
};

template <typename T>
HWY_INLINE Vec256<T> BitCastFromByte(Full256<T> /* tag */, Vec256<uint8_t> v) {
  return Vec256<T>{BitCastFromInteger256<T>()(v.raw)};
}

}  // namespace detail

template <typename T, typename FromT>
HWY_API Vec256<T> BitCast(Full256<T> d, Vec256<FromT> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

// Returns an all-zero vector.
template <typename T>
HWY_API Vec256<T> Zero(Full256<T> /* tag */) {
  return Vec256<T>{_mm256_setzero_si256()};
}
HWY_API Vec256<float> Zero(Full256<float> /* tag */) {
  return Vec256<float>{_mm256_setzero_ps()};
}
HWY_API Vec256<double> Zero(Full256<double> /* tag */) {
  return Vec256<double>{_mm256_setzero_pd()};
}

// Returns a vector with all lanes set to "t".
HWY_API Vec256<uint8_t> Set(Full256<uint8_t> /* tag */, const uint8_t t) {
  return Vec256<uint8_t>{_mm256_set1_epi8(static_cast<char>(t))};  // NOLINT
}
HWY_API Vec256<uint16_t> Set(Full256<uint16_t> /* tag */, const uint16_t t) {
  return Vec256<uint16_t>{_mm256_set1_epi16(static_cast<short>(t))};  // NOLINT
}
HWY_API Vec256<uint32_t> Set(Full256<uint32_t> /* tag */, const uint32_t t) {
  return Vec256<uint32_t>{_mm256_set1_epi32(static_cast<int>(t))};
}
HWY_API Vec256<uint64_t> Set(Full256<uint64_t> /* tag */, const uint64_t t) {
  return Vec256<uint64_t>{
      _mm256_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
HWY_API Vec256<int8_t> Set(Full256<int8_t> /* tag */, const int8_t t) {
  return Vec256<int8_t>{_mm256_set1_epi8(static_cast<char>(t))};  // NOLINT
}
HWY_API Vec256<int16_t> Set(Full256<int16_t> /* tag */, const int16_t t) {
  return Vec256<int16_t>{_mm256_set1_epi16(static_cast<short>(t))};  // NOLINT
}
HWY_API Vec256<int32_t> Set(Full256<int32_t> /* tag */, const int32_t t) {
  return Vec256<int32_t>{_mm256_set1_epi32(t)};
}
HWY_API Vec256<int64_t> Set(Full256<int64_t> /* tag */, const int64_t t) {
  return Vec256<int64_t>{
      _mm256_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
HWY_API Vec256<float> Set(Full256<float> /* tag */, const float t) {
  return Vec256<float>{_mm256_set1_ps(t)};
}
HWY_API Vec256<double> Set(Full256<double> /* tag */, const double t) {
  return Vec256<double>{_mm256_set1_pd(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T>
HWY_API Vec256<T> Undefined(Full256<T> /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return Vec256<T>{_mm256_undefined_si256()};
}
HWY_API Vec256<float> Undefined(Full256<float> /* tag */) {
  return Vec256<float>{_mm256_undefined_ps()};
}
HWY_API Vec256<double> Undefined(Full256<double> /* tag */) {
  return Vec256<double>{_mm256_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ================================================== LOGICAL

// ------------------------------ And

template <typename T>
HWY_API Vec256<T> And(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{_mm256_and_si256(a.raw, b.raw)};
}

HWY_API Vec256<float> And(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_and_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> And(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T>
HWY_API Vec256<T> AndNot(Vec256<T> not_mask, Vec256<T> mask) {
  return Vec256<T>{_mm256_andnot_si256(not_mask.raw, mask.raw)};
}
HWY_API Vec256<float> AndNot(const Vec256<float> not_mask,
                             const Vec256<float> mask) {
  return Vec256<float>{_mm256_andnot_ps(not_mask.raw, mask.raw)};
}
HWY_API Vec256<double> AndNot(const Vec256<double> not_mask,
                              const Vec256<double> mask) {
  return Vec256<double>{_mm256_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T>
HWY_API Vec256<T> Or(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{_mm256_or_si256(a.raw, b.raw)};
}

HWY_API Vec256<float> Or(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_or_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Or(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec256<T> Xor(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{_mm256_xor_si256(a.raw, b.raw)};
}

HWY_API Vec256<float> Xor(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_xor_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Xor(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Not

template <typename T>
HWY_API Vec256<T> Not(const Vec256<T> v) {
  using TU = MakeUnsigned<T>;
#if HWY_TARGET <= HWY_AVX3
  const __m256i vu = BitCast(Full256<TU>(), v).raw;
  return BitCast(Full256<T>(),
                 Vec256<TU>{_mm256_ternarylogic_epi32(vu, vu, vu, 0x55)});
#else
  return Xor(v, BitCast(Full256<T>(), Vec256<TU>{_mm256_set1_epi32(-1)}));
#endif
}

// ------------------------------ Or3

template <typename T>
HWY_API Vec256<T> Or3(Vec256<T> o1, Vec256<T> o2, Vec256<T> o3) {
#if HWY_TARGET <= HWY_AVX3
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m256i ret = _mm256_ternarylogic_epi64(
      BitCast(du, o1).raw, BitCast(du, o2).raw, BitCast(du, o3).raw, 0xFE);
  return BitCast(d, VU{ret});
#else
  return Or(o1, Or(o2, o3));
#endif
}

// ------------------------------ OrAnd

template <typename T>
HWY_API Vec256<T> OrAnd(Vec256<T> o, Vec256<T> a1, Vec256<T> a2) {
#if HWY_TARGET <= HWY_AVX3
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m256i ret = _mm256_ternarylogic_epi64(
      BitCast(du, o).raw, BitCast(du, a1).raw, BitCast(du, a2).raw, 0xF8);
  return BitCast(d, VU{ret});
#else
  return Or(o, And(a1, a2));
#endif
}

// ------------------------------ IfVecThenElse

template <typename T>
HWY_API Vec256<T> IfVecThenElse(Vec256<T> mask, Vec256<T> yes, Vec256<T> no) {
#if HWY_TARGET <= HWY_AVX3
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{_mm256_ternarylogic_epi64(BitCast(du, mask).raw,
                                                 BitCast(du, yes).raw,
                                                 BitCast(du, no).raw, 0xCA)});
#else
  return IfThenElse(MaskFromVec(mask), yes, no);
#endif
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T>
HWY_API Vec256<T> operator&(const Vec256<T> a, const Vec256<T> b) {
  return And(a, b);
}

template <typename T>
HWY_API Vec256<T> operator|(const Vec256<T> a, const Vec256<T> b) {
  return Or(a, b);
}

template <typename T>
HWY_API Vec256<T> operator^(const Vec256<T> a, const Vec256<T> b) {
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
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi8(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi16(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi32(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi64(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> PopulationCount(Vec256<T> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

#endif  // HWY_TARGET == HWY_AVX3_DL

// ================================================== SIGN

// ------------------------------ CopySign

template <typename T>
HWY_API Vec256<T> CopySign(const Vec256<T> magn, const Vec256<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const Full256<T> d;
  const auto msb = SignBit(d);

#if HWY_TARGET <= HWY_AVX3
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
  const __m256i out = _mm256_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, decltype(Zero(du)){out});
#else
  return Or(AndNot(msb, magn), And(msb, sign));
#endif
}

template <typename T>
HWY_API Vec256<T> CopySignToAbs(const Vec256<T> abs, const Vec256<T> sign) {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
#else
  return Or(abs, And(SignBit(Full256<T>()), sign));
#endif
}

// ================================================== MASK

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ IfThenElse

// Returns mask ? b : a.

namespace detail {

// Templates for signed/unsigned integer of a particular size.
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<1> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi8(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<2> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi16(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<4> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi32(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<8> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi64(no.raw, mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return detail::IfThenElse(hwy::SizeTag<sizeof(T)>(), mask, yes, no);
}
HWY_API Vec256<float> IfThenElse(Mask256<float> mask, Vec256<float> yes,
                                 Vec256<float> no) {
  return Vec256<float>{_mm256_mask_mov_ps(no.raw, mask.raw, yes.raw)};
}
HWY_API Vec256<double> IfThenElse(Mask256<double> mask, Vec256<double> yes,
                                  Vec256<double> no) {
  return Vec256<double>{_mm256_mask_mov_pd(no.raw, mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<1> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi8(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<2> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi16(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<4> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi32(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<8> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi64(mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  return detail::IfThenElseZero(hwy::SizeTag<sizeof(T)>(), mask, yes);
}
HWY_API Vec256<float> IfThenElseZero(Mask256<float> mask, Vec256<float> yes) {
  return Vec256<float>{_mm256_maskz_mov_ps(mask.raw, yes.raw)};
}
HWY_API Vec256<double> IfThenElseZero(Mask256<double> mask,
                                      Vec256<double> yes) {
  return Vec256<double>{_mm256_maskz_mov_pd(mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<1> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  // xor_epi8/16 are missing, but we have sub, which is just as fast for u8/16.
  return Vec256<T>{_mm256_mask_sub_epi8(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<2> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  return Vec256<T>{_mm256_mask_sub_epi16(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<4> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  return Vec256<T>{_mm256_mask_xor_epi32(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<8> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  return Vec256<T>{_mm256_mask_xor_epi64(no.raw, mask.raw, no.raw, no.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  return detail::IfThenZeroElse(hwy::SizeTag<sizeof(T)>(), mask, no);
}
HWY_API Vec256<float> IfThenZeroElse(Mask256<float> mask, Vec256<float> no) {
  return Vec256<float>{_mm256_mask_xor_ps(no.raw, mask.raw, no.raw, no.raw)};
}
HWY_API Vec256<double> IfThenZeroElse(Mask256<double> mask, Vec256<double> no) {
  return Vec256<double>{_mm256_mask_xor_pd(no.raw, mask.raw, no.raw, no.raw)};
}

template <typename T>
HWY_API Vec256<T> ZeroIfNegative(const Vec256<T> v) {
  static_assert(IsSigned<T>(), "Only for float");
  // AVX3 MaskFromVec only looks at the MSB
  return IfThenZeroElse(MaskFromVec(v), v);
}

// ------------------------------ Mask logical

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask32>(a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask16>(a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask32>(~a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask16>(~a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask32>(a.raw | b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask16>(a.raw | b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask32>(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask16>(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> And(const Mask256<T> a, Mask256<T> b) {
  return detail::And(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> AndNot(const Mask256<T> a, Mask256<T> b) {
  return detail::AndNot(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> Or(const Mask256<T> a, Mask256<T> b) {
  return detail::Or(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> Xor(const Mask256<T> a, Mask256<T> b) {
  return detail::Xor(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> Not(const Mask256<T> m) {
  // Flip only the valid bits.
  constexpr size_t N = 32 / sizeof(T);
  return Xor(m, Mask256<T>::FromBits((1ull << N) - 1));
}

#else  // AVX2

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  return Mask256<T>{v.raw};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{v.raw};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(Full256<T> /* tag */, const Mask256<T> v) {
  return Vec256<T>{v.raw};
}

// ------------------------------ IfThenElse

// mask ? yes : no
template <typename T>
HWY_API Vec256<T> IfThenElse(const Mask256<T> mask, const Vec256<T> yes,
                             const Vec256<T> no) {
  return Vec256<T>{_mm256_blendv_epi8(no.raw, yes.raw, mask.raw)};
}
HWY_API Vec256<float> IfThenElse(const Mask256<float> mask,
                                 const Vec256<float> yes,
                                 const Vec256<float> no) {
  return Vec256<float>{_mm256_blendv_ps(no.raw, yes.raw, mask.raw)};
}
HWY_API Vec256<double> IfThenElse(const Mask256<double> mask,
                                  const Vec256<double> yes,
                                  const Vec256<double> no) {
  return Vec256<double>{_mm256_blendv_pd(no.raw, yes.raw, mask.raw)};
}

// mask ? yes : 0
template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  return yes & VecFromMask(Full256<T>(), mask);
}

// mask ? 0 : no
template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  return AndNot(VecFromMask(Full256<T>(), mask), no);
}

template <typename T>
HWY_API Vec256<T> ZeroIfNegative(Vec256<T> v) {
  static_assert(IsSigned<T>(), "Only for float");
  const auto zero = Zero(Full256<T>());
  // AVX2 IfThenElse only looks at the MSB for 32/64-bit lanes
  return IfThenElse(MaskFromVec(v), zero, v);
}

// ------------------------------ Mask logical

template <typename T>
HWY_API Mask256<T> Not(const Mask256<T> m) {
  return MaskFromVec(Not(VecFromMask(Full256<T>(), m)));
}

template <typename T>
HWY_API Mask256<T> And(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> AndNot(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Or(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Xor(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

#endif  // HWY_TARGET <= HWY_AVX3

// ================================================== COMPARE

#if HWY_TARGET <= HWY_AVX3

// Comparisons set a mask bit to 1 if the condition is true, else 0.

template <typename TFrom, typename TTo>
HWY_API Mask256<TTo> RebindMask(Full256<TTo> /*tag*/, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask256<TTo>{m.raw};
}

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<1> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi8_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<2> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi16_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<4> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi32_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<8> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi64_mask(v.raw, bit.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> TestBit(const Vec256<T> v, const Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return detail::TestBit(hwy::SizeTag<sizeof(T)>(), v, bit);
}

// ------------------------------ Equality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask256<float> operator==(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

HWY_API Mask256<double> operator==(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask256<float> operator!=(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

HWY_API Mask256<double> operator!=(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

HWY_API Mask256<int8_t> operator>(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Mask256<int8_t>{_mm256_cmpgt_epi8_mask(a.raw, b.raw)};
}
HWY_API Mask256<int16_t> operator>(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Mask256<int16_t>{_mm256_cmpgt_epi16_mask(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> operator>(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Mask256<int32_t>{_mm256_cmpgt_epi32_mask(a.raw, b.raw)};
}
HWY_API Mask256<int64_t> operator>(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Mask256<int64_t>{_mm256_cmpgt_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask256<uint8_t> operator>(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Mask256<uint8_t>{_mm256_cmpgt_epu8_mask(a.raw, b.raw)};
}
HWY_API Mask256<uint16_t> operator>(const Vec256<uint16_t> a,
                                    const Vec256<uint16_t> b) {
  return Mask256<uint16_t>{_mm256_cmpgt_epu16_mask(a.raw, b.raw)};
}
HWY_API Mask256<uint32_t> operator>(const Vec256<uint32_t> a,
                                    const Vec256<uint32_t> b) {
  return Mask256<uint32_t>{_mm256_cmpgt_epu32_mask(a.raw, b.raw)};
}
HWY_API Mask256<uint64_t> operator>(const Vec256<uint64_t> a,
                                    const Vec256<uint64_t> b) {
  return Mask256<uint64_t>{_mm256_cmpgt_epu64_mask(a.raw, b.raw)};
}

HWY_API Mask256<float> operator>(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_GT_OQ)};
}
HWY_API Mask256<double> operator>(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

HWY_API Mask256<float> operator>=(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_GE_OQ)};
}
HWY_API Mask256<double> operator>=(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_GE_OQ)};
}

// ------------------------------ Mask

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<1> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi8_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<2> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi16_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<4> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi32_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<8> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi64_mask(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  return detail::MaskFromVec(hwy::SizeTag<sizeof(T)>(), v);
}
// There do not seem to be native floating-point versions of these instructions.
HWY_API Mask256<float> MaskFromVec(const Vec256<float> v) {
  return Mask256<float>{MaskFromVec(BitCast(Full256<int32_t>(), v)).raw};
}
HWY_API Mask256<double> MaskFromVec(const Vec256<double> v) {
  return Mask256<double>{MaskFromVec(BitCast(Full256<int64_t>(), v)).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi8(v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi16(v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi32(v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi64(v.raw)};
}

HWY_API Vec256<float> VecFromMask(const Mask256<float> v) {
  return Vec256<float>{_mm256_castsi256_ps(_mm256_movm_epi32(v.raw))};
}

HWY_API Vec256<double> VecFromMask(const Mask256<double> v) {
  return Vec256<double>{_mm256_castsi256_pd(_mm256_movm_epi64(v.raw))};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(Full256<T> /* tag */, const Mask256<T> v) {
  return VecFromMask(v);
}

#else  // AVX2

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <typename TFrom, typename TTo>
HWY_API Mask256<TTo> RebindMask(Full256<TTo> d_to, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return MaskFromVec(BitCast(d_to, VecFromMask(Full256<TFrom>(), m)));
}

template <typename T>
HWY_API Mask256<T> TestBit(const Vec256<T> v, const Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi8(a.raw, b.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi16(a.raw, b.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi32(a.raw, b.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi64(a.raw, b.raw)};
}

HWY_API Mask256<float> operator==(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_EQ_OQ)};
}

HWY_API Mask256<double> operator==(const Vec256<double> a,
                                   const Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Not(a == b);
}
HWY_API Mask256<float> operator!=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_NEQ_OQ)};
}
HWY_API Mask256<double> operator!=(const Vec256<double> a,
                                   const Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

// Pre-9.3 GCC immintrin.h uses char, which may be unsigned, causing cmpgt_epi8
// to perform an unsigned comparison instead of the intended signed. Workaround
// is to cast to an explicitly signed type. See https://godbolt.org/z/PL7Ujy
#if HWY_COMPILER_GCC != 0 && HWY_COMPILER_GCC < 930
#define HWY_AVX2_GCC_CMPGT8_WORKAROUND 1
#else
#define HWY_AVX2_GCC_CMPGT8_WORKAROUND 0
#endif

HWY_API Mask256<int8_t> Gt(hwy::SignedTag /*tag*/, Vec256<int8_t> a,
                           Vec256<int8_t> b) {
#if HWY_AVX2_GCC_CMPGT8_WORKAROUND
  using i8x32 = signed char __attribute__((__vector_size__(32)));
  return Mask256<int8_t>{static_cast<__m256i>(reinterpret_cast<i8x32>(a.raw) >
                                              reinterpret_cast<i8x32>(b.raw))};
#else
  return Mask256<int8_t>{_mm256_cmpgt_epi8(a.raw, b.raw)};
#endif
}
HWY_API Mask256<int16_t> Gt(hwy::SignedTag /*tag*/, Vec256<int16_t> a,
                            Vec256<int16_t> b) {
  return Mask256<int16_t>{_mm256_cmpgt_epi16(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> Gt(hwy::SignedTag /*tag*/, Vec256<int32_t> a,
                            Vec256<int32_t> b) {
  return Mask256<int32_t>{_mm256_cmpgt_epi32(a.raw, b.raw)};
}
HWY_API Mask256<int64_t> Gt(hwy::SignedTag /*tag*/, Vec256<int64_t> a,
                            Vec256<int64_t> b) {
  return Mask256<int64_t>{_mm256_cmpgt_epi64(a.raw, b.raw)};
}

template <typename T>
HWY_INLINE Mask256<T> Gt(hwy::UnsignedTag /*tag*/, Vec256<T> a, Vec256<T> b) {
  const Full256<T> du;
  const RebindToSigned<decltype(du)> di;
  const Vec256<T> msb = Set(du, (LimitsMax<T>() >> 1) + 1);
  return RebindMask(du, BitCast(di, Xor(a, msb)) > BitCast(di, Xor(b, msb)));
}

HWY_API Mask256<float> Gt(hwy::FloatTag /*tag*/, Vec256<float> a,
                          Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_GT_OQ)};
}
HWY_API Mask256<double> Gt(hwy::FloatTag /*tag*/, Vec256<double> a,
                           Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_GT_OQ)};
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> operator>(Vec256<T> a, Vec256<T> b) {
  return detail::Gt(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Weak inequality

HWY_API Mask256<float> operator>=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_GE_OQ)};
}
HWY_API Mask256<double> operator>=(const Vec256<double> a,
                                   const Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_GE_OQ)};
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ Reversed comparisons

template <typename T>
HWY_API Mask256<T> operator<(const Vec256<T> a, const Vec256<T> b) {
  return b > a;
}

template <typename T>
HWY_API Mask256<T> operator<=(const Vec256<T> a, const Vec256<T> b) {
  return b >= a;
}

// ------------------------------ Min (Gt, IfThenElse)

// Unsigned
HWY_API Vec256<uint8_t> Min(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_min_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Min(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_min_epu16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Min(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_min_epu32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Min(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint64_t>{_mm256_min_epu64(a.raw, b.raw)};
#else
  const Full256<uint64_t> du;
  const Full256<int64_t> di;
  const auto msb = Set(du, 1ull << 63);
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, b, a);
#endif
}

// Signed
HWY_API Vec256<int8_t> Min(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_min_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Min(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_min_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Min(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_min_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Min(const Vec256<int64_t> a, const Vec256<int64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_min_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, a, b);
#endif
}

// Float
HWY_API Vec256<float> Min(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_min_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Min(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_min_pd(a.raw, b.raw)};
}

// ------------------------------ Max (Gt, IfThenElse)

// Unsigned
HWY_API Vec256<uint8_t> Max(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_max_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Max(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_max_epu16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Max(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_max_epu32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Max(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint64_t>{_mm256_max_epu64(a.raw, b.raw)};
#else
  const Full256<uint64_t> du;
  const Full256<int64_t> di;
  const auto msb = Set(du, 1ull << 63);
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, a, b);
#endif
}

// Signed
HWY_API Vec256<int8_t> Max(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_max_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Max(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_max_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Max(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_max_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Max(const Vec256<int64_t> a, const Vec256<int64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_max_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, b, a);
#endif
}

// Float
HWY_API Vec256<float> Max(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_max_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Max(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_max_pd(a.raw, b.raw)};
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T>
HWY_API Mask256<T> FirstN(const Full256<T> d, size_t n) {
#if HWY_TARGET <= HWY_AVX3
  (void)d;
  constexpr size_t N = 32 / sizeof(T);
#if HWY_ARCH_X86_64
  const uint64_t all = (1ull << N) - 1;
  // BZHI only looks at the lower 8 bits of n!
  return Mask256<T>::FromBits((n > 255) ? all : _bzhi_u64(all, n));
#else
  const uint32_t all = static_cast<uint32_t>((1ull << N) - 1);
  // BZHI only looks at the lower 8 bits of n!
  return Mask256<T>::FromBits(
      (n > 255) ? all : _bzhi_u32(all, static_cast<uint32_t>(n)));
#endif  // HWY_ARCH_X86_64
#else
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(n)));
#endif
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
HWY_API Vec256<uint8_t> operator+(const Vec256<uint8_t> a,
                                  const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_add_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator+(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_add_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator+(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_add_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator+(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_add_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator+(const Vec256<int8_t> a,
                                 const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_add_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator+(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_add_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator+(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_add_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator+(const Vec256<int64_t> a,
                                  const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_add_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> operator+(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_add_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator+(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
HWY_API Vec256<uint8_t> operator-(const Vec256<uint8_t> a,
                                  const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator-(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator-(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator-(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_sub_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator-(const Vec256<int8_t> a,
                                 const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator-(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator-(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator-(const Vec256<int64_t> a,
                                  const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_sub_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> operator-(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_sub_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator-(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_sub_pd(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
HWY_API Vec256<uint64_t> SumsOf8(const Vec256<uint8_t> v) {
  return Vec256<uint64_t>{_mm256_sad_epu8(v.raw, _mm256_setzero_si256())};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedAdd(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_adds_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedAdd(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_adds_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedAdd(const Vec256<int8_t> a,
                                    const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_adds_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedAdd(const Vec256<int16_t> a,
                                     const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_adds_epi16(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedSub(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_subs_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedSub(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_subs_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedSub(const Vec256<int8_t> a,
                                    const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_subs_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedSub(const Vec256<int16_t> a,
                                     const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_subs_epi16(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
HWY_API Vec256<uint8_t> AverageRound(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_avg_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> AverageRound(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Abs (Sub)

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec256<int8_t> Abs(const Vec256<int8_t> v) {
#if HWY_COMPILER_MSVC
  // Workaround for incorrect codegen? (wrong result)
  const auto zero = Zero(Full256<int8_t>());
  return Vec256<int8_t>{_mm256_max_epi8(v.raw, (zero - v).raw)};
#else
  return Vec256<int8_t>{_mm256_abs_epi8(v.raw)};
#endif
}
HWY_API Vec256<int16_t> Abs(const Vec256<int16_t> v) {
  return Vec256<int16_t>{_mm256_abs_epi16(v.raw)};
}
HWY_API Vec256<int32_t> Abs(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_abs_epi32(v.raw)};
}
// i64 is implemented after BroadcastSignBit.

HWY_API Vec256<float> Abs(const Vec256<float> v) {
  const Vec256<int32_t> mask{_mm256_set1_epi32(0x7FFFFFFF)};
  return v & BitCast(Full256<float>(), mask);
}
HWY_API Vec256<double> Abs(const Vec256<double> v) {
  const Vec256<int64_t> mask{_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFLL)};
  return v & BitCast(Full256<double>(), mask);
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec256<uint16_t> operator*(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator*(Vec256<uint32_t> a, Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_mullo_epi32(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int16_t> operator*(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator*(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_mullo_epi32(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec256<uint16_t> MulHigh(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_mulhi_epu16(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> MulHigh(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_mulhi_epi16(a.raw, b.raw)};
}

HWY_API Vec256<int16_t> MulFixedPoint15(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_mulhrs_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec256<int64_t> MulEven(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int64_t>{_mm256_mul_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> MulEven(Vec256<uint32_t> a, Vec256<uint32_t> b) {
  return Vec256<uint64_t>{_mm256_mul_epu32(a.raw, b.raw)};
}

// ------------------------------ ShiftLeft

template <int kBits>
HWY_API Vec256<uint16_t> ShiftLeft(const Vec256<uint16_t> v) {
  return Vec256<uint16_t>{_mm256_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint32_t> ShiftLeft(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint64_t> ShiftLeft(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{_mm256_slli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int16_t> ShiftLeft(const Vec256<int16_t> v) {
  return Vec256<int16_t>{_mm256_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int32_t> ShiftLeft(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int64_t> ShiftLeft(const Vec256<int64_t> v) {
  return Vec256<int64_t>{_mm256_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ShiftLeft(const Vec256<T> v) {
  const Full256<T> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeft<kBits>(BitCast(d16, v)));
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits>
HWY_API Vec256<uint16_t> ShiftRight(const Vec256<uint16_t> v) {
  return Vec256<uint16_t>{_mm256_srli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint32_t> ShiftRight(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_srli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint64_t> ShiftRight(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{_mm256_srli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint8_t> ShiftRight(const Vec256<uint8_t> v) {
  const Full256<uint8_t> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec256<uint8_t> shifted{ShiftRight<kBits>(Vec256<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits>
HWY_API Vec256<int16_t> ShiftRight(const Vec256<int16_t> v) {
  return Vec256<int16_t>{_mm256_srai_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int32_t> ShiftRight(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_srai_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int8_t> ShiftRight(const Vec256<int8_t> v) {
  const Full256<int8_t> di;
  const Full256<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// i64 is implemented after BroadcastSignBit.

// ------------------------------ RotateRight

template <int kBits>
HWY_API Vec256<uint32_t> RotateRight(const Vec256<uint32_t> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint32_t>{_mm256_ror_epi32(v.raw, kBits)};
#else
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(31, 32 - kBits)>(v));
#endif
}

template <int kBits>
HWY_API Vec256<uint64_t> RotateRight(const Vec256<uint64_t> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint64_t>{_mm256_ror_epi64(v.raw, kBits)};
#else
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(63, 64 - kBits)>(v));
#endif
}

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

HWY_API Vec256<int8_t> BroadcastSignBit(const Vec256<int8_t> v) {
  return VecFromMask(v < Zero(Full256<int8_t>()));
}

HWY_API Vec256<int16_t> BroadcastSignBit(const Vec256<int16_t> v) {
  return ShiftRight<15>(v);
}

HWY_API Vec256<int32_t> BroadcastSignBit(const Vec256<int32_t> v) {
  return ShiftRight<31>(v);
}

HWY_API Vec256<int64_t> BroadcastSignBit(const Vec256<int64_t> v) {
#if HWY_TARGET == HWY_AVX2
  return VecFromMask(v < Zero(Full256<int64_t>()));
#else
  return Vec256<int64_t>{_mm256_srai_epi64(v.raw, 63)};
#endif
}

template <int kBits>
HWY_API Vec256<int64_t> ShiftRight(const Vec256<int64_t> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_srai_epi64(v.raw, kBits)};
#else
  const Full256<int64_t> di;
  const Full256<uint64_t> du;
  const auto right = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto sign = ShiftLeft<64 - kBits>(BroadcastSignBit(v));
  return right | sign;
#endif
}

HWY_API Vec256<int64_t> Abs(const Vec256<int64_t> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_abs_epi64(v.raw)};
#else
  const auto zero = Zero(Full256<int64_t>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

// ------------------------------ IfNegativeThenElse (BroadcastSignBit)
HWY_API Vec256<int8_t> IfNegativeThenElse(Vec256<int8_t> v, Vec256<int8_t> yes,
                                          Vec256<int8_t> no) {
  // int8: AVX2 IfThenElse only looks at the MSB.
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> IfNegativeThenElse(Vec256<T> v, Vec256<T> yes, Vec256<T> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const Full256<T> d;
  const RebindToSigned<decltype(d)> di;

  // 16-bit: no native blendv, so copy sign to lower byte's MSB.
  v = BitCast(d, BroadcastSignBit(BitCast(di, v)));
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API Vec256<T> IfNegativeThenElse(Vec256<T> v, Vec256<T> yes, Vec256<T> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const Full256<T> d;
  const RebindToFloat<decltype(d)> df;

  // 32/64-bit: use float IfThenElse, which only looks at the MSB.
  const MFromD<decltype(df)> msb = MaskFromVec(BitCast(df, v));
  return BitCast(d, IfThenElse(msb, BitCast(df, yes), BitCast(df, no)));
}

// ------------------------------ ShiftLeftSame

HWY_API Vec256<uint16_t> ShiftLeftSame(const Vec256<uint16_t> v,
                                       const int bits) {
  return Vec256<uint16_t>{_mm256_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint32_t> ShiftLeftSame(const Vec256<uint32_t> v,
                                       const int bits) {
  return Vec256<uint32_t>{_mm256_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint64_t> ShiftLeftSame(const Vec256<uint64_t> v,
                                       const int bits) {
  return Vec256<uint64_t>{_mm256_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int16_t> ShiftLeftSame(const Vec256<int16_t> v, const int bits) {
  return Vec256<int16_t>{_mm256_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int32_t> ShiftLeftSame(const Vec256<int32_t> v, const int bits) {
  return Vec256<int32_t>{_mm256_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int64_t> ShiftLeftSame(const Vec256<int64_t> v, const int bits) {
  return Vec256<int64_t>{_mm256_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ShiftLeftSame(const Vec256<T> v, const int bits) {
  const Full256<T> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeftSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

// ------------------------------ ShiftRightSame (BroadcastSignBit)

HWY_API Vec256<uint16_t> ShiftRightSame(const Vec256<uint16_t> v,
                                        const int bits) {
  return Vec256<uint16_t>{_mm256_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint32_t> ShiftRightSame(const Vec256<uint32_t> v,
                                        const int bits) {
  return Vec256<uint32_t>{_mm256_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint64_t> ShiftRightSame(const Vec256<uint64_t> v,
                                        const int bits) {
  return Vec256<uint64_t>{_mm256_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<uint8_t> ShiftRightSame(Vec256<uint8_t> v, const int bits) {
  const Full256<uint8_t> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftRightSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<uint8_t>(0xFF >> bits));
}

HWY_API Vec256<int16_t> ShiftRightSame(const Vec256<int16_t> v,
                                       const int bits) {
  return Vec256<int16_t>{_mm256_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int32_t> ShiftRightSame(const Vec256<int32_t> v,
                                       const int bits) {
  return Vec256<int32_t>{_mm256_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<int64_t> ShiftRightSame(const Vec256<int64_t> v,
                                       const int bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
#else
  const Full256<int64_t> di;
  const Full256<uint64_t> du;
  const auto right = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto sign = ShiftLeftSame(BroadcastSignBit(v), 64 - bits);
  return right | sign;
#endif
}

HWY_API Vec256<int8_t> ShiftRightSame(Vec256<int8_t> v, const int bits) {
  const Full256<int8_t> di;
  const Full256<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign =
      BitCast(di, Set(du, static_cast<uint8_t>(0x80 >> bits)));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Neg (Xor, Sub)

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T>
HWY_INLINE Vec256<T> Neg(hwy::FloatTag /*tag*/, const Vec256<T> v) {
  return Xor(v, SignBit(Full256<T>()));
}

// Not floating-point
template <typename T>
HWY_INLINE Vec256<T> Neg(hwy::NonFloatTag /*tag*/, const Vec256<T> v) {
  return Zero(Full256<T>()) - v;
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> Neg(const Vec256<T> v) {
  return detail::Neg(hwy::IsFloatTag<T>(), v);
}

// ------------------------------ Floating-point mul / div

HWY_API Vec256<float> operator*(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_mul_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator*(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_mul_pd(a.raw, b.raw)};
}

HWY_API Vec256<float> operator/(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_div_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator/(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_div_pd(a.raw, b.raw)};
}

// Approximate reciprocal
HWY_API Vec256<float> ApproximateReciprocal(const Vec256<float> v) {
  return Vec256<float>{_mm256_rcp_ps(v.raw)};
}

// Absolute value of difference.
HWY_API Vec256<float> AbsDiff(const Vec256<float> a, const Vec256<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
HWY_API Vec256<float> MulAdd(const Vec256<float> mul, const Vec256<float> x,
                             const Vec256<float> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x + add;
#else
  return Vec256<float>{_mm256_fmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
HWY_API Vec256<double> MulAdd(const Vec256<double> mul, const Vec256<double> x,
                              const Vec256<double> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x + add;
#else
  return Vec256<double>{_mm256_fmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns add - mul * x
HWY_API Vec256<float> NegMulAdd(const Vec256<float> mul, const Vec256<float> x,
                                const Vec256<float> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return add - mul * x;
#else
  return Vec256<float>{_mm256_fnmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
HWY_API Vec256<double> NegMulAdd(const Vec256<double> mul,
                                 const Vec256<double> x,
                                 const Vec256<double> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return add - mul * x;
#else
  return Vec256<double>{_mm256_fnmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns mul * x - sub
HWY_API Vec256<float> MulSub(const Vec256<float> mul, const Vec256<float> x,
                             const Vec256<float> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x - sub;
#else
  return Vec256<float>{_mm256_fmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
HWY_API Vec256<double> MulSub(const Vec256<double> mul, const Vec256<double> x,
                              const Vec256<double> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x - sub;
#else
  return Vec256<double>{_mm256_fmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// Returns -mul * x - sub
HWY_API Vec256<float> NegMulSub(const Vec256<float> mul, const Vec256<float> x,
                                const Vec256<float> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return Neg(mul * x) - sub;
#else
  return Vec256<float>{_mm256_fnmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
HWY_API Vec256<double> NegMulSub(const Vec256<double> mul,
                                 const Vec256<double> x,
                                 const Vec256<double> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return Neg(mul * x) - sub;
#else
  return Vec256<double>{_mm256_fnmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// ------------------------------ Floating-point square root

// Full precision square root
HWY_API Vec256<float> Sqrt(const Vec256<float> v) {
  return Vec256<float>{_mm256_sqrt_ps(v.raw)};
}
HWY_API Vec256<double> Sqrt(const Vec256<double> v) {
  return Vec256<double>{_mm256_sqrt_pd(v.raw)};
}

// Approximate reciprocal square root
HWY_API Vec256<float> ApproximateReciprocalSqrt(const Vec256<float> v) {
  return Vec256<float>{_mm256_rsqrt_ps(v.raw)};
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, tie to even
HWY_API Vec256<float> Round(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Round(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
HWY_API Vec256<float> Trunc(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Trunc(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
HWY_API Vec256<float> Ceil(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Ceil(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
HWY_API Vec256<float> Floor(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Floor(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

// ------------------------------ Floating-point classification

HWY_API Mask256<float> IsNaN(const Vec256<float> v) {
#if HWY_TARGET <= HWY_AVX3
  return Mask256<float>{_mm256_fpclass_ps_mask(v.raw, 0x81)};
#else
  return Mask256<float>{_mm256_cmp_ps(v.raw, v.raw, _CMP_UNORD_Q)};
#endif
}
HWY_API Mask256<double> IsNaN(const Vec256<double> v) {
#if HWY_TARGET <= HWY_AVX3
  return Mask256<double>{_mm256_fpclass_pd_mask(v.raw, 0x81)};
#else
  return Mask256<double>{_mm256_cmp_pd(v.raw, v.raw, _CMP_UNORD_Q)};
#endif
}

#if HWY_TARGET <= HWY_AVX3

HWY_API Mask256<float> IsInf(const Vec256<float> v) {
  return Mask256<float>{_mm256_fpclass_ps_mask(v.raw, 0x18)};
}
HWY_API Mask256<double> IsInf(const Vec256<double> v) {
  return Mask256<double>{_mm256_fpclass_pd_mask(v.raw, 0x18)};
}

HWY_API Mask256<float> IsFinite(const Vec256<float> v) {
  // fpclass doesn't have a flag for positive, so we have to check for inf/NaN
  // and negate the mask.
  return Not(Mask256<float>{_mm256_fpclass_ps_mask(v.raw, 0x99)});
}
HWY_API Mask256<double> IsFinite(const Vec256<double> v) {
  return Not(Mask256<double>{_mm256_fpclass_pd_mask(v.raw, 0x99)});
}

#else

template <typename T>
HWY_API Mask256<T> IsInf(const Vec256<T> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Full256<T> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T>
HWY_API Mask256<T> IsFinite(const Vec256<T> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // Shift left to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater). MSVC seems to generate
  // incorrect code if we instead add vu + vu.
  const VFromD<decltype(di)> exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(ShiftLeft<1>(vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

#endif  // HWY_TARGET <= HWY_AVX3

// ================================================== MEMORY

// ------------------------------ Load

template <typename T>
HWY_API Vec256<T> Load(Full256<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec256<T>{
      _mm256_load_si256(reinterpret_cast<const __m256i*>(aligned))};
}
HWY_API Vec256<float> Load(Full256<float> /* tag */,
                           const float* HWY_RESTRICT aligned) {
  return Vec256<float>{_mm256_load_ps(aligned)};
}
HWY_API Vec256<double> Load(Full256<double> /* tag */,
                            const double* HWY_RESTRICT aligned) {
  return Vec256<double>{_mm256_load_pd(aligned)};
}

template <typename T>
HWY_API Vec256<T> LoadU(Full256<T> /* tag */, const T* HWY_RESTRICT p) {
  return Vec256<T>{_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p))};
}
HWY_API Vec256<float> LoadU(Full256<float> /* tag */,
                            const float* HWY_RESTRICT p) {
  return Vec256<float>{_mm256_loadu_ps(p)};
}
HWY_API Vec256<double> LoadU(Full256<double> /* tag */,
                             const double* HWY_RESTRICT p) {
  return Vec256<double>{_mm256_loadu_pd(p)};
}

// ------------------------------ MaskedLoad

#if HWY_TARGET <= HWY_AVX3

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec256<T>{_mm256_maskz_loadu_epi8(m.raw, p)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec256<T>{_mm256_maskz_loadu_epi16(m.raw, p)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec256<T>{_mm256_maskz_loadu_epi32(m.raw, p)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec256<T>{_mm256_maskz_loadu_epi64(m.raw, p)};
}

HWY_API Vec256<float> MaskedLoad(Mask256<float> m, Full256<float> /* tag */,
                                 const float* HWY_RESTRICT p) {
  return Vec256<float>{_mm256_maskz_loadu_ps(m.raw, p)};
}

HWY_API Vec256<double> MaskedLoad(Mask256<double> m, Full256<double> /* tag */,
                                  const double* HWY_RESTRICT p) {
  return Vec256<double>{_mm256_maskz_loadu_pd(m.raw, p)};
}

#else  //  AVX2

// There is no maskload_epi8/16, so blend instead.
template <typename T, hwy::EnableIf<sizeof(T) <= 2>* = nullptr>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> d,
                             const T* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<const int*>(p);  // NOLINT
  return Vec256<T>{_mm256_maskload_epi32(pi, m.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<const long long*>(p);  // NOLINT
  return Vec256<T>{_mm256_maskload_epi64(pi, m.raw)};
}

HWY_API Vec256<float> MaskedLoad(Mask256<float> m, Full256<float> d,
                                 const float* HWY_RESTRICT p) {
  const Vec256<int32_t> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  return Vec256<float>{_mm256_maskload_ps(p, mi.raw)};
}

HWY_API Vec256<double> MaskedLoad(Mask256<double> m, Full256<double> d,
                                  const double* HWY_RESTRICT p) {
  const Vec256<int64_t> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  return Vec256<double>{_mm256_maskload_pd(p, mi.raw)};
}

#endif

// ------------------------------ LoadDup128

// Loads 128 bit and duplicates into both 128-bit halves. This avoids the
// 3-cycle cost of moving data between 128-bit halves and avoids port 5.
template <typename T>
HWY_API Vec256<T> LoadDup128(Full256<T> /* tag */, const T* HWY_RESTRICT p) {
#if HWY_COMPILER_MSVC && HWY_COMPILER_MSVC < 1931
  // Workaround for incorrect results with _mm256_broadcastsi128_si256. Note
  // that MSVC also lacks _mm256_zextsi128_si256, but cast (which leaves the
  // upper half undefined) is fine because we're overwriting that anyway.
  // This workaround seems in turn to generate incorrect code in MSVC 2022
  // (19.31), so use broadcastsi128 there.
  const __m128i v128 = LoadU(Full128<T>(), p).raw;
  return Vec256<T>{
      _mm256_inserti128_si256(_mm256_castsi128_si256(v128), v128, 1)};
#else
  return Vec256<T>{_mm256_broadcastsi128_si256(LoadU(Full128<T>(), p).raw)};
#endif
}
HWY_API Vec256<float> LoadDup128(Full256<float> /* tag */,
                                 const float* const HWY_RESTRICT p) {
#if HWY_COMPILER_MSVC && HWY_COMPILER_MSVC < 1931
  const __m128 v128 = LoadU(Full128<float>(), p).raw;
  return Vec256<float>{
      _mm256_insertf128_ps(_mm256_castps128_ps256(v128), v128, 1)};
#else
  return Vec256<float>{_mm256_broadcast_ps(reinterpret_cast<const __m128*>(p))};
#endif
}
HWY_API Vec256<double> LoadDup128(Full256<double> /* tag */,
                                  const double* const HWY_RESTRICT p) {
#if HWY_COMPILER_MSVC && HWY_COMPILER_MSVC < 1931
  const __m128d v128 = LoadU(Full128<double>(), p).raw;
  return Vec256<double>{
      _mm256_insertf128_pd(_mm256_castpd128_pd256(v128), v128, 1)};
#else
  return Vec256<double>{
      _mm256_broadcast_pd(reinterpret_cast<const __m128d*>(p))};
#endif
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec256<T> v, Full256<T> /* tag */, T* HWY_RESTRICT aligned) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(aligned), v.raw);
}
HWY_API void Store(const Vec256<float> v, Full256<float> /* tag */,
                   float* HWY_RESTRICT aligned) {
  _mm256_store_ps(aligned, v.raw);
}
HWY_API void Store(const Vec256<double> v, Full256<double> /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm256_store_pd(aligned, v.raw);
}

template <typename T>
HWY_API void StoreU(Vec256<T> v, Full256<T> /* tag */, T* HWY_RESTRICT p) {
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v.raw);
}
HWY_API void StoreU(const Vec256<float> v, Full256<float> /* tag */,
                    float* HWY_RESTRICT p) {
  _mm256_storeu_ps(p, v.raw);
}
HWY_API void StoreU(const Vec256<double> v, Full256<double> /* tag */,
                    double* HWY_RESTRICT p) {
  _mm256_storeu_pd(p, v.raw);
}

// ------------------------------ BlendedStore

#if HWY_TARGET <= HWY_AVX3

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm256_mask_storeu_epi8(p, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm256_mask_storeu_epi16(p, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm256_mask_storeu_epi32(p, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> /* tag */,
                          T* HWY_RESTRICT p) {
  _mm256_mask_storeu_epi64(p, m.raw, v.raw);
}

HWY_API void BlendedStore(Vec256<float> v, Mask256<float> m,
                          Full256<float> /* tag */, float* HWY_RESTRICT p) {
  _mm256_mask_storeu_ps(p, m.raw, v.raw);
}

HWY_API void BlendedStore(Vec256<double> v, Mask256<double> m,
                          Full256<double> /* tag */, double* HWY_RESTRICT p) {
  _mm256_mask_storeu_pd(p, m.raw, v.raw);
}

#else  //  AVX2

// Intel SDM says "No AC# reported for any mask bit combinations". However, AMD
// allows AC# if "Alignment checking enabled and: 256-bit memory operand not
// 32-byte aligned". Fortunately AC# is not enabled by default and requires both
// OS support (CR0) and the application to set rflags.AC. We assume these remain
// disabled because x86/x64 code and compiler output often contain misaligned
// scalar accesses, which would also fault.
//
// Caveat: these are slow on AMD Jaguar/Bulldozer.

template <typename T, hwy::EnableIf<sizeof(T) <= 2>* = nullptr>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                          T* HWY_RESTRICT p) {
  // There is no maskload_epi8/16. Blending is also unsafe because loading a
  // full vector that crosses the array end causes asan faults. Resort to scalar
  // code; the caller should instead use memcpy, assuming m is FirstN(d, n).
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  alignas(32) TU buf[32 / sizeof(T)];
  alignas(32) TU mask[32 / sizeof(T)];
  Store(BitCast(du, v), du, buf);
  Store(BitCast(du, VecFromMask(d, m)), du, mask);
  for (size_t i = 0; i < 32 / sizeof(T); ++i) {
    if (mask[i]) {
      CopyBytes<sizeof(T)>(buf + i, p + i);
    }
  }
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> /* tag */,
                          T* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<int*>(p);  // NOLINT
  _mm256_maskstore_epi32(pi, m.raw, v.raw);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> /* tag */,
                          T* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<long long*>(p);  // NOLINT
  _mm256_maskstore_epi64(pi, m.raw, v.raw);
}

HWY_API void BlendedStore(Vec256<float> v, Mask256<float> m, Full256<float> d,
                          float* HWY_RESTRICT p) {
  const Vec256<int32_t> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  _mm256_maskstore_ps(p, mi.raw, v.raw);
}

HWY_API void BlendedStore(Vec256<double> v, Mask256<double> m,
                          Full256<double> d, double* HWY_RESTRICT p) {
  const Vec256<int64_t> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  _mm256_maskstore_pd(p, mi.raw, v.raw);
}

#endif

// ------------------------------ Non-temporal stores

template <typename T>
HWY_API void Stream(Vec256<T> v, Full256<T> /* tag */,
                    T* HWY_RESTRICT aligned) {
  _mm256_stream_si256(reinterpret_cast<__m256i*>(aligned), v.raw);
}
HWY_API void Stream(const Vec256<float> v, Full256<float> /* tag */,
                    float* HWY_RESTRICT aligned) {
  _mm256_stream_ps(aligned, v.raw);
}
HWY_API void Stream(const Vec256<double> v, Full256<double> /* tag */,
                    double* HWY_RESTRICT aligned) {
  _mm256_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

#if HWY_TARGET <= HWY_AVX3
namespace detail {

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<4> /* tag */, Vec256<T> v,
                              Full256<T> /* tag */, T* HWY_RESTRICT base,
                              const Vec256<int32_t> offset) {
  _mm256_i32scatter_epi32(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<4> /* tag */, Vec256<T> v,
                             Full256<T> /* tag */, T* HWY_RESTRICT base,
                             const Vec256<int32_t> index) {
  _mm256_i32scatter_epi32(base, index.raw, v.raw, 4);
}

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<8> /* tag */, Vec256<T> v,
                              Full256<T> /* tag */, T* HWY_RESTRICT base,
                              const Vec256<int64_t> offset) {
  _mm256_i64scatter_epi64(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<8> /* tag */, Vec256<T> v,
                             Full256<T> /* tag */, T* HWY_RESTRICT base,
                             const Vec256<int64_t> index) {
  _mm256_i64scatter_epi64(base, index.raw, v.raw, 8);
}

}  // namespace detail

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                           const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::ScatterOffset(hwy::SizeTag<sizeof(T)>(), v, d, base, offset);
}
template <typename T, typename Index>
HWY_API void ScatterIndex(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                          const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::ScatterIndex(hwy::SizeTag<sizeof(T)>(), v, d, base, index);
}

HWY_API void ScatterOffset(Vec256<float> v, Full256<float> /* tag */,
                           float* HWY_RESTRICT base,
                           const Vec256<int32_t> offset) {
  _mm256_i32scatter_ps(base, offset.raw, v.raw, 1);
}
HWY_API void ScatterIndex(Vec256<float> v, Full256<float> /* tag */,
                          float* HWY_RESTRICT base,
                          const Vec256<int32_t> index) {
  _mm256_i32scatter_ps(base, index.raw, v.raw, 4);
}

HWY_API void ScatterOffset(Vec256<double> v, Full256<double> /* tag */,
                           double* HWY_RESTRICT base,
                           const Vec256<int64_t> offset) {
  _mm256_i64scatter_pd(base, offset.raw, v.raw, 1);
}
HWY_API void ScatterIndex(Vec256<double> v, Full256<double> /* tag */,
                          double* HWY_RESTRICT base,
                          const Vec256<int64_t> index) {
  _mm256_i64scatter_pd(base, index.raw, v.raw, 8);
}

#else

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                           const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  constexpr size_t N = 32 / sizeof(T);
  alignas(32) T lanes[N];
  Store(v, d, lanes);

  alignas(32) Offset offset_lanes[N];
  Store(offset, Full256<Offset>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, typename Index>
HWY_API void ScatterIndex(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                          const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  constexpr size_t N = 32 / sizeof(T);
  alignas(32) T lanes[N];
  Store(v, d, lanes);

  alignas(32) Index index_lanes[N];
  Store(index, Full256<Index>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

#endif

// ------------------------------ Gather

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> GatherOffset(hwy::SizeTag<4> /* tag */,
                                  Full256<T> /* tag */,
                                  const T* HWY_RESTRICT base,
                                  const Vec256<int32_t> offset) {
  return Vec256<T>{_mm256_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), offset.raw, 1)};
}
template <typename T>
HWY_INLINE Vec256<T> GatherIndex(hwy::SizeTag<4> /* tag */,
                                 Full256<T> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec256<int32_t> index) {
  return Vec256<T>{_mm256_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), index.raw, 4)};
}

template <typename T>
HWY_INLINE Vec256<T> GatherOffset(hwy::SizeTag<8> /* tag */,
                                  Full256<T> /* tag */,
                                  const T* HWY_RESTRICT base,
                                  const Vec256<int64_t> offset) {
  return Vec256<T>{_mm256_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), offset.raw, 1)};
}
template <typename T>
HWY_INLINE Vec256<T> GatherIndex(hwy::SizeTag<8> /* tag */,
                                 Full256<T> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec256<int64_t> index) {
  return Vec256<T>{_mm256_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), index.raw, 8)};
}

}  // namespace detail

template <typename T, typename Offset>
HWY_API Vec256<T> GatherOffset(Full256<T> d, const T* HWY_RESTRICT base,
                               const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::GatherOffset(hwy::SizeTag<sizeof(T)>(), d, base, offset);
}
template <typename T, typename Index>
HWY_API Vec256<T> GatherIndex(Full256<T> d, const T* HWY_RESTRICT base,
                              const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::GatherIndex(hwy::SizeTag<sizeof(T)>(), d, base, index);
}

HWY_API Vec256<float> GatherOffset(Full256<float> /* tag */,
                                   const float* HWY_RESTRICT base,
                                   const Vec256<int32_t> offset) {
  return Vec256<float>{_mm256_i32gather_ps(base, offset.raw, 1)};
}
HWY_API Vec256<float> GatherIndex(Full256<float> /* tag */,
                                  const float* HWY_RESTRICT base,
                                  const Vec256<int32_t> index) {
  return Vec256<float>{_mm256_i32gather_ps(base, index.raw, 4)};
}

HWY_API Vec256<double> GatherOffset(Full256<double> /* tag */,
                                    const double* HWY_RESTRICT base,
                                    const Vec256<int64_t> offset) {
  return Vec256<double>{_mm256_i64gather_pd(base, offset.raw, 1)};
}
HWY_API Vec256<double> GatherIndex(Full256<double> /* tag */,
                                   const double* HWY_RESTRICT base,
                                   const Vec256<int64_t> index) {
  return Vec256<double>{_mm256_i64gather_pd(base, index.raw, 8)};
}

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE

// ------------------------------ LowerHalf

template <typename T>
HWY_API Vec128<T> LowerHalf(Full128<T> /* tag */, Vec256<T> v) {
  return Vec128<T>{_mm256_castsi256_si128(v.raw)};
}
HWY_API Vec128<float> LowerHalf(Full128<float> /* tag */, Vec256<float> v) {
  return Vec128<float>{_mm256_castps256_ps128(v.raw)};
}
HWY_API Vec128<double> LowerHalf(Full128<double> /* tag */, Vec256<double> v) {
  return Vec128<double>{_mm256_castpd256_pd128(v.raw)};
}

template <typename T>
HWY_API Vec128<T> LowerHalf(Vec256<T> v) {
  return LowerHalf(Full128<T>(), v);
}

// ------------------------------ UpperHalf

template <typename T>
HWY_API Vec128<T> UpperHalf(Full128<T> /* tag */, Vec256<T> v) {
  return Vec128<T>{_mm256_extracti128_si256(v.raw, 1)};
}
HWY_API Vec128<float> UpperHalf(Full128<float> /* tag */, Vec256<float> v) {
  return Vec128<float>{_mm256_extractf128_ps(v.raw, 1)};
}
HWY_API Vec128<double> UpperHalf(Full128<double> /* tag */, Vec256<double> v) {
  return Vec128<double>{_mm256_extractf128_pd(v.raw, 1)};
}

// ------------------------------ ExtractLane (Store)
template <typename T>
HWY_API T ExtractLane(const Vec256<T> v, size_t i) {
  const Full256<T> d;
  HWY_DASSERT(i < Lanes(d));
  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (Store)
template <typename T>
HWY_API Vec256<T> InsertLane(const Vec256<T> v, size_t i, T t) {
  const Full256<T> d;
  HWY_DASSERT(i < Lanes(d));
  alignas(64) T lanes[64 / sizeof(T)];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec256<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ZeroExtendVector

// Unfortunately the initial _mm256_castsi128_si256 intrinsic leaves the upper
// bits undefined. Although it makes sense for them to be zero (VEX encoded
// 128-bit instructions zero the upper lanes to avoid large penalties), a
// compiler could decide to optimize out code that relies on this.
//
// The newer _mm256_zextsi128_si256 intrinsic fixes this by specifying the
// zeroing, but it is not available on MSVC until 15.7 nor GCC until 10.1. For
// older GCC, we can still obtain the desired code thanks to pattern
// recognition; note that the expensive insert instruction is not actually
// generated, see https://gcc.godbolt.org/z/1MKGaP.

#if !defined(HWY_HAVE_ZEXT)
#if (HWY_COMPILER_MSVC && HWY_COMPILER_MSVC >= 1915) ||  \
    (HWY_COMPILER_CLANG && HWY_COMPILER_CLANG >= 500) || \
    (HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL >= 1000)
#define HWY_HAVE_ZEXT 1
#else
#define HWY_HAVE_ZEXT 0
#endif
#endif  // defined(HWY_HAVE_ZEXT)

template <typename T>
HWY_API Vec256<T> ZeroExtendVector(Full256<T> /* tag */, Vec128<T> lo) {
#if HWY_HAVE_ZEXT
return Vec256<T>{_mm256_zextsi128_si256(lo.raw)};
#else
  return Vec256<T>{_mm256_inserti128_si256(_mm256_setzero_si256(), lo.raw, 0)};
#endif
}
HWY_API Vec256<float> ZeroExtendVector(Full256<float> /* tag */,
                                       Vec128<float> lo) {
#if HWY_HAVE_ZEXT
  return Vec256<float>{_mm256_zextps128_ps256(lo.raw)};
#else
  return Vec256<float>{_mm256_insertf128_ps(_mm256_setzero_ps(), lo.raw, 0)};
#endif
}
HWY_API Vec256<double> ZeroExtendVector(Full256<double> /* tag */,
                                        Vec128<double> lo) {
#if HWY_HAVE_ZEXT
  return Vec256<double>{_mm256_zextpd128_pd256(lo.raw)};
#else
  return Vec256<double>{_mm256_insertf128_pd(_mm256_setzero_pd(), lo.raw, 0)};
#endif
}

// ------------------------------ Combine

template <typename T>
HWY_API Vec256<T> Combine(Full256<T> d, Vec128<T> hi, Vec128<T> lo) {
  const auto lo256 = ZeroExtendVector(d, lo);
  return Vec256<T>{_mm256_inserti128_si256(lo256.raw, hi.raw, 1)};
}
HWY_API Vec256<float> Combine(Full256<float> d, Vec128<float> hi,
                              Vec128<float> lo) {
  const auto lo256 = ZeroExtendVector(d, lo);
  return Vec256<float>{_mm256_insertf128_ps(lo256.raw, hi.raw, 1)};
}
HWY_API Vec256<double> Combine(Full256<double> d, Vec128<double> hi,
                               Vec128<double> lo) {
  const auto lo256 = ZeroExtendVector(d, lo);
  return Vec256<double>{_mm256_insertf128_pd(lo256.raw, hi.raw, 1)};
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(Full256<T> /* tag */, const Vec256<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  // This is the same operation as _mm256_bslli_epi128.
  return Vec256<T>{_mm256_slli_si256(v.raw, kBytes)};
}

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(const Vec256<T> v) {
  return ShiftLeftBytes<kBytes>(Full256<T>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(const Vec256<T> v) {
  return ShiftLeftLanes<kLanes>(Full256<T>(), v);
}

// ------------------------------ ShiftRightBytes

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftRightBytes(Full256<T> /* tag */, const Vec256<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  // This is the same operation as _mm256_bsrli_epi128.
  return Vec256<T>{_mm256_srli_si256(v.raw, kBytes)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T>
HWY_API Vec256<T> ShiftRightLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ CombineShiftRightBytes

// Extracts 128 bits from <hi, lo> by skipping the least-significant kBytes.
template <int kBytes, typename T, class V = Vec256<T>>
HWY_API V CombineShiftRightBytes(Full256<T> d, V hi, V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec256<uint8_t>{_mm256_alignr_epi8(
                        BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)});
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane>
HWY_API Vec256<uint16_t> Broadcast(const Vec256<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m256i lo = _mm256_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec256<uint16_t>{_mm256_unpacklo_epi64(lo, lo)};
  } else {
    const __m256i hi =
        _mm256_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec256<uint16_t>{_mm256_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec256<uint32_t> Broadcast(const Vec256<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane>
HWY_API Vec256<uint64_t> Broadcast(const Vec256<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<uint64_t>{_mm256_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Signed
template <int kLane>
HWY_API Vec256<int16_t> Broadcast(const Vec256<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m256i lo = _mm256_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec256<int16_t>{_mm256_unpacklo_epi64(lo, lo)};
  } else {
    const __m256i hi =
        _mm256_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec256<int16_t>{_mm256_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec256<int32_t> Broadcast(const Vec256<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane>
HWY_API Vec256<int64_t> Broadcast(const Vec256<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<int64_t>{_mm256_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Float
template <int kLane>
HWY_API Vec256<float> Broadcast(Vec256<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x55 * kLane)};
}
template <int kLane>
HWY_API Vec256<double> Broadcast(const Vec256<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<double>{_mm256_shuffle_pd(v.raw, v.raw, 15 * kLane)};
}

// ------------------------------ Hard-coded shuffles

// Notation: let Vec256<int32_t> have lanes 7,6,5,4,3,2,1,0 (0 is
// least-significant). Shuffle0321 rotates four-lane blocks one lane to the
// right (the previous least-significant lane is now most-significant =>
// 47650321). These could also be implemented via CombineShiftRightBytes but
// the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle2301(const Vec256<T> v) {
  return Vec256<T>{_mm256_shuffle_epi32(v.raw, 0xB1)};
}
HWY_API Vec256<float> Shuffle2301(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0xB1)};
}

namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle2301(const Vec256<T> a, const Vec256<T> b) {
  const Full256<T> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(2, 3, 0, 1);
  return BitCast(d, Vec256<float>{_mm256_shuffle_ps(BitCast(df, a).raw,
                                                    BitCast(df, b).raw, m)});
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle1230(const Vec256<T> a, const Vec256<T> b) {
  const Full256<T> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(1, 2, 3, 0);
  return BitCast(d, Vec256<float>{_mm256_shuffle_ps(BitCast(df, a).raw,
                                                    BitCast(df, b).raw, m)});
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Shuffle3012(const Vec256<T> a, const Vec256<T> b) {
  const Full256<T> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(3, 0, 1, 2);
  return BitCast(d, Vec256<float>{_mm256_shuffle_ps(BitCast(df, a).raw,
                                                    BitCast(df, b).raw, m)});
}

}  // namespace detail

// Swap 64-bit halves
HWY_API Vec256<uint32_t> Shuffle1032(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<int32_t> Shuffle1032(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<float> Shuffle1032(const Vec256<float> v) {
  // Shorter encoding than _mm256_permute_ps.
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x4E)};
}
HWY_API Vec256<uint64_t> Shuffle01(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<int64_t> Shuffle01(const Vec256<int64_t> v) {
  return Vec256<int64_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<double> Shuffle01(const Vec256<double> v) {
  // Shorter encoding than _mm256_permute_pd.
  return Vec256<double>{_mm256_shuffle_pd(v.raw, v.raw, 5)};
}

// Rotate right 32 bits
HWY_API Vec256<uint32_t> Shuffle0321(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec256<int32_t> Shuffle0321(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec256<float> Shuffle0321(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x39)};
}
// Rotate left 32 bits
HWY_API Vec256<uint32_t> Shuffle2103(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec256<int32_t> Shuffle2103(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec256<float> Shuffle2103(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x93)};
}

// Reverse
HWY_API Vec256<uint32_t> Shuffle0123(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec256<int32_t> Shuffle0123(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec256<float> Shuffle0123(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x1B)};
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T>
struct Indices256 {
  __m256i raw;
};

// Native 8x32 instruction: indices remain unchanged
template <typename T, typename TI, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Indices256<T> IndicesFromVec(Full256<T> /* tag */, Vec256<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Full256<TI> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(32 / sizeof(T))))));
#endif
  return Indices256<T>{vec.raw};
}

// 64-bit lanes: convert indices to 8x32 unless AVX3 is available
template <typename T, typename TI, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Indices256<T> IndicesFromVec(Full256<T> d, Vec256<TI> idx64) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
  const Rebind<TI, decltype(d)> di;
  (void)di;  // potentially unused
#if HWY_IS_DEBUG_BUILD
  HWY_DASSERT(AllFalse(di, Lt(idx64, Zero(di))) &&
              AllTrue(di, Lt(idx64, Set(di, static_cast<TI>(32 / sizeof(T))))));
#endif

#if HWY_TARGET <= HWY_AVX3
  (void)d;
  return Indices256<T>{idx64.raw};
#else
  const Repartition<float, decltype(d)> df;  // 32-bit!
  // Replicate 64-bit index into upper 32 bits
  const Vec256<TI> dup =
      BitCast(di, Vec256<float>{_mm256_moveldup_ps(BitCast(df, idx64).raw)});
  // For each idx64 i, idx32 are 2*i and 2*i+1.
  const Vec256<TI> idx32 = dup + dup + Set(di, TI(1) << 32);
  return Indices256<T>{idx32.raw};
#endif
}

template <typename T, typename TI>
HWY_API Indices256<T> SetTableIndices(const Full256<T> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
  return Vec256<T>{_mm256_permutevar8x32_epi32(v.raw, idx.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<T>{_mm256_permutexvar_epi64(idx.raw, v.raw)};
#else
  return Vec256<T>{_mm256_permutevar8x32_epi32(v.raw, idx.raw)};
#endif
}

HWY_API Vec256<float> TableLookupLanes(const Vec256<float> v,
                                       const Indices256<float> idx) {
  return Vec256<float>{_mm256_permutevar8x32_ps(v.raw, idx.raw)};
}

HWY_API Vec256<double> TableLookupLanes(const Vec256<double> v,
                                        const Indices256<double> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<double>{_mm256_permutexvar_pd(idx.raw, v.raw)};
#else
  const Full256<double> df;
  const Full256<uint64_t> du;
  return BitCast(df, Vec256<uint64_t>{_mm256_permutevar8x32_epi32(
                         BitCast(du, v).raw, idx.raw)});
#endif
}

// ------------------------------ SwapAdjacentBlocks

template <typename T>
HWY_API Vec256<T> SwapAdjacentBlocks(Vec256<T> v) {
  return Vec256<T>{_mm256_permute2x128_si256(v.raw, v.raw, 0x01)};
}

HWY_API Vec256<float> SwapAdjacentBlocks(Vec256<float> v) {
  return Vec256<float>{_mm256_permute2f128_ps(v.raw, v.raw, 0x01)};
}

HWY_API Vec256<double> SwapAdjacentBlocks(Vec256<double> v) {
  return Vec256<double>{_mm256_permute2f128_pd(v.raw, v.raw, 0x01)};
}

// ------------------------------ Reverse (RotateRight)

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Reverse(Full256<T> d, const Vec256<T> v) {
  alignas(32) constexpr int32_t kReverse[8] = {7, 6, 5, 4, 3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse(Full256<T> d, const Vec256<T> v) {
  alignas(32) constexpr int64_t kReverse[4] = {3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> Reverse(Full256<T> d, const Vec256<T> v) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToSigned<decltype(d)> di;
  alignas(32) constexpr int16_t kReverse[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                                7,  6,  5,  4,  3,  2,  1, 0};
  const Vec256<int16_t> idx = Load(di, kReverse);
  return BitCast(d, Vec256<int16_t>{
                        _mm256_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  const Vec256<uint32_t> rev32 = Reverse(du32, BitCast(du32, v));
  return BitCast(d, RotateRight<16>(rev32));
#endif
}

// ------------------------------ Reverse2

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> Reverse2(Full256<T> d, const Vec256<T> v) {
  const Full256<uint32_t> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Reverse2(Full256<T> /* tag */, const Vec256<T> v) {
  return Shuffle2301(v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse2(Full256<T> /* tag */, const Vec256<T> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4 (SwapAdjacentBlocks)

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> Reverse4(Full256<T> d, const Vec256<T> v) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToSigned<decltype(d)> di;
  alignas(32) constexpr int16_t kReverse4[16] = {3,  2,  1, 0, 7,  6,  5,  4,
                                                 11, 10, 9, 8, 15, 14, 13, 12};
  const Vec256<int16_t> idx = Load(di, kReverse4);
  return BitCast(d, Vec256<int16_t>{
                        _mm256_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<decltype(d)> dw;
  return Reverse2(d, BitCast(d, Shuffle2301(BitCast(dw, v))));
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Reverse4(Full256<T> /* tag */, const Vec256<T> v) {
  return Shuffle0123(v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse4(Full256<T> /* tag */, const Vec256<T> v) {
  // Could also use _mm256_permute4x64_epi64.
  return SwapAdjacentBlocks(Shuffle01(v));
}

// ------------------------------ Reverse8

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> Reverse8(Full256<T> d, const Vec256<T> v) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToSigned<decltype(d)> di;
  alignas(32) constexpr int16_t kReverse8[16] = {7,  6,  5,  4,  3,  2,  1, 0,
                                                 15, 14, 13, 12, 11, 10, 9, 8};
  const Vec256<int16_t> idx = Load(di, kReverse8);
  return BitCast(d, Vec256<int16_t>{
                        _mm256_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<decltype(d)> dw;
  return Reverse2(d, BitCast(d, Shuffle0123(BitCast(dw, v))));
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Reverse8(Full256<T> d, const Vec256<T> v) {
  return Reverse(d, v);
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Reverse8(Full256<T> /* tag */, const Vec256<T> /* v */) {
  HWY_ASSERT(0);  // AVX2 does not have 8 64-bit lanes
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

HWY_API Vec256<uint8_t> InterleaveLower(const Vec256<uint8_t> a,
                                        const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> InterleaveLower(const Vec256<uint16_t> a,
                                         const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> InterleaveLower(const Vec256<uint32_t> a,
                                         const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> InterleaveLower(const Vec256<uint64_t> a,
                                         const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec256<int8_t> InterleaveLower(const Vec256<int8_t> a,
                                       const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> InterleaveLower(const Vec256<int16_t> a,
                                        const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> InterleaveLower(const Vec256<int32_t> a,
                                        const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> InterleaveLower(const Vec256<int64_t> a,
                                        const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec256<float> InterleaveLower(const Vec256<float> a,
                                      const Vec256<float> b) {
  return Vec256<float>{_mm256_unpacklo_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> InterleaveLower(const Vec256<double> a,
                                       const Vec256<double> b) {
  return Vec256<double>{_mm256_unpacklo_pd(a.raw, b.raw)};
}

// ------------------------------ InterleaveUpper

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec256<uint8_t> InterleaveUpper(const Vec256<uint8_t> a,
                                        const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> InterleaveUpper(const Vec256<uint16_t> a,
                                         const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> InterleaveUpper(const Vec256<uint32_t> a,
                                         const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> InterleaveUpper(const Vec256<uint64_t> a,
                                         const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec256<int8_t> InterleaveUpper(const Vec256<int8_t> a,
                                       const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> InterleaveUpper(const Vec256<int16_t> a,
                                        const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> InterleaveUpper(const Vec256<int32_t> a,
                                        const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> InterleaveUpper(const Vec256<int64_t> a,
                                        const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec256<float> InterleaveUpper(const Vec256<float> a,
                                      const Vec256<float> b) {
  return Vec256<float>{_mm256_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> InterleaveUpper(const Vec256<double> a,
                                       const Vec256<double> b) {
  return Vec256<double>{_mm256_unpackhi_pd(a.raw, b.raw)};
}

}  // namespace detail

template <typename T, class V = Vec256<T>>
HWY_API V InterleaveUpper(Full256<T> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec256<TW> ZipLower(Vec256<T> a, Vec256<T> b) {
  return BitCast(Full256<TW>(), InterleaveLower(a, b));
}
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec256<TW> ZipLower(Full256<TW> dw, Vec256<T> a, Vec256<T> b) {
  return BitCast(dw, InterleaveLower(a, b));
}

template <typename T, typename TW = MakeWide<T>>
HWY_API Vec256<TW> ZipUpper(Full256<TW> dw, Vec256<T> a, Vec256<T> b) {
  return BitCast(dw, InterleaveUpper(Full256<T>(), a, b));
}

// ------------------------------ Blocks (LowerHalf, ZeroExtendVector)

// _mm256_broadcastsi128_si256 has 7 cycle latency on ICL.
// _mm256_permute2x128_si256 is slow on Zen1 (8 uops), so we avoid it (at no
// extra cost) for LowerLower and UpperLower.

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec256<T> ConcatLowerLower(Full256<T> d, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  const Half<decltype(d)> d2;
  return Vec256<T>{_mm256_inserti128_si256(lo.raw, LowerHalf(d2, hi).raw, 1)};
}
HWY_API Vec256<float> ConcatLowerLower(Full256<float> d, const Vec256<float> hi,
                                       const Vec256<float> lo) {
  const Half<decltype(d)> d2;
  return Vec256<float>{_mm256_insertf128_ps(lo.raw, LowerHalf(d2, hi).raw, 1)};
}
HWY_API Vec256<double> ConcatLowerLower(Full256<double> d,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  const Half<decltype(d)> d2;
  return Vec256<double>{_mm256_insertf128_pd(lo.raw, LowerHalf(d2, hi).raw, 1)};
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves / swap blocks)
template <typename T>
HWY_API Vec256<T> ConcatLowerUpper(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{_mm256_permute2x128_si256(lo.raw, hi.raw, 0x21)};
}
HWY_API Vec256<float> ConcatLowerUpper(Full256<float> /* tag */,
                                       const Vec256<float> hi,
                                       const Vec256<float> lo) {
  return Vec256<float>{_mm256_permute2f128_ps(lo.raw, hi.raw, 0x21)};
}
HWY_API Vec256<double> ConcatLowerUpper(Full256<double> /* tag */,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  return Vec256<double>{_mm256_permute2f128_pd(lo.raw, hi.raw, 0x21)};
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <typename T>
HWY_API Vec256<T> ConcatUpperLower(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{_mm256_blend_epi32(hi.raw, lo.raw, 0x0F)};
}
HWY_API Vec256<float> ConcatUpperLower(Full256<float> /* tag */,
                                       const Vec256<float> hi,
                                       const Vec256<float> lo) {
  return Vec256<float>{_mm256_blend_ps(hi.raw, lo.raw, 0x0F)};
}
HWY_API Vec256<double> ConcatUpperLower(Full256<double> /* tag */,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  return Vec256<double>{_mm256_blend_pd(hi.raw, lo.raw, 3)};
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <typename T>
HWY_API Vec256<T> ConcatUpperUpper(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{_mm256_permute2x128_si256(lo.raw, hi.raw, 0x31)};
}
HWY_API Vec256<float> ConcatUpperUpper(Full256<float> /* tag */,
                                       const Vec256<float> hi,
                                       const Vec256<float> lo) {
  return Vec256<float>{_mm256_permute2f128_ps(lo.raw, hi.raw, 0x31)};
}
HWY_API Vec256<double> ConcatUpperUpper(Full256<double> /* tag */,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  return Vec256<double>{_mm256_permute2f128_pd(lo.raw, hi.raw, 0x31)};
}

// ------------------------------ ConcatOdd

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ConcatOdd(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET == HWY_AVX3_DL
  alignas(32) constexpr uint8_t kIdx[32] = {
      1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,
      33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63};
  return BitCast(d, Vec256<uint16_t>{_mm256_mask2_permutex2var_epi8(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask32{0xFFFFFFFFu}, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Unsigned 8-bit shift so we can pack.
  const Vec256<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec256<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  const __m256i u8 = _mm256_packus_epi16(uL.raw, uH.raw);
  return Vec256<T>{_mm256_permute4x64_epi64(u8, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> ConcatOdd(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(32) constexpr uint16_t kIdx[16] = {1,  3,  5,  7,  9,  11, 13, 15,
                                             17, 19, 21, 23, 25, 27, 29, 31};
  return BitCast(d, Vec256<uint16_t>{_mm256_mask2_permutex2var_epi16(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask16{0xFFFF}, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Unsigned 16-bit shift so we can pack.
  const Vec256<uint32_t> uH = ShiftRight<16>(BitCast(dw, hi));
  const Vec256<uint32_t> uL = ShiftRight<16>(BitCast(dw, lo));
  const __m256i u16 = _mm256_packus_epi32(uL.raw, uH.raw);
  return Vec256<T>{_mm256_permute4x64_epi64(u16, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> ConcatOdd(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(32) constexpr uint32_t kIdx[8] = {1, 3, 5, 7, 9, 11, 13, 15};
  return BitCast(d, Vec256<uint32_t>{_mm256_mask2_permutex2var_epi32(
                        BitCast(du, lo).raw, Load(du, kIdx).raw, __mmask8{0xFF},
                        BitCast(du, hi).raw)});
#else
  const RebindToFloat<decltype(d)> df;
  const Vec256<float> v3131{_mm256_shuffle_ps(
      BitCast(df, lo).raw, BitCast(df, hi).raw, _MM_SHUFFLE(3, 1, 3, 1))};
  return Vec256<T>{_mm256_permute4x64_epi64(BitCast(du, v3131).raw,
                                            _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

HWY_API Vec256<float> ConcatOdd(Full256<float> d, Vec256<float> hi,
                                Vec256<float> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(32) constexpr uint32_t kIdx[8] = {1, 3, 5, 7, 9, 11, 13, 15};
  return Vec256<float>{_mm256_mask2_permutex2var_ps(lo.raw, Load(du, kIdx).raw,
                                                    __mmask8{0xFF}, hi.raw)};
#else
  const Vec256<float> v3131{
      _mm256_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(3, 1, 3, 1))};
  return BitCast(d, Vec256<uint32_t>{_mm256_permute4x64_epi64(
                        BitCast(du, v3131).raw, _MM_SHUFFLE(3, 1, 2, 0))});
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> ConcatOdd(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(64) constexpr uint64_t kIdx[4] = {1, 3, 5, 7};
  return BitCast(d, Vec256<uint64_t>{_mm256_mask2_permutex2var_epi64(
                        BitCast(du, lo).raw, Load(du, kIdx).raw, __mmask8{0xFF},
                        BitCast(du, hi).raw)});
#else
  const RebindToFloat<decltype(d)> df;
  const Vec256<double> v31{
      _mm256_shuffle_pd(BitCast(df, lo).raw, BitCast(df, hi).raw, 15)};
  return Vec256<T>{
      _mm256_permute4x64_epi64(BitCast(du, v31).raw, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

HWY_API Vec256<double> ConcatOdd(Full256<double> d, Vec256<double> hi,
                                 Vec256<double> lo) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint64_t kIdx[4] = {1, 3, 5, 7};
  return Vec256<double>{_mm256_mask2_permutex2var_pd(lo.raw, Load(du, kIdx).raw,
                                                     __mmask8{0xFF}, hi.raw)};
#else
  (void)d;
  const Vec256<double> v31{_mm256_shuffle_pd(lo.raw, hi.raw, 15)};
  return Vec256<double>{
      _mm256_permute4x64_pd(v31.raw, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

// ------------------------------ ConcatEven

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ConcatEven(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET == HWY_AVX3_DL
  alignas(64) constexpr uint8_t kIdx[32] = {
      0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62};
  return BitCast(d, Vec256<uint32_t>{_mm256_mask2_permutex2var_epi8(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask32{0xFFFFFFFFu}, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec256<uint16_t> mask = Set(dw, 0x00FF);
  const Vec256<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec256<uint16_t> uL = And(BitCast(dw, lo), mask);
  const __m256i u8 = _mm256_packus_epi16(uL.raw, uH.raw);
  return Vec256<T>{_mm256_permute4x64_epi64(u8, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> ConcatEven(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(64) constexpr uint16_t kIdx[16] = {0,  2,  4,  6,  8,  10, 12, 14,
                                             16, 18, 20, 22, 24, 26, 28, 30};
  return BitCast(d, Vec256<uint32_t>{_mm256_mask2_permutex2var_epi16(
                        BitCast(du, lo).raw, Load(du, kIdx).raw,
                        __mmask16{0xFFFF}, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Isolate lower 16 bits per u32 so we can pack.
  const Vec256<uint32_t> mask = Set(dw, 0x0000FFFF);
  const Vec256<uint32_t> uH = And(BitCast(dw, hi), mask);
  const Vec256<uint32_t> uL = And(BitCast(dw, lo), mask);
  const __m256i u16 = _mm256_packus_epi32(uL.raw, uH.raw);
  return Vec256<T>{_mm256_permute4x64_epi64(u16, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> ConcatEven(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(64) constexpr uint32_t kIdx[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  return BitCast(d, Vec256<uint32_t>{_mm256_mask2_permutex2var_epi32(
                        BitCast(du, lo).raw, Load(du, kIdx).raw, __mmask8{0xFF},
                        BitCast(du, hi).raw)});
#else
  const RebindToFloat<decltype(d)> df;
  const Vec256<float> v2020{_mm256_shuffle_ps(
      BitCast(df, lo).raw, BitCast(df, hi).raw, _MM_SHUFFLE(2, 0, 2, 0))};
  return Vec256<T>{_mm256_permute4x64_epi64(BitCast(du, v2020).raw,
                                            _MM_SHUFFLE(3, 1, 2, 0))};

#endif
}

HWY_API Vec256<float> ConcatEven(Full256<float> d, Vec256<float> hi,
                                 Vec256<float> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(64) constexpr uint32_t kIdx[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  return Vec256<float>{_mm256_mask2_permutex2var_ps(lo.raw, Load(du, kIdx).raw,
                                                    __mmask8{0xFF}, hi.raw)};
#else
  const Vec256<float> v2020{
      _mm256_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(2, 0, 2, 0))};
  return BitCast(d, Vec256<uint32_t>{_mm256_permute4x64_epi64(
                        BitCast(du, v2020).raw, _MM_SHUFFLE(3, 1, 2, 0))});

#endif
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> ConcatEven(Full256<T> d, Vec256<T> hi, Vec256<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3
  alignas(64) constexpr uint64_t kIdx[4] = {0, 2, 4, 6};
  return BitCast(d, Vec256<uint64_t>{_mm256_mask2_permutex2var_epi64(
                        BitCast(du, lo).raw, Load(du, kIdx).raw, __mmask8{0xFF},
                        BitCast(du, hi).raw)});
#else
  const RebindToFloat<decltype(d)> df;
  const Vec256<double> v20{
      _mm256_shuffle_pd(BitCast(df, lo).raw, BitCast(df, hi).raw, 0)};
  return Vec256<T>{
      _mm256_permute4x64_epi64(BitCast(du, v20).raw, _MM_SHUFFLE(3, 1, 2, 0))};

#endif
}

HWY_API Vec256<double> ConcatEven(Full256<double> d, Vec256<double> hi,
                                  Vec256<double> lo) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) constexpr uint64_t kIdx[4] = {0, 2, 4, 6};
  return Vec256<double>{_mm256_mask2_permutex2var_pd(lo.raw, Load(du, kIdx).raw,
                                                     __mmask8{0xFF}, hi.raw)};
#else
  (void)d;
  const Vec256<double> v20{_mm256_shuffle_pd(lo.raw, hi.raw, 0)};
  return Vec256<double>{
      _mm256_permute4x64_pd(v20.raw, _MM_SHUFFLE(3, 1, 2, 0))};
#endif
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> DupEven(Vec256<T> v) {
  return Vec256<T>{_mm256_shuffle_epi32(v.raw, _MM_SHUFFLE(2, 2, 0, 0))};
}
HWY_API Vec256<float> DupEven(Vec256<float> v) {
  return Vec256<float>{
      _mm256_shuffle_ps(v.raw, v.raw, _MM_SHUFFLE(2, 2, 0, 0))};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> DupEven(const Vec256<T> v) {
  return InterleaveLower(Full256<T>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> DupOdd(Vec256<T> v) {
  return Vec256<T>{_mm256_shuffle_epi32(v.raw, _MM_SHUFFLE(3, 3, 1, 1))};
}
HWY_API Vec256<float> DupOdd(Vec256<float> v) {
  return Vec256<float>{
      _mm256_shuffle_ps(v.raw, v.raw, _MM_SHUFFLE(3, 3, 1, 1))};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> DupOdd(const Vec256<T> v) {
  return InterleaveUpper(Full256<T>(), v, v);
}

// ------------------------------ OddEven

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<1> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  const Full256<T> d;
  const Full256<uint8_t> d8;
  alignas(32) constexpr uint8_t mask[16] = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                            0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, LoadDup128(d8, mask))), b, a);
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<2> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{_mm256_blend_epi16(a.raw, b.raw, 0x55)};
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<4> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{_mm256_blend_epi32(a.raw, b.raw, 0x55)};
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<8> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{_mm256_blend_epi32(a.raw, b.raw, 0x33)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> OddEven(const Vec256<T> a, const Vec256<T> b) {
  return detail::OddEven(hwy::SizeTag<sizeof(T)>(), a, b);
}
HWY_API Vec256<float> OddEven(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_blend_ps(a.raw, b.raw, 0x55)};
}

HWY_API Vec256<double> OddEven(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_blend_pd(a.raw, b.raw, 5)};
}

// ------------------------------ OddEvenBlocks

template <typename T>
Vec256<T> OddEvenBlocks(Vec256<T> odd, Vec256<T> even) {
  return Vec256<T>{_mm256_blend_epi32(odd.raw, even.raw, 0xFu)};
}

HWY_API Vec256<float> OddEvenBlocks(Vec256<float> odd, Vec256<float> even) {
  return Vec256<float>{_mm256_blend_ps(odd.raw, even.raw, 0xFu)};
}

HWY_API Vec256<double> OddEvenBlocks(Vec256<double> odd, Vec256<double> even) {
  return Vec256<double>{_mm256_blend_pd(odd.raw, even.raw, 0x3u)};
}

// ------------------------------ ReverseBlocks (ConcatLowerUpper)

template <typename T>
HWY_API Vec256<T> ReverseBlocks(Full256<T> d, Vec256<T> v) {
  return ConcatLowerUpper(d, v, v);
}

// ------------------------------ TableLookupBytes (ZeroExtendVector)

// Both full
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec256<T> bytes,
                                    const Vec256<TI> from) {
  return Vec256<TI>{_mm256_shuffle_epi8(bytes.raw, from.raw)};
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec256<T> bytes,
                                        const Vec128<TI, NI> from) {
  // First expand to full 128, then 256.
  const auto from_256 = ZeroExtendVector(Full256<TI>(), Vec128<TI>{from.raw});
  const auto tbl_full = TableLookupBytes(bytes, from_256);
  // Shrink to 128, then partial.
  return Vec128<TI, NI>{LowerHalf(Full128<TI>(), tbl_full).raw};
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec128<T, N> bytes,
                                    const Vec256<TI> from) {
  // First expand to full 128, then 256.
  const auto bytes_256 = ZeroExtendVector(Full256<T>(), Vec128<T>{bytes.raw});
  return TableLookupBytes(bytes_256, from);
}

// Partial both are handled by x86_128.

// ------------------------------ Shl (Mul, ZipLower)

namespace detail {

#if HWY_TARGET > HWY_AVX3  // AVX2 or older

// Returns 2^v for use as per-lane multipliers to emulate 16-bit shifts.
template <typename T>
HWY_INLINE Vec256<MakeUnsigned<T>> Pow2(const Vec256<T> v) {
  static_assert(sizeof(T) == 2, "Only for 16-bit");
  const Full256<T> d;
  const RepartitionToWide<decltype(d)> dw;
  const Rebind<float, decltype(dw)> df;
  const auto zero = Zero(d);
  // Move into exponent (this u16 will become the upper half of an f32)
  const auto exp = ShiftLeft<23 - 16>(v);
  const auto upper = exp + Set(d, 0x3F80);  // upper half of 1.0f
  // Insert 0 into lower halves for reinterpreting as binary32.
  const auto f0 = ZipLower(dw, zero, upper);
  const auto f1 = ZipUpper(dw, zero, upper);
  // Do not use ConvertTo because it checks for overflow, which is redundant
  // because we only care about v in [0, 16).
  const Vec256<int32_t> bits0{_mm256_cvttps_epi32(BitCast(df, f0).raw)};
  const Vec256<int32_t> bits1{_mm256_cvttps_epi32(BitCast(df, f1).raw)};
  return Vec256<MakeUnsigned<T>>{_mm256_packus_epi32(bits0.raw, bits1.raw)};
}

#endif  // HWY_TARGET > HWY_AVX3

HWY_INLINE Vec256<uint16_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint16_t> v,
                                Vec256<uint16_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint16_t>{_mm256_sllv_epi16(v.raw, bits.raw)};
#else
  return v * Pow2(bits);
#endif
}

HWY_INLINE Vec256<uint32_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint32_t> v,
                                Vec256<uint32_t> bits) {
  return Vec256<uint32_t>{_mm256_sllv_epi32(v.raw, bits.raw)};
}

HWY_INLINE Vec256<uint64_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint64_t> v,
                                Vec256<uint64_t> bits) {
  return Vec256<uint64_t>{_mm256_sllv_epi64(v.raw, bits.raw)};
}

template <typename T>
HWY_INLINE Vec256<T> Shl(hwy::SignedTag /*tag*/, Vec256<T> v, Vec256<T> bits) {
  // Signed left shifts are the same as unsigned.
  const Full256<T> di;
  const Full256<MakeUnsigned<T>> du;
  return BitCast(di,
                 Shl(hwy::UnsignedTag(), BitCast(du, v), BitCast(du, bits)));
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> operator<<(Vec256<T> v, Vec256<T> bits) {
  return detail::Shl(hwy::TypeTag<T>(), v, bits);
}

// ------------------------------ Shr (MulHigh, IfThenElse, Not)

HWY_API Vec256<uint16_t> operator>>(Vec256<uint16_t> v, Vec256<uint16_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint16_t>{_mm256_srlv_epi16(v.raw, bits.raw)};
#else
  Full256<uint16_t> d;
  // For bits=0, we cannot mul by 2^16, so fix the result later.
  auto out = MulHigh(v, detail::Pow2(Set(d, 16) - bits));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d), v, out);
#endif
}

HWY_API Vec256<uint32_t> operator>>(Vec256<uint32_t> v, Vec256<uint32_t> bits) {
  return Vec256<uint32_t>{_mm256_srlv_epi32(v.raw, bits.raw)};
}

HWY_API Vec256<uint64_t> operator>>(Vec256<uint64_t> v, Vec256<uint64_t> bits) {
  return Vec256<uint64_t>{_mm256_srlv_epi64(v.raw, bits.raw)};
}

HWY_API Vec256<int16_t> operator>>(Vec256<int16_t> v, Vec256<int16_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int16_t>{_mm256_srav_epi16(v.raw, bits.raw)};
#else
  return detail::SignedShr(Full256<int16_t>(), v, bits);
#endif
}

HWY_API Vec256<int32_t> operator>>(Vec256<int32_t> v, Vec256<int32_t> bits) {
  return Vec256<int32_t>{_mm256_srav_epi32(v.raw, bits.raw)};
}

HWY_API Vec256<int64_t> operator>>(Vec256<int64_t> v, Vec256<int64_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_srav_epi64(v.raw, bits.raw)};
#else
  return detail::SignedShr(Full256<int64_t>(), v, bits);
#endif
}

HWY_INLINE Vec256<uint64_t> MulEven(const Vec256<uint64_t> a,
                                    const Vec256<uint64_t> b) {
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

HWY_INLINE Vec256<uint64_t> MulOdd(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
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

HWY_API Vec256<float> ReorderWidenMulAccumulate(Full256<float> df32,
                                                Vec256<bfloat16_t> a,
                                                Vec256<bfloat16_t> b,
                                                const Vec256<float> sum0,
                                                Vec256<float>& sum1) {
  // TODO(janwas): _mm256_dpbf16_ps when available
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const Vec256<uint16_t> zero = Zero(du16);
  // Lane order within sum0/1 is undefined, hence we can avoid the
  // longer-latency lane-crossing PromoteTo.
  const Vec256<uint32_t> a0 = ZipLower(du32, zero, BitCast(du16, a));
  const Vec256<uint32_t> a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const Vec256<uint32_t> b0 = ZipLower(du32, zero, BitCast(du16, b));
  const Vec256<uint32_t> b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

HWY_API Vec256<double> PromoteTo(Full256<double> /* tag */,
                                 const Vec128<float, 4> v) {
  return Vec256<double>{_mm256_cvtps_pd(v.raw)};
}

HWY_API Vec256<double> PromoteTo(Full256<double> /* tag */,
                                 const Vec128<int32_t, 4> v) {
  return Vec256<double>{_mm256_cvtepi32_pd(v.raw)};
}

// Unsigned: zero-extend.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then Zip* would be faster.
HWY_API Vec256<uint16_t> PromoteTo(Full256<uint16_t> /* tag */,
                                   Vec128<uint8_t> v) {
  return Vec256<uint16_t>{_mm256_cvtepu8_epi16(v.raw)};
}
HWY_API Vec256<uint32_t> PromoteTo(Full256<uint32_t> /* tag */,
                                   Vec128<uint8_t, 8> v) {
  return Vec256<uint32_t>{_mm256_cvtepu8_epi32(v.raw)};
}
HWY_API Vec256<int16_t> PromoteTo(Full256<int16_t> /* tag */,
                                  Vec128<uint8_t> v) {
  return Vec256<int16_t>{_mm256_cvtepu8_epi16(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<uint8_t, 8> v) {
  return Vec256<int32_t>{_mm256_cvtepu8_epi32(v.raw)};
}
HWY_API Vec256<uint32_t> PromoteTo(Full256<uint32_t> /* tag */,
                                   Vec128<uint16_t> v) {
  return Vec256<uint32_t>{_mm256_cvtepu16_epi32(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<uint16_t> v) {
  return Vec256<int32_t>{_mm256_cvtepu16_epi32(v.raw)};
}
HWY_API Vec256<uint64_t> PromoteTo(Full256<uint64_t> /* tag */,
                                   Vec128<uint32_t> v) {
  return Vec256<uint64_t>{_mm256_cvtepu32_epi64(v.raw)};
}

// Signed: replicate sign bit.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then ZipUpper/lo followed by
// signed shift would be faster.
HWY_API Vec256<int16_t> PromoteTo(Full256<int16_t> /* tag */,
                                  Vec128<int8_t> v) {
  return Vec256<int16_t>{_mm256_cvtepi8_epi16(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<int8_t, 8> v) {
  return Vec256<int32_t>{_mm256_cvtepi8_epi32(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<int16_t> v) {
  return Vec256<int32_t>{_mm256_cvtepi16_epi32(v.raw)};
}
HWY_API Vec256<int64_t> PromoteTo(Full256<int64_t> /* tag */,
                                  Vec128<int32_t> v) {
  return Vec256<int64_t>{_mm256_cvtepi32_epi64(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

HWY_API Vec128<uint16_t> DemoteTo(Full128<uint16_t> /* tag */,
                                  const Vec256<int32_t> v) {
  const __m256i u16 = _mm256_packus_epi32(v.raw, v.raw);
  // Concatenating lower halves of both 128-bit blocks afterward is more
  // efficient than an extra input with low block = high block of v.
  return Vec128<uint16_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(u16, 0x88))};
}

HWY_API Vec128<int16_t> DemoteTo(Full128<int16_t> /* tag */,
                                 const Vec256<int32_t> v) {
  const __m256i i16 = _mm256_packs_epi32(v.raw, v.raw);
  return Vec128<int16_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(i16, 0x88))};
}

HWY_API Vec128<uint8_t, 8> DemoteTo(Full64<uint8_t> /* tag */,
                                    const Vec256<int32_t> v) {
  const __m256i u16_blocks = _mm256_packus_epi32(v.raw, v.raw);
  // Concatenate lower 64 bits of each 128-bit block
  const __m256i u16_concat = _mm256_permute4x64_epi64(u16_blocks, 0x88);
  const __m128i u16 = _mm256_castsi256_si128(u16_concat);
  // packus treats the input as signed; we want unsigned. Clear the MSB to get
  // unsigned saturation to u8.
  const __m128i i16 = _mm_and_si128(u16, _mm_set1_epi16(0x7FFF));
  return Vec128<uint8_t, 8>{_mm_packus_epi16(i16, i16)};
}

HWY_API Vec128<uint8_t> DemoteTo(Full128<uint8_t> /* tag */,
                                 const Vec256<int16_t> v) {
  const __m256i u8 = _mm256_packus_epi16(v.raw, v.raw);
  return Vec128<uint8_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(u8, 0x88))};
}

HWY_API Vec128<int8_t, 8> DemoteTo(Full64<int8_t> /* tag */,
                                   const Vec256<int32_t> v) {
  const __m256i i16_blocks = _mm256_packs_epi32(v.raw, v.raw);
  // Concatenate lower 64 bits of each 128-bit block
  const __m256i i16_concat = _mm256_permute4x64_epi64(i16_blocks, 0x88);
  const __m128i i16 = _mm256_castsi256_si128(i16_concat);
  return Vec128<int8_t, 8>{_mm_packs_epi16(i16, i16)};
}

HWY_API Vec128<int8_t> DemoteTo(Full128<int8_t> /* tag */,
                                const Vec256<int16_t> v) {
  const __m256i i8 = _mm256_packs_epi16(v.raw, v.raw);
  return Vec128<int8_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(i8, 0x88))};
}

  // Avoid "value of intrinsic immediate argument '8' is out of range '0 - 7'".
  // 8 is the correct value of _MM_FROUND_NO_EXC, which is allowed here.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4556, ignored "-Wsign-conversion")

HWY_API Vec128<float16_t> DemoteTo(Full128<float16_t> df16,
                                   const Vec256<float> v) {
#ifdef HWY_DISABLE_F16C
  const RebindToUnsigned<decltype(df16)> du16;
  const Rebind<uint32_t, decltype(df16)> du;
  const RebindToSigned<decltype(du)> di;
  const auto bits32 = BitCast(du, v);
  const auto sign = ShiftRight<31>(bits32);
  const auto biased_exp32 = ShiftRight<23>(bits32) & Set(du, 0xFF);
  const auto mantissa32 = bits32 & Set(du, 0x7FFFFF);

  const auto k15 = Set(di, 15);
  const auto exp = Min(BitCast(di, biased_exp32) - Set(di, 127), k15);
  const auto is_tiny = exp < Set(di, -24);

  const auto is_subnormal = exp < Set(di, -14);
  const auto biased_exp16 =
      BitCast(du, IfThenZeroElse(is_subnormal, exp + k15));
  const auto sub_exp = BitCast(du, Set(di, -14) - exp);  // [1, 11)
  const auto sub_m = (Set(du, 1) << (Set(du, 10) - sub_exp)) +
                     (mantissa32 >> (Set(du, 13) + sub_exp));
  const auto mantissa16 = IfThenElse(RebindMask(du, is_subnormal), sub_m,
                                     ShiftRight<13>(mantissa32));  // <1024

  const auto sign16 = ShiftLeft<15>(sign);
  const auto normal16 = sign16 | ShiftLeft<10>(biased_exp16) | mantissa16;
  const auto bits16 = IfThenZeroElse(is_tiny, BitCast(di, normal16));
  return BitCast(df16, DemoteTo(du16, bits16));
#else
  (void)df16;
  return Vec128<float16_t>{_mm256_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
#endif
}

HWY_DIAGNOSTICS(pop)

HWY_API Vec128<bfloat16_t> DemoteTo(Full128<bfloat16_t> dbf16,
                                    const Vec256<float> v) {
  // TODO(janwas): _mm256_cvtneps_pbh once we have avx512bf16.
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

HWY_API Vec256<bfloat16_t> ReorderDemote2To(Full256<bfloat16_t> dbf16,
                                            Vec256<float> a, Vec256<float> b) {
  // TODO(janwas): _mm256_cvtne2ps_pbh once we have avx512bf16.
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec256<uint32_t> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

HWY_API Vec128<float> DemoteTo(Full128<float> /* tag */,
                               const Vec256<double> v) {
  return Vec128<float>{_mm256_cvtpd_ps(v.raw)};
}

HWY_API Vec128<int32_t> DemoteTo(Full128<int32_t> /* tag */,
                                 const Vec256<double> v) {
  const auto clamped = detail::ClampF64ToI32Max(Full256<double>(), v);
  return Vec128<int32_t>{_mm256_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
HWY_API Vec128<uint8_t, 8> U8FromU32(const Vec256<uint32_t> v) {
  const Full256<uint32_t> d32;
  alignas(32) static constexpr uint32_t k8From32[8] = {
      0x0C080400u, ~0u, ~0u, ~0u, ~0u, 0x0C080400u, ~0u, ~0u};
  // Place first four bytes in lo[0], remaining 4 in hi[1].
  const auto quad = TableLookupBytes(v, Load(d32, k8From32));
  // Interleave both quadruplets - OR instead of unpack reduces port5 pressure.
  const auto lo = LowerHalf(quad);
  const auto hi = UpperHalf(Full128<uint32_t>(), quad);
  const auto pair = LowerHalf(lo | hi);
  return BitCast(Full64<uint8_t>(), pair);
}

// ------------------------------ Truncations

namespace detail {

// LO and HI each hold four indices of bytes within a 128-bit block.
template <uint32_t LO, uint32_t HI, typename T>
HWY_INLINE Vec128<uint32_t> LookupAndConcatHalves(Vec256<T> v) {
  const Full256<uint32_t> d32;

#if HWY_TARGET <= HWY_AVX3_DL
  alignas(32) constexpr uint32_t kMap[8] = {
      LO, HI, 0x10101010 + LO, 0x10101010 + HI, 0, 0, 0, 0};
  const auto result = _mm256_permutexvar_epi8(v.raw, Load(d32, kMap).raw);
#else
  alignas(32) static constexpr uint32_t kMap[8] = {LO,  HI,  ~0u, ~0u,
                                                   ~0u, ~0u, LO,  HI};
  const auto quad = TableLookupBytes(v, Load(d32, kMap));
  const auto result = _mm256_permute4x64_epi64(quad.raw, 0xCC);
  // Possible alternative:
  // const auto lo = LowerHalf(quad);
  // const auto hi = UpperHalf(Full128<uint32_t>(), quad);
  // const auto result = lo | hi;
#endif

  return Vec128<uint32_t>{_mm256_castsi256_si128(result)};
}

// LO and HI each hold two indices of bytes within a 128-bit block.
template <uint16_t LO, uint16_t HI, typename T>
HWY_INLINE Vec128<uint32_t, 2> LookupAndConcatQuarters(Vec256<T> v) {
  const Full256<uint16_t> d16;

#if HWY_TARGET <= HWY_AVX3_DL
  alignas(32) constexpr uint16_t kMap[16] = {
      LO, HI, 0x1010 + LO, 0x1010 + HI, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  const auto result = _mm256_permutexvar_epi8(v.raw, Load(d16, kMap).raw);
  return LowerHalf(Vec128<uint32_t>{_mm256_castsi256_si128(result)});
#else
  constexpr uint16_t ff = static_cast<uint16_t>(~0u);
  alignas(32) static constexpr uint16_t kMap[16] = {
      LO, ff, HI, ff, ff, ff, ff, ff, ff, ff, ff, ff, LO, ff, HI, ff};
  const auto quad = TableLookupBytes(v, Load(d16, kMap));
  const auto mixed = _mm256_permute4x64_epi64(quad.raw, 0xCC);
  const auto half = _mm256_castsi256_si128(mixed);
  return LowerHalf(Vec128<uint32_t>{_mm_packus_epi32(half, half)});
#endif
}

}  // namespace detail

HWY_API Vec128<uint8_t, 4> TruncateTo(Simd<uint8_t, 4, 0> /* tag */,
                                      const Vec256<uint64_t> v) {
  const Full256<uint32_t> d32;
#if HWY_TARGET <= HWY_AVX3_DL
  alignas(32) constexpr uint32_t kMap[8] = {0x18100800u, 0, 0, 0, 0, 0, 0, 0};
  const auto result = _mm256_permutexvar_epi8(v.raw, Load(d32, kMap).raw);
  return LowerHalf(LowerHalf(LowerHalf(Vec256<uint8_t>{result})));
#else
  alignas(32) static constexpr uint32_t kMap[8] = {0xFFFF0800u, ~0u, ~0u, ~0u,
                                                   0x0800FFFFu, ~0u, ~0u, ~0u};
  const auto quad = TableLookupBytes(v, Load(d32, kMap));
  const auto lo = LowerHalf(quad);
  const auto hi = UpperHalf(Full128<uint32_t>(), quad);
  const auto result = lo | hi;
  return LowerHalf(LowerHalf(Vec128<uint8_t>{result.raw}));
#endif
}

HWY_API Vec128<uint16_t, 4> TruncateTo(Simd<uint16_t, 4, 0> /* tag */,
                                       const Vec256<uint64_t> v) {
  const auto result = detail::LookupAndConcatQuarters<0x100, 0x908>(v);
  return Vec128<uint16_t, 4>{result.raw};
}

HWY_API Vec128<uint32_t> TruncateTo(Simd<uint32_t, 4, 0> /* tag */,
                                    const Vec256<uint64_t> v) {
  const Full256<uint32_t> d32;
  alignas(32) constexpr uint32_t kEven[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto v32 =
      TableLookupLanes(BitCast(d32, v), SetTableIndices(d32, kEven));
  return LowerHalf(Vec256<uint32_t>{v32.raw});
}

HWY_API Vec128<uint8_t, 8> TruncateTo(Simd<uint8_t, 8, 0> /* tag */,
                                      const Vec256<uint32_t> v) {
  const auto full = detail::LookupAndConcatQuarters<0x400, 0xC08>(v);
  return Vec128<uint8_t, 8>{full.raw};
}

HWY_API Vec128<uint16_t> TruncateTo(Simd<uint16_t, 8, 0> /* tag */,
                                    const Vec256<uint32_t> v) {
  const auto full = detail::LookupAndConcatHalves<0x05040100, 0x0D0C0908>(v);
  return Vec128<uint16_t>{full.raw};
}

HWY_API Vec128<uint8_t> TruncateTo(Simd<uint8_t, 16, 0> /* tag */,
                                   const Vec256<uint16_t> v) {
  const auto full = detail::LookupAndConcatHalves<0x06040200, 0x0E0C0A08>(v);
  return Vec128<uint8_t>{full.raw};
}

// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

HWY_API Vec256<float> ConvertTo(Full256<float> /* tag */,
                                const Vec256<int32_t> v) {
  return Vec256<float>{_mm256_cvtepi32_ps(v.raw)};
}

HWY_API Vec256<double> ConvertTo(Full256<double> dd, const Vec256<int64_t> v) {
#if HWY_TARGET <= HWY_AVX3
  (void)dd;
  return Vec256<double>{_mm256_cvtepi64_pd(v.raw)};
#else
  // Based on wim's approach (https://stackoverflow.com/questions/41144668/)
  const Repartition<uint32_t, decltype(dd)> d32;
  const Repartition<uint64_t, decltype(dd)> d64;

  // Toggle MSB of lower 32-bits and insert exponent for 2^84 + 2^63
  const auto k84_63 = Set(d64, 0x4530000080000000ULL);
  const auto v_upper = BitCast(dd, ShiftRight<32>(BitCast(d64, v)) ^ k84_63);

  // Exponent is 2^52, lower 32 bits from v (=> 32-bit OddEven)
  const auto k52 = Set(d32, 0x43300000);
  const auto v_lower = BitCast(dd, OddEven(k52, BitCast(d32, v)));

  const auto k84_63_52 = BitCast(dd, Set(d64, 0x4530000080100000ULL));
  return (v_upper - k84_63_52) + v_lower;  // order matters!
#endif
}

// Truncates (rounds toward zero).
HWY_API Vec256<int32_t> ConvertTo(Full256<int32_t> d, const Vec256<float> v) {
  return detail::FixConversionOverflow(d, v, _mm256_cvttps_epi32(v.raw));
}

HWY_API Vec256<int64_t> ConvertTo(Full256<int64_t> di, const Vec256<double> v) {
#if HWY_TARGET <= HWY_AVX3
  return detail::FixConversionOverflow(di, v, _mm256_cvttpd_epi64(v.raw));
#else
  using VI = decltype(Zero(di));
  const VI k0 = Zero(di);
  const VI k1 = Set(di, 1);
  const VI k51 = Set(di, 51);

  // Exponent indicates whether the number can be represented as int64_t.
  const VI biased_exp = ShiftRight<52>(BitCast(di, v)) & Set(di, 0x7FF);
  const VI exp = biased_exp - Set(di, 0x3FF);
  const auto in_range = exp < Set(di, 63);

  // If we were to cap the exponent at 51 and add 2^52, the number would be in
  // [2^52, 2^53) and mantissa bits could be read out directly. We need to
  // round-to-0 (truncate), but changing rounding mode in MXCSR hits a
  // compiler reordering bug: https://gcc.godbolt.org/z/4hKj6c6qc . We instead
  // manually shift the mantissa into place (we already have many of the
  // inputs anyway).
  const VI shift_mnt = Max(k51 - exp, k0);
  const VI shift_int = Max(exp - k51, k0);
  const VI mantissa = BitCast(di, v) & Set(di, (1ULL << 52) - 1);
  // Include implicit 1-bit; shift by one more to ensure it's in the mantissa.
  const VI int52 = (mantissa | Set(di, 1ULL << 52)) >> (shift_mnt + k1);
  // For inputs larger than 2^52, insert zeros at the bottom.
  const VI shifted = int52 << shift_int;
  // Restore the one bit lost when shifting in the implicit 1-bit.
  const VI restored = shifted | ((mantissa & k1) << (shift_int - k1));

  // Saturate to LimitsMin (unchanged when negating below) or LimitsMax.
  const VI sign_mask = BroadcastSignBit(BitCast(di, v));
  const VI limit = Set(di, LimitsMax<int64_t>()) - sign_mask;
  const VI magnitude = IfThenElse(in_range, restored, limit);

  // If the input was negative, negate the integer (two's complement).
  return (magnitude ^ sign_mask) - sign_mask;
#endif
}

HWY_API Vec256<int32_t> NearestInt(const Vec256<float> v) {
  const Full256<int32_t> di;
  return detail::FixConversionOverflow(di, v, _mm256_cvtps_epi32(v.raw));
}


HWY_API Vec256<float> PromoteTo(Full256<float> df32,
                                const Vec128<float16_t> v) {
#ifdef HWY_DISABLE_F16C
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t>{v.raw});
  const auto sign = ShiftRight<15>(bits16);
  const auto biased_exp = ShiftRight<10>(bits16) & Set(du32, 0x1F);
  const auto mantissa = bits16 & Set(du32, 0x3FF);
  const auto subnormal =
      BitCast(du32, ConvertTo(df32, BitCast(di32, mantissa)) *
                        Set(df32, 1.0f / 16384 / 1024));

  const auto biased_exp32 = biased_exp + Set(du32, 127 - 15);
  const auto mantissa32 = ShiftLeft<23 - 10>(mantissa);
  const auto normal = ShiftLeft<23>(biased_exp32) | mantissa32;
  const auto bits32 = IfThenElse(biased_exp == Zero(du32), subnormal, normal);
  return BitCast(df32, ShiftLeft<31>(sign) | bits32);
#else
  (void)df32;
  return Vec256<float>{_mm256_cvtph_ps(v.raw)};
#endif
}

HWY_API Vec256<float> PromoteTo(Full256<float> df32,
                                const Vec128<bfloat16_t> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec256<uint8_t> AESRound(Vec256<uint8_t> state,
                                 Vec256<uint8_t> round_key) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint8_t>{_mm256_aesenc_epi128(state.raw, round_key.raw)};
#else
  const Full256<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, AESRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec256<uint8_t> AESLastRound(Vec256<uint8_t> state,
                                     Vec256<uint8_t> round_key) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint8_t>{_mm256_aesenclast_epi128(state.raw, round_key.raw)};
#else
  const Full256<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(d,
                 AESLastRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESLastRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec256<uint64_t> CLMulLower(Vec256<uint64_t> a, Vec256<uint64_t> b) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint64_t>{_mm256_clmulepi64_epi128(a.raw, b.raw, 0x00)};
#else
  const Full256<uint64_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, CLMulLower(UpperHalf(d2, a), UpperHalf(d2, b)),
                 CLMulLower(LowerHalf(a), LowerHalf(b)));
#endif
}

HWY_API Vec256<uint64_t> CLMulUpper(Vec256<uint64_t> a, Vec256<uint64_t> b) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint64_t>{_mm256_clmulepi64_epi128(a.raw, b.raw, 0x11)};
#else
  const Full256<uint64_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, CLMulUpper(UpperHalf(d2, a), UpperHalf(d2, b)),
                 CLMulUpper(LowerHalf(a), LowerHalf(b)));
#endif
}

#endif  // HWY_DISABLE_PCLMUL_AES

// ================================================== MISC

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, typename T2>
HWY_API Vec256<T> Iota(const Full256<T> d, const T2 first) {
  HWY_ALIGN T lanes[32 / sizeof(T)];
  for (size_t i = 0; i < 32 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ LoadMaskBits

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T>
HWY_API Mask256<T> LoadMaskBits(const Full256<T> /* tag */,
                                const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return Mask256<T>::FromBits(mask_bits);
}

// ------------------------------ StoreMaskBits

// `p` points to at least 8 writable bytes.
template <typename T>
HWY_API size_t StoreMaskBits(const Full256<T> /* tag */, const Mask256<T> mask,
                             uint8_t* bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  CopyBytes<kNumBytes>(&mask.raw, bits);

  // Non-full byte, need to clear the undefined upper bits.
  if (N < 8) {
    const int mask = static_cast<int>((1ull << N) - 1);
    bits[0] = static_cast<uint8_t>(bits[0] & mask);
  }
  return kNumBytes;
}

// ------------------------------ Mask testing

template <typename T>
HWY_API size_t CountTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  return PopCount(static_cast<uint64_t>(mask.raw));
}

template <typename T>
HWY_API intptr_t FindFirstTrue(const Full256<T> /* tag */,
                               const Mask256<T> mask) {
  return mask.raw ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask.raw)) : -1;
}

// Beware: the suffix indicates the number of mask bits, not lane size!

namespace detail {

template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<1> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<2> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<4> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<8> /*tag*/, const Mask256<T> mask) {
  return (uint64_t{mask.raw} & 0xF) == 0;
}

}  // namespace detail

template <typename T>
HWY_API bool AllFalse(const Full256<T> /* tag */, const Mask256<T> mask) {
  return detail::AllFalse(hwy::SizeTag<sizeof(T)>(), mask);
}

namespace detail {

template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<1> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFFFFFu;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<2> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFu;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<4> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFu;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<8> /*tag*/, const Mask256<T> mask) {
  // Cannot use _kortestc because we have less than 8 mask bits.
  return mask.raw == 0xFu;
}

}  // namespace detail

template <typename T>
HWY_API bool AllTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  return detail::AllTrue(hwy::SizeTag<sizeof(T)>(), mask);
}

// ------------------------------ Compress

// 16-bit is defined in x86_512 so we can use 512-bit vectors.

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> mask) {
  return Vec256<T>{_mm256_maskz_compress_epi32(mask.raw, v.raw)};
}

HWY_API Vec256<float> Compress(Vec256<float> v, Mask256<float> mask) {
  return Vec256<float>{_mm256_maskz_compress_ps(mask.raw, v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> mask) {
  // See CompressIsPartition.
  alignas(16) constexpr uint64_t packed_array[16] = {
      // PrintCompress64x4NibbleTables
      0x00003210, 0x00003210, 0x00003201, 0x00003210, 0x00003102, 0x00003120,
      0x00003021, 0x00003210, 0x00002103, 0x00002130, 0x00002031, 0x00002310,
      0x00001032, 0x00001320, 0x00000321, 0x00003210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 2) -
  // _mm256_permutexvar_epi64 will ignore the upper bits.
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(64) constexpr uint64_t shifts[4] = {0, 4, 8, 12};
  const auto indices = Indices256<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

// ------------------------------ CompressNot (Compress)

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec256<T> CompressNot(Vec256<T> v, const Mask256<T> mask) {
  return Compress(v, Not(mask));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> CompressNot(Vec256<T> v, Mask256<T> mask) {
  // See CompressIsPartition.
  alignas(16) constexpr uint64_t packed_array[16] = {
      // PrintCompressNot64x4NibbleTables
      0x00003210, 0x00000321, 0x00001320, 0x00001032, 0x00002310, 0x00002031,
      0x00002130, 0x00002103, 0x00003210, 0x00003021, 0x00003120, 0x00003102,
      0x00003210, 0x00003201, 0x00003210, 0x00003210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 2) -
  // _mm256_permutexvar_epi64 will ignore the upper bits.
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(64) constexpr uint64_t shifts[4] = {0, 4, 8, 12};
  const auto indices = Indices256<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

// ------------------------------ CompressBlocksNot
HWY_API Vec256<uint64_t> CompressBlocksNot(Vec256<uint64_t> v,
                                           Mask256<uint64_t> mask) {
  return CompressNot(v, mask);
}

// ------------------------------ CompressBits (LoadMaskBits)
template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(Full256<T>(), bits));
}

// ------------------------------ CompressStore

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> mask, Full256<T> d,
                             T* HWY_RESTRICT unaligned) {
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

  const uint64_t mask_bits{mask.raw};

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  _mm256_mask_compressstoreu_epi16(unaligned, mask.raw, vu.raw);
#else
  // Split into halves to keep the table size manageable.
  const Half<decltype(du)> duh;
  const auto vL = LowerHalf(duh, vu);
  const auto vH = UpperHalf(duh, vu);

  const uint64_t mask_bitsL = mask_bits & 0xFF;
  const uint64_t mask_bitsH = mask_bits >> 8;

  const auto idxL = detail::IndicesForCompress16(mask_bitsL);
  const auto idxH = detail::IndicesForCompress16(mask_bitsH);

  // Compress and 128-bit halves.
  const Vec128<uint16_t> cL{_mm_permutexvar_epi16(idxL.raw, vL.raw)};
  const Vec128<uint16_t> cH{_mm_permutexvar_epi16(idxH.raw, vH.raw)};
  const Half<decltype(d)> dh;
  StoreU(BitCast(dh, cL), dh, unaligned);
  StoreU(BitCast(dh, cH), dh, unaligned + PopCount(mask_bitsL));
#endif  // HWY_TARGET == HWY_AVX3_DL

  return PopCount(mask_bits);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> mask, Full256<T> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw});
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> mask, Full256<T> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw} & 0xFull);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

HWY_API size_t CompressStore(Vec256<float> v, Mask256<float> mask,
                             Full256<float> /* tag */,
                             float* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw});
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(float));
#endif
  return count;
}

HWY_API size_t CompressStore(Vec256<double> v, Mask256<double> mask,
                             Full256<double> /* tag */,
                             double* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw} & 0xFull);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(double));
#endif
  return count;
}

// ------------------------------ CompressBlendedStore (CompressStore)

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API size_t CompressBlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  // Native (32 or 64-bit) AVX-512 instruction already does the blending at no
  // extra cost (latency 11, rthroughput 2 - same as compress plus store).
  return CompressStore(v, m, d, unaligned);
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CompressBlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
#if HWY_TARGET <= HWY_AVX3_DL
  return CompressStore(v, m, d, unaligned);  // also native
#else
  const size_t count = CountTrue(d, m);
  BlendedStore(Compress(v, m), FirstN(d, count), d, unaligned);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
#endif
}

// ------------------------------ CompressBitsStore (LoadMaskBits)

template <typename T>
HWY_API size_t CompressBitsStore(Vec256<T> v, const uint8_t* HWY_RESTRICT bits,
                                 Full256<T> d, T* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

#else  // AVX2

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

// 256 suffix avoids ambiguity with x86_128 without needing HWY_IF_LE128 there.
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  const Repartition<uint32_t, decltype(d)> du32;
  const auto vbits = BitCast(du, Set(du32, static_cast<uint32_t>(mask_bits)));

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  const Repartition<uint64_t, decltype(d)> du64;
  alignas(32) constexpr uint64_t kRep8[4] = {
      0x0000000000000000ull, 0x0101010101010101ull, 0x0202020202020202ull,
      0x0303030303030303ull};
  const auto rep8 = TableLookupBytes(vbits, BitCast(du, Load(du64, kRep8)));

  alignas(32) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint16_t kBit[16] = {
      1,     2,     4,     8,     16,     32,     64,     128,
      0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint32_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint64_t kBit[8] = {1, 2, 4, 8};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T>
HWY_API Mask256<T> LoadMaskBits(Full256<T> d,
                                const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::LoadMaskBits256(d, mask_bits);
}

// ------------------------------ StoreMaskBits

namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE uint64_t BitsFromMask(const Mask256<T> mask) {
  const Full256<T> d;
  const Full256<uint8_t> d8;
  const auto sign_bits = BitCast(d8, VecFromMask(d, mask)).raw;
  // Prevent sign-extension of 32-bit masks because the intrinsic returns int.
  return static_cast<uint32_t>(_mm256_movemask_epi8(sign_bits));
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE uint64_t BitsFromMask(const Mask256<T> mask) {
#if HWY_ARCH_X86_64
  const Full256<T> d;
  const Full256<uint8_t> d8;
  const Mask256<uint8_t> mask8 = MaskFromVec(BitCast(d8, VecFromMask(d, mask)));
  const uint64_t sign_bits8 = BitsFromMask(mask8);
  // Skip the bits from the lower byte of each u16 (better not to use the
  // same packs_epi16 as SSE4, because that requires an extra swizzle here).
  return _pext_u64(sign_bits8, 0xAAAAAAAAull);
#else
  // Slow workaround for 32-bit builds, which lack _pext_u64.
  // Remove useless lower half of each u16 while preserving the sign bit.
  // Bytes [0, 8) and [16, 24) have the same sign bits as the input lanes.
  const auto sign_bits = _mm256_packs_epi16(mask.raw, _mm256_setzero_si256());
  // Move odd qwords (value zero) to top so they don't affect the mask value.
  const auto compressed =
      _mm256_permute4x64_epi64(sign_bits, _MM_SHUFFLE(3, 1, 2, 0));
  return static_cast<unsigned>(_mm256_movemask_epi8(compressed));
#endif  // HWY_ARCH_X86_64
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE uint64_t BitsFromMask(const Mask256<T> mask) {
  const Full256<T> d;
  const Full256<float> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask)).raw;
  return static_cast<unsigned>(_mm256_movemask_ps(sign_bits));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE uint64_t BitsFromMask(const Mask256<T> mask) {
  const Full256<T> d;
  const Full256<double> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask)).raw;
  return static_cast<unsigned>(_mm256_movemask_pd(sign_bits));
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <typename T>
HWY_API size_t StoreMaskBits(const Full256<T> /* tag */, const Mask256<T> mask,
                             uint8_t* bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  const uint64_t mask_bits = detail::BitsFromMask(mask);
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

// ------------------------------ Mask testing

// Specialize for 16-bit lanes to avoid unnecessary pext. This assumes each mask
// lane is 0 or ~0.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API bool AllFalse(const Full256<T> d, const Mask256<T> mask) {
  const Repartition<uint8_t, decltype(d)> d8;
  const Mask256<uint8_t> mask8 = MaskFromVec(BitCast(d8, VecFromMask(d, mask)));
  return detail::BitsFromMask(mask8) == 0;
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API bool AllFalse(const Full256<T> /* tag */, const Mask256<T> mask) {
  // Cheaper than PTEST, which is 2 uop / 3L.
  return detail::BitsFromMask(mask) == 0;
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API bool AllTrue(const Full256<T> d, const Mask256<T> mask) {
  const Repartition<uint8_t, decltype(d)> d8;
  const Mask256<uint8_t> mask8 = MaskFromVec(BitCast(d8, VecFromMask(d, mask)));
  return detail::BitsFromMask(mask8) == (1ull << 32) - 1;
}
template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API bool AllTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  constexpr uint64_t kAllBits = (1ull << (32 / sizeof(T))) - 1;
  return detail::BitsFromMask(mask) == kAllBits;
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CountTrue(const Full256<T> d, const Mask256<T> mask) {
  const Repartition<uint8_t, decltype(d)> d8;
  const Mask256<uint8_t> mask8 = MaskFromVec(BitCast(d8, VecFromMask(d, mask)));
  return PopCount(detail::BitsFromMask(mask8)) >> 1;
}
template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API size_t CountTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

template <typename T>
HWY_API intptr_t FindFirstTrue(const Full256<T> /* tag */,
                               const Mask256<T> mask) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero64(mask_bits)) : -1;
}

// ------------------------------ Compress, CompressBits

namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Indices256<uint32_t> IndicesFromBits(Full256<T> d,
                                                uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> d32;
  // We need a masked Iota(). With 8 lanes, there are 256 combinations and a LUT
  // of SetTableIndices would require 8 KiB, a large part of L1D. The other
  // alternative is _pext_u64, but this is extremely slow on Zen2 (18 cycles)
  // and unavailable in 32-bit builds. We instead compress each index into 4
  // bits, for a total of 1 KiB.
  alignas(16) constexpr uint32_t packed_array[256] = {
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

  // No need to mask because _mm256_permutevar8x32_epi32 ignores bits 3..31.
  // Just shift each copy of the 32 bit LUT to extract its 4-bit fields.
  // If broadcasting 32-bit from memory incurs the 3-cycle block-crossing
  // latency, it may be faster to use LoadDup128 and PSHUFB.
  const auto packed = Set(d32, packed_array[mask_bits]);
  alignas(32) constexpr uint32_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  return Indices256<uint32_t>{(packed >> Load(d32, shifts)).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Indices256<uint32_t> IndicesFromBits(Full256<T> d,
                                                uint64_t mask_bits) {
  const Repartition<uint32_t, decltype(d)> d32;

  // For 64-bit, we still need 32-bit indices because there is no 64-bit
  // permutevar, but there are only 4 lanes, so we can afford to skip the
  // unpacking and load the entire index vector directly.
  alignas(32) constexpr uint32_t u32_indices[128] = {
      // PrintCompress64x4PairTables
      0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 2, 3, 0, 1, 4, 5,
      6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 4, 5, 0, 1, 2, 3, 6, 7, 0, 1, 4, 5,
      2, 3, 6, 7, 2, 3, 4, 5, 0, 1, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 6, 7,
      0, 1, 2, 3, 4, 5, 0, 1, 6, 7, 2, 3, 4, 5, 2, 3, 6, 7, 0, 1, 4, 5,
      0, 1, 2, 3, 6, 7, 4, 5, 4, 5, 6, 7, 0, 1, 2, 3, 0, 1, 4, 5, 6, 7,
      2, 3, 2, 3, 4, 5, 6, 7, 0, 1, 0, 1, 2, 3, 4, 5, 6, 7};
  return Indices256<uint32_t>{Load(d32, u32_indices + 8 * mask_bits).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Indices256<uint32_t> IndicesFromNotBits(Full256<T> d,
                                                   uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> d32;
  // We need a masked Iota(). With 8 lanes, there are 256 combinations and a LUT
  // of SetTableIndices would require 8 KiB, a large part of L1D. The other
  // alternative is _pext_u64, but this is extremely slow on Zen2 (18 cycles)
  // and unavailable in 32-bit builds. We instead compress each index into 4
  // bits, for a total of 1 KiB.
  alignas(16) constexpr uint32_t packed_array[256] = {
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

  // No need to mask because <_mm256_permutevar8x32_epi32> ignores bits 3..31.
  // Just shift each copy of the 32 bit LUT to extract its 4-bit fields.
  // If broadcasting 32-bit from memory incurs the 3-cycle block-crossing
  // latency, it may be faster to use LoadDup128 and PSHUFB.
  const auto packed = Set(d32, packed_array[mask_bits]);
  alignas(32) constexpr uint32_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  return Indices256<uint32_t>{(packed >> Load(d32, shifts)).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Indices256<uint32_t> IndicesFromNotBits(Full256<T> d,
                                                   uint64_t mask_bits) {
  const Repartition<uint32_t, decltype(d)> d32;

  // For 64-bit, we still need 32-bit indices because there is no 64-bit
  // permutevar, but there are only 4 lanes, so we can afford to skip the
  // unpacking and load the entire index vector directly.
  alignas(32) constexpr uint32_t u32_indices[128] = {
      // PrintCompressNot64x4PairTables
      0, 1, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 0, 1, 0, 1, 4, 5, 6, 7,
      2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 0, 1, 2, 3, 6, 7, 4, 5, 2, 3, 6, 7,
      0, 1, 4, 5, 0, 1, 6, 7, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 0, 1,
      2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 0, 1, 6, 7, 0, 1, 4, 5, 2, 3, 6, 7,
      4, 5, 0, 1, 2, 3, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 2, 3, 0, 1, 4, 5,
      6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};
  return Indices256<uint32_t>{Load(d32, u32_indices + 8 * mask_bits).raw};
}
template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<T> Compress(Vec256<T> v, const uint64_t mask_bits) {
  const Full256<T> d;
  const Repartition<uint32_t, decltype(d)> du32;

  HWY_DASSERT(mask_bits < (1ull << (32 / sizeof(T))));
  const auto indices = IndicesFromBits(d, mask_bits);
  return BitCast(d, TableLookupLanes(BitCast(du32, v), indices));
}

// LUTs are infeasible for 2^16 possible masks, so splice together two
// half-vector Compress.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<T> Compress(Vec256<T> v, const uint64_t mask_bits) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto vu16 = BitCast(du, v);  // (required for float16_t inputs)
  const Half<decltype(du)> duh;
  const auto half0 = LowerHalf(duh, vu16);
  const auto half1 = UpperHalf(duh, vu16);

  const uint64_t mask_bits0 = mask_bits & 0xFF;
  const uint64_t mask_bits1 = mask_bits >> 8;
  const auto compressed0 = detail::CompressBits(half0, mask_bits0);
  const auto compressed1 = detail::CompressBits(half1, mask_bits1);

  alignas(32) uint16_t all_true[16] = {};
  // Store mask=true lanes, left to right.
  const size_t num_true0 = PopCount(mask_bits0);
  Store(compressed0, duh, all_true);
  StoreU(compressed1, duh, all_true + num_true0);

  if (hwy::HWY_NAMESPACE::CompressIsPartition<T>::value) {
    // Store mask=false lanes, right to left. The second vector fills the upper
    // half with right-aligned false lanes. The first vector is shifted
    // rightwards to overwrite the true lanes of the second.
    alignas(32) uint16_t all_false[16] = {};
    const size_t num_true1 = PopCount(mask_bits1);
    Store(compressed1, duh, all_false + 8);
    StoreU(compressed0, duh, all_false + num_true1);

    const auto mask = FirstN(du, num_true0 + num_true1);
    return BitCast(d,
                   IfThenElse(mask, Load(du, all_true), Load(du, all_false)));
  } else {
    // Only care about the mask=true lanes.
    return BitCast(d, Load(du, all_true));
  }
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<T> CompressNot(Vec256<T> v, const uint64_t mask_bits) {
  const Full256<T> d;
  const Repartition<uint32_t, decltype(d)> du32;

  HWY_DASSERT(mask_bits < (1ull << (32 / sizeof(T))));
  const auto indices = IndicesFromNotBits(d, mask_bits);
  return BitCast(d, TableLookupLanes(BitCast(du32, v), indices));
}

// LUTs are infeasible for 2^16 possible masks, so splice together two
// half-vector Compress.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<T> CompressNot(Vec256<T> v, const uint64_t mask_bits) {
  // Compress ensures only the lower 16 bits are set, so flip those.
  return Compress(v, mask_bits ^ 0xFFFF);
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> m) {
  return detail::Compress(v, detail::BitsFromMask(m));
}

template <typename T>
HWY_API Vec256<T> CompressNot(Vec256<T> v, Mask256<T> m) {
  return detail::CompressNot(v, detail::BitsFromMask(m));
}

HWY_API Vec256<uint64_t> CompressBlocksNot(Vec256<uint64_t> v,
                                           Mask256<uint64_t> mask) {
  return CompressNot(v, mask);
}

template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <typename T>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                             T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(m);
  const size_t count = PopCount(mask_bits);
  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API size_t CompressBlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(m);
  const size_t count = PopCount(mask_bits);
  BlendedStore(detail::Compress(v, mask_bits), FirstN(d, count), d, unaligned);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CompressBlendedStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(m);
  const size_t count = PopCount(mask_bits);
  const Vec256<T> compressed = detail::Compress(v, mask_bits);

#if HWY_MEM_OPS_MIGHT_FAULT  // true if HWY_IS_MSAN
  // BlendedStore tests mask for each lane, but we know that the mask is
  // FirstN, so we can just copy.
  alignas(32) T buf[16];
  Store(compressed, d, buf);
  memcpy(unaligned, buf, count * sizeof(T));
#else
  BlendedStore(compressed, FirstN(d, count), d, unaligned);
#endif
  return count;
}

template <typename T>
HWY_API size_t CompressBitsStore(Vec256<T> v, const uint8_t* HWY_RESTRICT bits,
                                 Full256<T> d, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }
  const size_t count = PopCount(mask_bits);

  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ LoadInterleaved3/4

// Implemented in generic_ops, we just overload LoadTransposedBlocks3/4.

namespace detail {

// Input:
// 1 0 (<- first block of unaligned)
// 3 2
// 5 4
// Output:
// 3 0
// 4 1
// 5 2
template <typename T>
HWY_API void LoadTransposedBlocks3(Full256<T> d,
                                   const T* HWY_RESTRICT unaligned,
                                   Vec256<T>& A, Vec256<T>& B, Vec256<T>& C) {
  constexpr size_t N = 32 / sizeof(T);
  const Vec256<T> v10 = LoadU(d, unaligned + 0 * N);  // 1 0
  const Vec256<T> v32 = LoadU(d, unaligned + 1 * N);
  const Vec256<T> v54 = LoadU(d, unaligned + 2 * N);

  A = ConcatUpperLower(d, v32, v10);
  B = ConcatLowerUpper(d, v54, v10);
  C = ConcatUpperLower(d, v54, v32);
}

// Input (128-bit blocks):
// 1 0 (first block of unaligned)
// 3 2
// 5 4
// 7 6
// Output:
// 4 0 (LSB of A)
// 5 1
// 6 2
// 7 3
template <typename T>
HWY_API void LoadTransposedBlocks4(Full256<T> d,
                                   const T* HWY_RESTRICT unaligned,
                                   Vec256<T>& A, Vec256<T>& B, Vec256<T>& C,
                                   Vec256<T>& D) {
  constexpr size_t N = 32 / sizeof(T);
  const Vec256<T> v10 = LoadU(d, unaligned + 0 * N);
  const Vec256<T> v32 = LoadU(d, unaligned + 1 * N);
  const Vec256<T> v54 = LoadU(d, unaligned + 2 * N);
  const Vec256<T> v76 = LoadU(d, unaligned + 3 * N);

  A = ConcatLowerLower(d, v54, v10);
  B = ConcatUpperUpper(d, v54, v10);
  C = ConcatLowerLower(d, v76, v32);
  D = ConcatUpperUpper(d, v76, v32);
}

}  // namespace detail

// ------------------------------ StoreInterleaved2/3/4 (ConcatUpperLower)

// Implemented in generic_ops, we just overload StoreTransposedBlocks2/3/4.

namespace detail {

// Input (128-bit blocks):
// 2 0 (LSB of i)
// 3 1
// Output:
// 1 0
// 3 2
template <typename T>
HWY_API void StoreTransposedBlocks2(const Vec256<T> i, const Vec256<T> j,
                                    const Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatUpperUpper(d, j, i);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
}

// Input (128-bit blocks):
// 3 0 (LSB of i)
// 4 1
// 5 2
// Output:
// 1 0
// 3 2
// 5 4
template <typename T>
HWY_API void StoreTransposedBlocks3(const Vec256<T> i, const Vec256<T> j,
                                    const Vec256<T> k, Full256<T> d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatUpperLower(d, i, k);
  const auto out2 = ConcatUpperUpper(d, k, j);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
}

// Input (128-bit blocks):
// 4 0 (LSB of i)
// 5 1
// 6 2
// 7 3
// Output:
// 1 0
// 3 2
// 5 4
// 7 6
template <typename T>
HWY_API void StoreTransposedBlocks4(const Vec256<T> i, const Vec256<T> j,
                                    const Vec256<T> k, const Vec256<T> l,
                                    Full256<T> d, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  // Write lower halves, then upper.
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatLowerLower(d, l, k);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  const auto out2 = ConcatUpperUpper(d, j, i);
  const auto out3 = ConcatUpperUpper(d, l, k);
  StoreU(out2, d, unaligned + 2 * N);
  StoreU(out3, d, unaligned + 3 * N);
}

}  // namespace detail

// ------------------------------ Reductions

namespace detail {

// Returns sum{lane[i]} in each lane. "v3210" is a replicated 128-bit block.
// Same logic as x86/128.h, but with Vec256 arguments.
template <typename T>
HWY_INLINE Vec256<T> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const auto v1032 = Shuffle1032(v3210);
  const auto v31_20_31_20 = v3210 + v1032;
  const auto v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T>
HWY_INLINE Vec256<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const auto v1032 = Shuffle1032(v3210);
  const auto v31_20_31_20 = Min(v3210, v1032);
  const auto v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec256<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const auto v1032 = Shuffle1032(v3210);
  const auto v31_20_31_20 = Max(v3210, v1032);
  const auto v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

template <typename T>
HWY_INLINE Vec256<T> SumOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const auto v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T>
HWY_INLINE Vec256<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const auto v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec256<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const auto v01 = Shuffle01(v10);
  return Max(v10, v01);
}

// u16/i16
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> MinOfLanes(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  const Repartition<int32_t, Full256<T>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Full256<T>(), Or(min, ShiftLeft<16>(min)));
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> MaxOfLanes(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  const Repartition<int32_t, Full256<T>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Full256<T>(), Or(min, ShiftLeft<16>(min)));
}

}  // namespace detail

// Supported for {uif}32x8, {uif}64x4. Returns the sum in each lane.
template <typename T>
HWY_API Vec256<T> SumOfLanes(Full256<T> d, const Vec256<T> vHL) {
  const Vec256<T> vLH = ConcatLowerUpper(d, vHL, vHL);
  return detail::SumOfLanes(hwy::SizeTag<sizeof(T)>(), vLH + vHL);
}
template <typename T>
HWY_API Vec256<T> MinOfLanes(Full256<T> d, const Vec256<T> vHL) {
  const Vec256<T> vLH = ConcatLowerUpper(d, vHL, vHL);
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), Min(vLH, vHL));
}
template <typename T>
HWY_API Vec256<T> MaxOfLanes(Full256<T> d, const Vec256<T> vHL) {
  const Vec256<T> vLH = ConcatLowerUpper(d, vHL, vHL);
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), Max(vLH, vHL));
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// Note that the GCC warnings are not suppressed if we only wrap the *intrin.h -
// the warning seems to be issued at the call site of intrinsics, i.e. our code.
HWY_DIAGNOSTICS(pop)
