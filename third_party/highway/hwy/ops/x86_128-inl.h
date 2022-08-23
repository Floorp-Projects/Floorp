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

// 128-bit vectors and SSE4 instructions, plus some AVX2 and AVX512-VL
// operations when compiling for those targets.
// External include guard in highway.h - see comment there.

#include <emmintrin.h>
#include <stdio.h>
#if HWY_TARGET == HWY_SSSE3
#include <tmmintrin.h>  // SSSE3
#else
#include <smmintrin.h>  // SSE4
#include <wmmintrin.h>  // CLMUL
#endif
#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

#if HWY_IS_MSAN
#include <sanitizer/msan_interface.h>
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

#if HWY_TARGET <= HWY_AVX2
template <typename T>
using Full256 = Simd<T, 32 / sizeof(T), 0>;
#endif

#if HWY_TARGET <= HWY_AVX3
template <typename T>
using Full512 = Simd<T, 64 / sizeof(T), 0>;
#endif

namespace detail {

template <typename T>
struct Raw128 {
  using type = __m128i;
};
template <>
struct Raw128<float> {
  using type = __m128;
};
template <>
struct Raw128<double> {
  using type = __m128d;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T>::type;

 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec128& operator+=(const Vec128 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec128& operator-=(const Vec128 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec128& operator&=(const Vec128 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec128& operator|=(const Vec128 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec128& operator^=(const Vec128 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

template <typename T>
using Vec64 = Vec128<T, 8 / sizeof(T)>;

template <typename T>
using Vec32 = Vec128<T, 4 / sizeof(T)>;

#if HWY_TARGET <= HWY_AVX3

// Forward-declare for use by DeduceD, see below.
template <typename T>
class Vec512;

namespace detail {

// Template arg: sizeof(lane type)
template <size_t size>
struct RawMask128 {};
template <>
struct RawMask128<1> {
  using type = __mmask16;
};
template <>
struct RawMask128<2> {
  using type = __mmask8;
};
template <>
struct RawMask128<4> {
  using type = __mmask8;
};
template <>
struct RawMask128<8> {
  using type = __mmask8;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  using Raw = typename detail::RawMask128<sizeof(T)>::type;

  static Mask128<T, N> FromBits(uint64_t mask_bits) {
    return Mask128<T, N>{static_cast<Raw>(mask_bits)};
  }

  Raw raw;
};

#else  // AVX2 or below

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  typename detail::Raw128<T>::type raw;
};

#endif  // HWY_TARGET <= HWY_AVX3

#if HWY_TARGET <= HWY_AVX2
// Forward-declare for use by DeduceD, see below.
template <typename T>
class Vec256;
#endif

namespace detail {

// Deduce Simd<T, N, 0> from Vec*<T, N> (pointers because Vec256/512 may be
// incomplete types at this point; this is simpler than avoiding multiple
// definitions of DFromV via #if)
struct DeduceD {
  template <typename T, size_t N>
  Simd<T, N, 0> operator()(const Vec128<T, N>*) const {
    return Simd<T, N, 0>();
  }
#if HWY_TARGET <= HWY_AVX2
  template <typename T>
  Full256<T> operator()(const hwy::HWY_NAMESPACE::Vec256<T>*) const {
    return Full256<T>();
  }
#endif
#if HWY_TARGET <= HWY_AVX3
  template <typename T>
  Full512<T> operator()(const hwy::HWY_NAMESPACE::Vec512<T>*) const {
    return Full512<T>();
  }
#endif
};

// Workaround for MSVC v19.14: alias with a dependent type fails to specialize.
template <class V>
struct ExpandDFromV {
  using type = decltype(DeduceD()(static_cast<V*>(nullptr)));
};

}  // namespace detail

template <class V>
using DFromV = typename detail::ExpandDFromV<V>::type;

template <class V>
using TFromV = TFromD<DFromV<V>>;

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m128i BitCastToInteger(__m128i v) { return v; }
HWY_INLINE __m128i BitCastToInteger(__m128 v) { return _mm_castps_si128(v); }
HWY_INLINE __m128i BitCastToInteger(__m128d v) { return _mm_castpd_si128(v); }

template <typename T, size_t N>
HWY_INLINE Vec128<uint8_t, N * sizeof(T)> BitCastToByte(Vec128<T, N> v) {
  return Vec128<uint8_t, N * sizeof(T)>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger128 {
  HWY_INLINE __m128i operator()(__m128i v) { return v; }
};
template <>
struct BitCastFromInteger128<float> {
  HWY_INLINE __m128 operator()(__m128i v) { return _mm_castsi128_ps(v); }
};
template <>
struct BitCastFromInteger128<double> {
  HWY_INLINE __m128d operator()(__m128i v) { return _mm_castsi128_pd(v); }
};

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> BitCastFromByte(Simd<T, N, 0> /* tag */,
                                        Vec128<uint8_t, N * sizeof(T)> v) {
  return Vec128<T, N>{BitCastFromInteger128<T>()(v.raw)};
}

}  // namespace detail

template <typename T, size_t N, typename FromT>
HWY_API Vec128<T, N> BitCast(Simd<T, N, 0> d,
                             Vec128<FromT, N * sizeof(T) / sizeof(FromT)> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Zero

// Returns an all-zero vector/part.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Zero(Simd<T, N, 0> /* tag */) {
  return Vec128<T, N>{_mm_setzero_si128()};
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Zero(Simd<float, N, 0> /* tag */) {
  return Vec128<float, N>{_mm_setzero_ps()};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> Zero(Simd<double, N, 0> /* tag */) {
  return Vec128<double, N>{_mm_setzero_pd()};
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Set

// Returns a vector/part with all lanes set to "t".
template <size_t N, HWY_IF_LE128(uint8_t, N)>
HWY_API Vec128<uint8_t, N> Set(Simd<uint8_t, N, 0> /* tag */, const uint8_t t) {
  return Vec128<uint8_t, N>{_mm_set1_epi8(static_cast<char>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Set(Simd<uint16_t, N, 0> /* tag */,
                                const uint16_t t) {
  return Vec128<uint16_t, N>{_mm_set1_epi16(static_cast<short>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Set(Simd<uint32_t, N, 0> /* tag */,
                                const uint32_t t) {
  return Vec128<uint32_t, N>{_mm_set1_epi32(static_cast<int>(t))};
}
template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> Set(Simd<uint64_t, N, 0> /* tag */,
                                const uint64_t t) {
  return Vec128<uint64_t, N>{
      _mm_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(int8_t, N)>
HWY_API Vec128<int8_t, N> Set(Simd<int8_t, N, 0> /* tag */, const int8_t t) {
  return Vec128<int8_t, N>{_mm_set1_epi8(static_cast<char>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(int16_t, N)>
HWY_API Vec128<int16_t, N> Set(Simd<int16_t, N, 0> /* tag */, const int16_t t) {
  return Vec128<int16_t, N>{_mm_set1_epi16(static_cast<short>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(int32_t, N)>
HWY_API Vec128<int32_t, N> Set(Simd<int32_t, N, 0> /* tag */, const int32_t t) {
  return Vec128<int32_t, N>{_mm_set1_epi32(t)};
}
template <size_t N, HWY_IF_LE128(int64_t, N)>
HWY_API Vec128<int64_t, N> Set(Simd<int64_t, N, 0> /* tag */, const int64_t t) {
  return Vec128<int64_t, N>{
      _mm_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Set(Simd<float, N, 0> /* tag */, const float t) {
  return Vec128<float, N>{_mm_set1_ps(t)};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> Set(Simd<double, N, 0> /* tag */, const double t) {
  return Vec128<double, N>{_mm_set1_pd(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Undefined(Simd<T, N, 0> /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return Vec128<T, N>{_mm_undefined_si128()};
}
template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> Undefined(Simd<float, N, 0> /* tag */) {
  return Vec128<float, N>{_mm_undefined_ps()};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> Undefined(Simd<double, N, 0> /* tag */) {
  return Vec128<double, N>{_mm_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ GetLane

// Gets the single value stored in a vector/part.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(_mm_cvtsi128_si32(v.raw) & 0xFF);
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(_mm_cvtsi128_si32(v.raw) & 0xFFFF);
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(_mm_cvtsi128_si32(v.raw));
}
template <size_t N>
HWY_API float GetLane(const Vec128<float, N> v) {
  return _mm_cvtss_f32(v.raw);
}
template <size_t N>
HWY_API uint64_t GetLane(const Vec128<uint64_t, N> v) {
#if HWY_ARCH_X86_32
  alignas(16) uint64_t lanes[2];
  Store(v, Simd<uint64_t, N, 0>(), lanes);
  return lanes[0];
#else
  return static_cast<uint64_t>(_mm_cvtsi128_si64(v.raw));
#endif
}
template <size_t N>
HWY_API int64_t GetLane(const Vec128<int64_t, N> v) {
#if HWY_ARCH_X86_32
  alignas(16) int64_t lanes[2];
  Store(v, Simd<int64_t, N, 0>(), lanes);
  return lanes[0];
#else
  return _mm_cvtsi128_si64(v.raw);
#endif
}
template <size_t N>
HWY_API double GetLane(const Vec128<double, N> v) {
  return _mm_cvtsd_f64(v.raw);
}

// ================================================== LOGICAL

// ------------------------------ And

template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_and_si128(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> And(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_and_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> And(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> not_mask, Vec128<T, N> mask) {
  return Vec128<T, N>{_mm_andnot_si128(not_mask.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> AndNot(const Vec128<float, N> not_mask,
                                const Vec128<float, N> mask) {
  return Vec128<float, N>{_mm_andnot_ps(not_mask.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> AndNot(const Vec128<double, N> not_mask,
                                 const Vec128<double, N> mask) {
  return Vec128<double, N>{_mm_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_or_si128(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Or(const Vec128<float, N> a,
                            const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_or_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Or(const Vec128<double, N> a,
                             const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_xor_si128(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Xor(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_xor_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Xor(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Not

template <typename T, size_t N>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
#if HWY_TARGET <= HWY_AVX3
  const __m128i vu = BitCast(du, v).raw;
  return BitCast(d, VU{_mm_ternarylogic_epi32(vu, vu, vu, 0x55)});
#else
  return Xor(v, BitCast(d, VU{_mm_set1_epi32(-1)}));
#endif
}

// ------------------------------ Or3

template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(o1)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m128i ret = _mm_ternarylogic_epi64(
      BitCast(du, o1).raw, BitCast(du, o2).raw, BitCast(du, o3).raw, 0xFE);
  return BitCast(d, VU{ret});
#else
  return Or(o1, Or(o2, o3));
#endif
}

// ------------------------------ OrAnd

template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(o)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m128i ret = _mm_ternarylogic_epi64(
      BitCast(du, o).raw, BitCast(du, a1).raw, BitCast(du, a2).raw, 0xF8);
  return BitCast(d, VU{ret});
#else
  return Or(o, And(a1, a2));
#endif
}

// ------------------------------ IfVecThenElse

template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(no)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(
      d, VU{_mm_ternarylogic_epi64(BitCast(du, mask).raw, BitCast(du, yes).raw,
                                   BitCast(du, no).raw, 0xCA)});
#else
  return IfThenElse(MaskFromVec(mask), yes, no);
#endif
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
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

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<1> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi8(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<2> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi16(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<4> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi32(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<8> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi64(v.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> PopulationCount(Vec128<T, N> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

#endif  // HWY_TARGET == HWY_AVX3_DL

// ================================================== SIGN

// ------------------------------ Neg

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Neg(hwy::FloatTag /*tag*/, const Vec128<T, N> v) {
  return Xor(v, SignBit(DFromV<decltype(v)>()));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Neg(hwy::NonFloatTag /*tag*/, const Vec128<T, N> v) {
  return Zero(DFromV<decltype(v)>()) - v;
}

}  // namespace detail

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Neg(const Vec128<T, N> v) {
  return detail::Neg(hwy::IsFloatTag<T>(), v);
}

// ------------------------------ Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
template <size_t N>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
#if HWY_COMPILER_MSVC
  // Workaround for incorrect codegen? (reaches breakpoint)
  const auto zero = Zero(DFromV<decltype(v)>());
  return Vec128<int8_t, N>{_mm_max_epi8(v.raw, (zero - v).raw)};
#else
  return Vec128<int8_t, N>{_mm_abs_epi8(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_abs_epi16(v.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_abs_epi32(v.raw)};
}
// i64 is implemented after BroadcastSignBit.
template <size_t N>
HWY_API Vec128<float, N> Abs(const Vec128<float, N> v) {
  const Vec128<int32_t, N> mask{_mm_set1_epi32(0x7FFFFFFF)};
  return v & BitCast(DFromV<decltype(v)>(), mask);
}
template <size_t N>
HWY_API Vec128<double, N> Abs(const Vec128<double, N> v) {
  const Vec128<int64_t, N> mask{_mm_set1_epi64x(0x7FFFFFFFFFFFFFFFLL)};
  return v & BitCast(DFromV<decltype(v)>(), mask);
}

// ------------------------------ CopySign

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const DFromV<decltype(magn)> d;
  const auto msb = SignBit(d);

#if HWY_TARGET <= HWY_AVX3
  const RebindToUnsigned<decltype(d)> du;
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
  const __m128i out = _mm_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, VFromD<decltype(du)>{out});
#else
  return Or(AndNot(msb, magn), And(msb, sign));
#endif
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
#else
  return Or(abs, And(SignBit(DFromV<decltype(abs)>()), sign));
#endif
}

// ================================================== MASK

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ IfThenElse

// Returns mask ? b : a.

namespace detail {

// Templates for signed/unsigned integer of a particular size.
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<1> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi8(no.raw, mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<2> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi16(no.raw, mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<4> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi32(no.raw, mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<8> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi64(no.raw, mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return detail::IfThenElse(hwy::SizeTag<sizeof(T)>(), mask, yes, no);
}

template <size_t N>
HWY_API Vec128<float, N> IfThenElse(Mask128<float, N> mask,
                                    Vec128<float, N> yes, Vec128<float, N> no) {
  return Vec128<float, N>{_mm_mask_mov_ps(no.raw, mask.raw, yes.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> IfThenElse(Mask128<double, N> mask,
                                     Vec128<double, N> yes,
                                     Vec128<double, N> no) {
  return Vec128<double, N>{_mm_mask_mov_pd(no.raw, mask.raw, yes.raw)};
}

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<1> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi8(mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<2> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi16(mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<4> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi32(mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<8> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi64(mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return detail::IfThenElseZero(hwy::SizeTag<sizeof(T)>(), mask, yes);
}

template <size_t N>
HWY_API Vec128<float, N> IfThenElseZero(Mask128<float, N> mask,
                                        Vec128<float, N> yes) {
  return Vec128<float, N>{_mm_maskz_mov_ps(mask.raw, yes.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> IfThenElseZero(Mask128<double, N> mask,
                                         Vec128<double, N> yes) {
  return Vec128<double, N>{_mm_maskz_mov_pd(mask.raw, yes.raw)};
}

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<1> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  // xor_epi8/16 are missing, but we have sub, which is just as fast for u8/16.
  return Vec128<T, N>{_mm_mask_sub_epi8(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<2> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_sub_epi16(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<4> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_xor_epi32(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<8> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_xor_epi64(no.raw, mask.raw, no.raw, no.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return detail::IfThenZeroElse(hwy::SizeTag<sizeof(T)>(), mask, no);
}

template <size_t N>
HWY_API Vec128<float, N> IfThenZeroElse(Mask128<float, N> mask,
                                        Vec128<float, N> no) {
  return Vec128<float, N>{_mm_mask_xor_ps(no.raw, mask.raw, no.raw, no.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> IfThenZeroElse(Mask128<double, N> mask,
                                         Vec128<double, N> no) {
  return Vec128<double, N>{_mm_mask_xor_pd(no.raw, mask.raw, no.raw, no.raw)};
}

// ------------------------------ Mask logical

// For Clang and GCC, mask intrinsics (KORTEST) weren't added until recently.
#if !defined(HWY_COMPILER_HAS_MASK_INTRINSICS)
#if HWY_COMPILER_MSVC != 0 || HWY_COMPILER_GCC_ACTUAL >= 700 || \
    HWY_COMPILER_CLANG >= 800
#define HWY_COMPILER_HAS_MASK_INTRINSICS 1
#else
#define HWY_COMPILER_HAS_MASK_INTRINSICS 0
#endif
#endif  // HWY_COMPILER_HAS_MASK_INTRINSICS

namespace detail {

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(~a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(a.raw | b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(a.raw ^ b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::And(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::AndNot(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::Or(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::Xor(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  // Flip only the valid bits.
  // TODO(janwas): use _knot intrinsics if N >= 8.
  return Xor(m, Mask128<T, N>::FromBits((1ull << N) - 1));
}

#else  // AVX2 or below

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return Mask128<T, N>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Simd<T, N, 0> /* tag */,
                                 const Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

#if HWY_TARGET == HWY_SSSE3

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  const auto vmask = VecFromMask(DFromV<decltype(no)>(), mask);
  return Or(And(vmask, yes), AndNot(vmask, no));
}

#else  // HWY_TARGET == HWY_SSSE3

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return Vec128<T, N>{_mm_blendv_epi8(no.raw, yes.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> IfThenElse(const Mask128<float, N> mask,
                                    const Vec128<float, N> yes,
                                    const Vec128<float, N> no) {
  return Vec128<float, N>{_mm_blendv_ps(no.raw, yes.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> IfThenElse(const Mask128<double, N> mask,
                                     const Vec128<double, N> yes,
                                     const Vec128<double, N> no) {
  return Vec128<double, N>{_mm_blendv_pd(no.raw, yes.raw, mask.raw)};
}

#endif  // HWY_TARGET == HWY_SSSE3

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return yes & VecFromMask(DFromV<decltype(yes)>(), mask);
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return AndNot(VecFromMask(DFromV<decltype(no)>(), mask), no);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  return MaskFromVec(Not(VecFromMask(Simd<T, N, 0>(), m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ ShiftLeft

template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeft(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{_mm_slli_epi16(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeft(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{_mm_slli_epi32(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeft(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{_mm_slli_epi64(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftLeft(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_slli_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftLeft(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_slli_epi32(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftLeft(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{_mm_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeft(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{ShiftLeft<kBits>(Vec128<MakeWide<T>>{v.raw}).raw};
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftRight(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{_mm_srli_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftRight(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{_mm_srli_epi32(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftRight(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{_mm_srli_epi64(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> ShiftRight(const Vec128<uint8_t, N> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRight<kBits>(Vec128<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftRight(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_srai_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftRight(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_srai_epi32(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> ShiftRight(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// i64 is implemented after BroadcastSignBit.

// ================================================== SWIZZLE (1)

// ------------------------------ TableLookupBytes
template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T, N> bytes,
                                        const Vec128<TI, NI> from) {
  return Vec128<TI, NI>{_mm_shuffle_epi8(bytes.raw, from.raw)};
}

// ------------------------------ TableLookupBytesOr0
// For all vector widths; x86 anyway zeroes if >= 0x80.
template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, const VI from) {
  return TableLookupBytes(bytes, from);
}

// ------------------------------ Shuffles (ShiftRight, TableLookupBytes)

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{_mm_shuffle_epi32(v.raw, 0xB1)};
}
template <size_t N>
HWY_API Vec128<float, N> Shuffle2301(const Vec128<float, N> v) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<float, N>{_mm_shuffle_ps(v.raw, v.raw, 0xB1)};
}

// These are used by generic_ops-inl to implement LoadInterleaved3. As with
// Intel's shuffle* intrinsics and InterleaveLower, the lower half of the output
// comes from the first argument.
namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> Shuffle2301(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const Twice<DFromV<decltype(a)>> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {1, 0, 7, 6};
  return Vec128<T, 4>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, 4> Shuffle2301(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const Twice<DFromV<decltype(a)>> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0x0302, 0x0100, 0x0f0e, 0x0d0c};
  return Vec128<T, 4>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, 4> Shuffle2301(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(2, 3, 0, 1);
  return BitCast(d, Vec128<float, 4>{_mm_shuffle_ps(BitCast(df, a).raw,
                                                    BitCast(df, b).raw, m)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> Shuffle1230(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const Twice<DFromV<decltype(a)>> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0, 3, 6, 5};
  return Vec128<T, 4>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, 4> Shuffle1230(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const Twice<DFromV<decltype(a)>> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0x0100, 0x0706, 0x0d0c, 0x0b0a};
  return Vec128<T, 4>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, 4> Shuffle1230(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(1, 2, 3, 0);
  return BitCast(d, Vec128<float, 4>{_mm_shuffle_ps(BitCast(df, a).raw,
                                                    BitCast(df, b).raw, m)});
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, 4> Shuffle3012(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const Twice<DFromV<decltype(a)>> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {2, 1, 4, 7};
  return Vec128<T, 4>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, 4> Shuffle3012(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const Twice<DFromV<decltype(a)>> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0x0504, 0x0302, 0x0908, 0x0f0e};
  return Vec128<T, 4>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, 4> Shuffle3012(const Vec128<T, 4> a, const Vec128<T, 4> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(3, 0, 1, 2);
  return BitCast(d, Vec128<float, 4>{_mm_shuffle_ps(BitCast(df, a).raw,
                                                    BitCast(df, b).raw, m)});
}

}  // namespace detail

// Swap 64-bit halves
HWY_API Vec128<uint32_t> Shuffle1032(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<int32_t> Shuffle1032(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<float> Shuffle1032(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x4E)};
}
HWY_API Vec128<uint64_t> Shuffle01(const Vec128<uint64_t> v) {
  return Vec128<uint64_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<int64_t> Shuffle01(const Vec128<int64_t> v) {
  return Vec128<int64_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<double> Shuffle01(const Vec128<double> v) {
  return Vec128<double>{_mm_shuffle_pd(v.raw, v.raw, 1)};
}

// Rotate right 32 bits
HWY_API Vec128<uint32_t> Shuffle0321(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec128<int32_t> Shuffle0321(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec128<float> Shuffle0321(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x39)};
}
// Rotate left 32 bits
HWY_API Vec128<uint32_t> Shuffle2103(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec128<int32_t> Shuffle2103(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec128<float> Shuffle2103(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x93)};
}

// Reverse
HWY_API Vec128<uint32_t> Shuffle0123(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec128<int32_t> Shuffle0123(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec128<float> Shuffle0123(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x1B)};
}

// ================================================== COMPARE

#if HWY_TARGET <= HWY_AVX3

// Comparisons set a mask bit to 1 if the condition is true, else 0.

template <typename TFrom, size_t NFrom, typename TTo, size_t NTo>
HWY_API Mask128<TTo, NTo> RebindMask(Simd<TTo, NTo, 0> /*tag*/,
                                     Mask128<TFrom, NFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask128<TTo, NTo>{m.raw};
}

namespace detail {

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<1> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi8_mask(v.raw, bit.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<2> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi16_mask(v.raw, bit.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<4> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi32_mask(v.raw, bit.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<8> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi64_mask(v.raw, bit.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(const Vec128<T, N> v, const Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return detail::TestBit(hwy::SizeTag<sizeof(T)>(), v, bit);
}

// ------------------------------ Equality

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi8_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi16_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi32_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<float, N> operator==(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

template <size_t N>
HWY_API Mask128<double, N> operator==(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi8_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi16_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi32_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<float, N> operator!=(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

template <size_t N>
HWY_API Mask128<double, N> operator!=(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

// Signed/float <
template <size_t N>
HWY_API Mask128<int8_t, N> operator>(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpgt_epi8_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator>(Vec128<int16_t, N> a,
                                      Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpgt_epi16_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator>(Vec128<int32_t, N> a,
                                      Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpgt_epi32_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator>(Vec128<int64_t, N> a,
                                      Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{_mm_cmpgt_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<uint8_t, N> operator>(Vec128<uint8_t, N> a,
                                      Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{_mm_cmpgt_epu8_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator>(Vec128<uint16_t, N> a,
                                       Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{_mm_cmpgt_epu16_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator>(Vec128<uint32_t, N> a,
                                       Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{_mm_cmpgt_epu32_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator>(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{_mm_cmpgt_epu64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<float, N> operator>(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_GT_OQ)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>(Vec128<double, N> a, Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

template <size_t N>
HWY_API Mask128<float, N> operator>=(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_GE_OQ)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>=(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_GE_OQ)};
}

// ------------------------------ Mask

namespace detail {

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<1> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi8_mask(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<2> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi16_mask(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<4> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi32_mask(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<8> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi64_mask(v.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return detail::MaskFromVec(hwy::SizeTag<sizeof(T)>(), v);
}
// There do not seem to be native floating-point versions of these instructions.
template <size_t N>
HWY_API Mask128<float, N> MaskFromVec(const Vec128<float, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return Mask128<float, N>{MaskFromVec(BitCast(di, v)).raw};
}
template <size_t N>
HWY_API Mask128<double, N> MaskFromVec(const Vec128<double, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return Mask128<double, N>{MaskFromVec(BitCast(di, v)).raw};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi8(v.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi16(v.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi32(v.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi64(v.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> VecFromMask(const Mask128<float, N> v) {
  return Vec128<float, N>{_mm_castsi128_ps(_mm_movm_epi32(v.raw))};
}

template <size_t N>
HWY_API Vec128<double, N> VecFromMask(const Mask128<double, N> v) {
  return Vec128<double, N>{_mm_castsi128_pd(_mm_movm_epi64(v.raw))};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(Simd<T, N, 0> /* tag */,
                                 const Mask128<T, N> v) {
  return VecFromMask(v);
}

#else  // AVX2 or below

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <typename TFrom, typename TTo, size_t N>
HWY_API Mask128<TTo, N> RebindMask(Simd<TTo, N, 0> /*tag*/,
                                   Mask128<TFrom, N> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  const Simd<TFrom, N, 0> d;
  return MaskFromVec(BitCast(Simd<TTo, N, 0>(), VecFromMask(d, m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

// Unsigned
template <size_t N>
HWY_API Mask128<uint8_t, N> operator==(const Vec128<uint8_t, N> a,
                                       const Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{_mm_cmpeq_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator==(const Vec128<uint16_t, N> a,
                                        const Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{_mm_cmpeq_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator==(const Vec128<uint32_t, N> a,
                                        const Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{_mm_cmpeq_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const Simd<uint32_t, N * 2, 0> d32;
  const Simd<uint64_t, N, 0> d64;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
#else
  return Mask128<uint64_t, N>{_mm_cmpeq_epi64(a.raw, b.raw)};
#endif
}

// Signed
template <size_t N>
HWY_API Mask128<int8_t, N> operator==(const Vec128<int8_t, N> a,
                                      const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpeq_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator==(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpeq_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator==(const Vec128<int32_t, N> a,
                                       const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpeq_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  // Same as signed ==; avoid duplicating the SSSE3 version.
  const DFromV<decltype(a)> d;
  RebindToUnsigned<decltype(d)> du;
  return RebindMask(d, BitCast(du, a) == BitCast(du, b));
}

// Float
template <size_t N>
HWY_API Mask128<float, N> operator==(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpeq_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator==(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpeq_pd(a.raw, b.raw)};
}

// ------------------------------ Inequality

// This cannot have T as a template argument, otherwise it is not more
// specialized than rewritten operator== in C++20, leading to compile
// errors: https://gcc.godbolt.org/z/xsrPhPvPT.
template <size_t N>
HWY_API Mask128<uint8_t, N> operator!=(Vec128<uint8_t, N> a,
                                       Vec128<uint8_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator!=(Vec128<uint16_t, N> a,
                                       Vec128<uint16_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator!=(Vec128<uint32_t, N> a,
                                       Vec128<uint32_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator!=(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int8_t, N> operator!=(Vec128<int8_t, N> a,
                                      Vec128<int8_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator!=(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator!=(Vec128<int32_t, N> a,
                                       Vec128<int32_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator!=(Vec128<int64_t, N> a,
                                       Vec128<int64_t, N> b) {
  return Not(a == b);
}

template <size_t N>
HWY_API Mask128<float, N> operator!=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpneq_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator!=(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpneq_pd(a.raw, b.raw)};
}

// ------------------------------ Strict inequality

namespace detail {

template <size_t N>
HWY_INLINE Mask128<int8_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int8_t, N> a,
                                 Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpgt_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int16_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int16_t, N> a,
                                  Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpgt_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int32_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int32_t, N> a,
                                  Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpgt_epi32(a.raw, b.raw)};
}

template <size_t N>
HWY_INLINE Mask128<int64_t, N> Gt(hwy::SignedTag /*tag*/,
                                  const Vec128<int64_t, N> a,
                                  const Vec128<int64_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  // See https://stackoverflow.com/questions/65166174/:
  const Simd<int64_t, N, 0> d;
  const RepartitionToNarrow<decltype(d)> d32;
  const Vec128<int64_t, N> m_eq32{Eq(BitCast(d32, a), BitCast(d32, b)).raw};
  const Vec128<int64_t, N> m_gt32{Gt(BitCast(d32, a), BitCast(d32, b)).raw};
  // If a.upper is greater, upper := true. Otherwise, if a.upper == b.upper:
  // upper := b-a (unsigned comparison result of lower). Otherwise: upper := 0.
  const __m128i upper = OrAnd(m_gt32, m_eq32, Sub(b, a)).raw;
  // Duplicate upper to lower half.
  return Mask128<int64_t, N>{_mm_shuffle_epi32(upper, _MM_SHUFFLE(3, 3, 1, 1))};
#else
  return Mask128<int64_t, N>{_mm_cmpgt_epi64(a.raw, b.raw)};  // SSE4.2
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Gt(hwy::UnsignedTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  const DFromV<decltype(a)> du;
  const RebindToSigned<decltype(du)> di;
  const Vec128<T, N> msb = Set(du, (LimitsMax<T>() >> 1) + 1);
  const auto sa = BitCast(di, Xor(a, msb));
  const auto sb = BitCast(di, Xor(b, msb));
  return RebindMask(du, Gt(hwy::SignedTag(), sa, sb));
}

template <size_t N>
HWY_INLINE Mask128<float, N> Gt(hwy::FloatTag /*tag*/, Vec128<float, N> a,
                                Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpgt_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<double, N> Gt(hwy::FloatTag /*tag*/, Vec128<double, N> a,
                                 Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpgt_pd(a.raw, b.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Gt(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Weak inequality

template <size_t N>
HWY_API Mask128<float, N> operator>=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpge_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>=(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpge_pd(a.raw, b.raw)};
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ Reversed comparisons

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<(Vec128<T, N> a, Vec128<T, N> b) {
  return b > a;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<=(Vec128<T, N> a, Vec128<T, N> b) {
  return b >= a;
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> FirstN(const Simd<T, N, 0> d, size_t num) {
#if HWY_TARGET <= HWY_AVX3
  (void)d;
  const uint64_t all = (1ull << N) - 1;
  // BZHI only looks at the lower 8 bits of num!
  const uint64_t bits = (num > 255) ? all : _bzhi_u64(all, num);
  return Mask128<T, N>::FromBits(bits);
#else
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
#endif
}

template <class D>
using MFromD = decltype(FirstN(D(), 0));

// ================================================== MEMORY (1)

// Clang static analysis claims the memory immediately after a partial vector
// store is uninitialized, and also flags the input to partial loads (at least
// for loadl_pd) as "garbage". This is a false alarm because msan does not
// raise errors. We work around this by using CopyBytes instead of intrinsics,
// but only for the analyzer to avoid potentially bad code generation.
// Unfortunately __clang_analyzer__ was not defined for clang-tidy prior to v7.
#ifndef HWY_SAFE_PARTIAL_LOAD_STORE
#if defined(__clang_analyzer__) || \
    (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 700)
#define HWY_SAFE_PARTIAL_LOAD_STORE 1
#else
#define HWY_SAFE_PARTIAL_LOAD_STORE 0
#endif
#endif  // HWY_SAFE_PARTIAL_LOAD_STORE

// ------------------------------ Load

template <typename T>
HWY_API Vec128<T> Load(Full128<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec128<T>{_mm_load_si128(reinterpret_cast<const __m128i*>(aligned))};
}
HWY_API Vec128<float> Load(Full128<float> /* tag */,
                           const float* HWY_RESTRICT aligned) {
  return Vec128<float>{_mm_load_ps(aligned)};
}
HWY_API Vec128<double> Load(Full128<double> /* tag */,
                            const double* HWY_RESTRICT aligned) {
  return Vec128<double>{_mm_load_pd(aligned)};
}

template <typename T>
HWY_API Vec128<T> LoadU(Full128<T> /* tag */, const T* HWY_RESTRICT p) {
  return Vec128<T>{_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))};
}
HWY_API Vec128<float> LoadU(Full128<float> /* tag */,
                            const float* HWY_RESTRICT p) {
  return Vec128<float>{_mm_loadu_ps(p)};
}
HWY_API Vec128<double> LoadU(Full128<double> /* tag */,
                             const double* HWY_RESTRICT p) {
  return Vec128<double>{_mm_loadu_pd(p)};
}

template <typename T>
HWY_API Vec64<T> Load(Full64<T> /* tag */, const T* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128i v = _mm_setzero_si128();
  CopyBytes<8>(p, &v);
  return Vec64<T>{v};
#else
  return Vec64<T>{_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p))};
#endif
}

HWY_API Vec128<float, 2> Load(Full64<float> /* tag */,
                              const float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<8>(p, &v);
  return Vec128<float, 2>{v};
#else
  const __m128 hi = _mm_setzero_ps();
  return Vec128<float, 2>{_mm_loadl_pi(hi, reinterpret_cast<const __m64*>(p))};
#endif
}

HWY_API Vec64<double> Load(Full64<double> /* tag */,
                           const double* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128d v = _mm_setzero_pd();
  CopyBytes<8>(p, &v);
  return Vec64<double>{v};
#else
  return Vec64<double>{_mm_load_sd(p)};
#endif
}

HWY_API Vec128<float, 1> Load(Full32<float> /* tag */,
                              const float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<4>(p, &v);
  return Vec128<float, 1>{v};
#else
  return Vec128<float, 1>{_mm_load_ss(p)};
#endif
}

// Any <= 32 bit except <float, 1>
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> Load(Simd<T, N, 0> /* tag */, const T* HWY_RESTRICT p) {
  constexpr size_t kSize = sizeof(T) * N;
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<kSize>(p, &v);
  return Vec128<T, N>{v};
#else
  int32_t bits = 0;
  CopyBytes<kSize>(p, &bits);
  return Vec128<T, N>{_mm_cvtsi32_si128(bits)};
#endif
}

// For < 128 bit, LoadU == Load.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> LoadU(Simd<T, N, 0> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> LoadDup128(Simd<T, N, 0> d, const T* HWY_RESTRICT p) {
  return LoadU(d, p);
}

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, size_t N, typename T2, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Iota(const Simd<T, N, 0> d, const T2 first) {
  HWY_ALIGN T lanes[16 / sizeof(T)];
  for (size_t i = 0; i < 16 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ------------------------------ MaskedLoad

#if HWY_TARGET <= HWY_AVX3

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> /* tag */,
                                const T* HWY_RESTRICT p) {
  return Vec128<T, N>{_mm_maskz_loadu_epi8(m.raw, p)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> /* tag */,
                                const T* HWY_RESTRICT p) {
  return Vec128<T, N>{_mm_maskz_loadu_epi16(m.raw, p)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> /* tag */,
                                const T* HWY_RESTRICT p) {
  return Vec128<T, N>{_mm_maskz_loadu_epi32(m.raw, p)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> /* tag */,
                                const T* HWY_RESTRICT p) {
  return Vec128<T, N>{_mm_maskz_loadu_epi64(m.raw, p)};
}

template <size_t N>
HWY_API Vec128<float, N> MaskedLoad(Mask128<float, N> m,
                                    Simd<float, N, 0> /* tag */,
                                    const float* HWY_RESTRICT p) {
  return Vec128<float, N>{_mm_maskz_loadu_ps(m.raw, p)};
}

template <size_t N>
HWY_API Vec128<double, N> MaskedLoad(Mask128<double, N> m,
                                     Simd<double, N, 0> /* tag */,
                                     const double* HWY_RESTRICT p) {
  return Vec128<double, N>{_mm_maskz_loadu_pd(m.raw, p)};
}

#elif HWY_TARGET == HWY_AVX2

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> /* tag */,
                                const T* HWY_RESTRICT p) {
  auto p_p = reinterpret_cast<const int*>(p);  // NOLINT
  return Vec128<T, N>{_mm_maskload_epi32(p_p, m.raw)};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> /* tag */,
                                const T* HWY_RESTRICT p) {
  auto p_p = reinterpret_cast<const long long*>(p);  // NOLINT
  return Vec128<T, N>{_mm_maskload_epi64(p_p, m.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> MaskedLoad(Mask128<float, N> m, Simd<float, N, 0> d,
                                    const float* HWY_RESTRICT p) {
  const Vec128<int32_t, N> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  return Vec128<float, N>{_mm_maskload_ps(p, mi.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> MaskedLoad(Mask128<double, N> m, Simd<double, N, 0> d,
                                     const double* HWY_RESTRICT p) {
  const Vec128<int64_t, N> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  return Vec128<double, N>{_mm_maskload_pd(p, mi.raw)};
}

// There is no maskload_epi8/16, so blend instead.
template <typename T, size_t N, hwy::EnableIf<sizeof(T) <= 2>* = nullptr>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> d,
                                const T* HWY_RESTRICT p) {
  return IfThenElseZero(m, Load(d, p));
}

#else  // <= SSE4

// Avoid maskmov* - its nontemporal 'hint' causes it to bypass caches (slow).
template <typename T, size_t N>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N, 0> d,
                                const T* HWY_RESTRICT p) {
  return IfThenElseZero(m, Load(d, p));
}

#endif

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec128<T> v, Full128<T> /* tag */, T* HWY_RESTRICT aligned) {
  _mm_store_si128(reinterpret_cast<__m128i*>(aligned), v.raw);
}
HWY_API void Store(const Vec128<float> v, Full128<float> /* tag */,
                   float* HWY_RESTRICT aligned) {
  _mm_store_ps(aligned, v.raw);
}
HWY_API void Store(const Vec128<double> v, Full128<double> /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm_store_pd(aligned, v.raw);
}

template <typename T>
HWY_API void StoreU(Vec128<T> v, Full128<T> /* tag */, T* HWY_RESTRICT p) {
  _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v.raw);
}
HWY_API void StoreU(const Vec128<float> v, Full128<float> /* tag */,
                    float* HWY_RESTRICT p) {
  _mm_storeu_ps(p, v.raw);
}
HWY_API void StoreU(const Vec128<double> v, Full128<double> /* tag */,
                    double* HWY_RESTRICT p) {
  _mm_storeu_pd(p, v.raw);
}

template <typename T>
HWY_API void Store(Vec64<T> v, Full64<T> /* tag */, T* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);
#else
  _mm_storel_epi64(reinterpret_cast<__m128i*>(p), v.raw);
#endif
}
HWY_API void Store(const Vec128<float, 2> v, Full64<float> /* tag */,
                   float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);
#else
  _mm_storel_pi(reinterpret_cast<__m64*>(p), v.raw);
#endif
}
HWY_API void Store(const Vec64<double> v, Full64<double> /* tag */,
                   double* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);
#else
  _mm_storel_pd(p, v.raw);
#endif
}

// Any <= 32 bit except <float, 1>
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API void Store(Vec128<T, N> v, Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  CopyBytes<sizeof(T) * N>(&v, p);
}
HWY_API void Store(const Vec128<float, 1> v, Full32<float> /* tag */,
                   float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<4>(&v, p);
#else
  _mm_store_ss(p, v.raw);
#endif
}

// For < 128 bit, StoreU == Store.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API void StoreU(const Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ------------------------------ BlendedStore

namespace detail {

// There is no maskload_epi8/16 with which we could safely implement
// BlendedStore. Manual blending is also unsafe because loading a full vector
// that crosses the array end causes asan faults. Resort to scalar code; the
// caller should instead use memcpy, assuming m is FirstN(d, n).
template <typename T, size_t N>
HWY_API void ScalarMaskedStore(Vec128<T, N> v, Mask128<T, N> m, Simd<T, N, 0> d,
                               T* HWY_RESTRICT p) {
  const RebindToSigned<decltype(d)> di;  // for testing mask if T=bfloat16_t.
  using TI = TFromD<decltype(di)>;
  alignas(16) TI buf[N];
  alignas(16) TI mask[N];
  Store(BitCast(di, v), di, buf);
  Store(BitCast(di, VecFromMask(d, m)), di, mask);
  for (size_t i = 0; i < N; ++i) {
    if (mask[i]) {
      CopyBytes<sizeof(T)>(buf + i, p + i);
    }
  }
}
}  // namespace detail

#if HWY_TARGET <= HWY_AVX3

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  _mm_mask_storeu_epi8(p, m.raw, v.raw);
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  _mm_mask_storeu_epi16(p, m.raw, v.raw);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<int*>(p);  // NOLINT
  _mm_mask_storeu_epi32(pi, m.raw, v.raw);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<long long*>(p);  // NOLINT
  _mm_mask_storeu_epi64(pi, m.raw, v.raw);
}

template <size_t N>
HWY_API void BlendedStore(Vec128<float, N> v, Mask128<float, N> m,
                          Simd<float, N, 0>, float* HWY_RESTRICT p) {
  _mm_mask_storeu_ps(p, m.raw, v.raw);
}

template <size_t N>
HWY_API void BlendedStore(Vec128<double, N> v, Mask128<double, N> m,
                          Simd<double, N, 0>, double* HWY_RESTRICT p) {
  _mm_mask_storeu_pd(p, m.raw, v.raw);
}

#elif HWY_TARGET == HWY_AVX2

template <typename T, size_t N, hwy::EnableIf<sizeof(T) <= 2>* = nullptr>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m, Simd<T, N, 0> d,
                          T* HWY_RESTRICT p) {
  detail::ScalarMaskedStore(v, m, d, p);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  // For partial vectors, avoid writing other lanes by zeroing their mask.
  if (N < 4) {
    const Full128<T> df;
    const Mask128<T> mf{m.raw};
    m = Mask128<T, N>{And(mf, FirstN(df, N)).raw};
  }

  auto pi = reinterpret_cast<int*>(p);  // NOLINT
  _mm_maskstore_epi32(pi, m.raw, v.raw);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                          Simd<T, N, 0> /* tag */, T* HWY_RESTRICT p) {
  // For partial vectors, avoid writing other lanes by zeroing their mask.
  if (N < 2) {
    const Full128<T> df;
    const Mask128<T> mf{m.raw};
    m = Mask128<T, N>{And(mf, FirstN(df, N)).raw};
  }

  auto pi = reinterpret_cast<long long*>(p);  // NOLINT
  _mm_maskstore_epi64(pi, m.raw, v.raw);
}

template <size_t N>
HWY_API void BlendedStore(Vec128<float, N> v, Mask128<float, N> m,
                          Simd<float, N, 0> d, float* HWY_RESTRICT p) {
  using T = float;
  // For partial vectors, avoid writing other lanes by zeroing their mask.
  if (N < 4) {
    const Full128<T> df;
    const Mask128<T> mf{m.raw};
    m = Mask128<T, N>{And(mf, FirstN(df, N)).raw};
  }

  const Vec128<MakeSigned<T>, N> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  _mm_maskstore_ps(p, mi.raw, v.raw);
}

template <size_t N>
HWY_API void BlendedStore(Vec128<double, N> v, Mask128<double, N> m,
                          Simd<double, N, 0> d, double* HWY_RESTRICT p) {
  using T = double;
  // For partial vectors, avoid writing other lanes by zeroing their mask.
  if (N < 2) {
    const Full128<T> df;
    const Mask128<T> mf{m.raw};
    m = Mask128<T, N>{And(mf, FirstN(df, N)).raw};
  }

  const Vec128<MakeSigned<T>, N> mi =
      BitCast(RebindToSigned<decltype(d)>(), VecFromMask(d, m));
  _mm_maskstore_pd(p, mi.raw, v.raw);
}

#else  // <= SSE4

template <typename T, size_t N>
HWY_API void BlendedStore(Vec128<T, N> v, Mask128<T, N> m, Simd<T, N, 0> d,
                          T* HWY_RESTRICT p) {
  // Avoid maskmov* - its nontemporal 'hint' causes it to bypass caches (slow).
  detail::ScalarMaskedStore(v, m, d, p);
}

#endif  // SSE4

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator+(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_add_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator+(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_add_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator+(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_add_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator+(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_add_epi64(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator+(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_add_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator+(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_add_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator+(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_add_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator+(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_add_epi64(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator+(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_add_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator+(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator-(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_sub_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator-(Vec128<uint16_t, N> a,
                                      Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_sub_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator-(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_sub_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator-(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_sub_epi64(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator-(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_sub_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator-(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_sub_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator-(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_sub_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator-(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_sub_epi64(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator-(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_sub_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator-(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_sub_pd(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
template <size_t N>
HWY_API Vec128<uint64_t, N / 8> SumsOf8(const Vec128<uint8_t, N> v) {
  return Vec128<uint64_t, N / 8>{_mm_sad_epu8(v.raw, _mm_setzero_si128())};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedAdd(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_adds_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedAdd(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_adds_epu16(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedAdd(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_adds_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedAdd(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_adds_epi16(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedSub(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_subs_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedSub(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_subs_epu16(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedSub(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_subs_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedSub(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_subs_epi16(a.raw, b.raw)};
}

// ------------------------------ AverageRound

// Returns (a + b + 1) / 2

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> AverageRound(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_avg_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> AverageRound(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Integer multiplication

template <size_t N>
HWY_API Vec128<uint16_t, N> operator*(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_mullo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator*(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mullo_epi16(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_mulhi_epu16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mulhi_epi16(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(const Vec128<int16_t, N> a,
                                           const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mulhrs_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(const Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{_mm_mul_epu32(a.raw, b.raw)};
}

#if HWY_TARGET == HWY_SSSE3

template <size_t N, HWY_IF_LE64(int32_t, N)>  // N=1 or 2
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  return Set(Simd<int64_t, (N + 1) / 2, 0>(),
             static_cast<int64_t>(GetLane(a)) * GetLane(b));
}
HWY_API Vec128<int64_t> MulEven(const Vec128<int32_t> a,
                                const Vec128<int32_t> b) {
  alignas(16) int32_t a_lanes[4];
  alignas(16) int32_t b_lanes[4];
  const Full128<int32_t> di32;
  Store(a, di32, a_lanes);
  Store(b, di32, b_lanes);
  alignas(16) int64_t mul[2];
  mul[0] = static_cast<int64_t>(a_lanes[0]) * b_lanes[0];
  mul[1] = static_cast<int64_t>(a_lanes[2]) * b_lanes[2];
  return Load(Full128<int64_t>(), mul);
}

#else  // HWY_TARGET == HWY_SSSE3

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{_mm_mul_epi32(a.raw, b.raw)};
}

#endif  // HWY_TARGET == HWY_SSSE3

template <size_t N>
HWY_API Vec128<uint32_t, N> operator*(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  // Not as inefficient as it looks: _mm_mullo_epi32 has 10 cycle latency.
  // 64-bit right shift would also work but also needs port 5, so no benefit.
  // Notation: x=don't care, z=0.
  const __m128i a_x3x1 = _mm_shuffle_epi32(a.raw, _MM_SHUFFLE(3, 3, 1, 1));
  const auto mullo_x2x0 = MulEven(a, b);
  const __m128i b_x3x1 = _mm_shuffle_epi32(b.raw, _MM_SHUFFLE(3, 3, 1, 1));
  const auto mullo_x3x1 =
      MulEven(Vec128<uint32_t, N>{a_x3x1}, Vec128<uint32_t, N>{b_x3x1});
  // We could _mm_slli_epi64 by 32 to get 3z1z and OR with z2z0, but generating
  // the latter requires one more instruction or a constant.
  const __m128i mul_20 =
      _mm_shuffle_epi32(mullo_x2x0.raw, _MM_SHUFFLE(2, 0, 2, 0));
  const __m128i mul_31 =
      _mm_shuffle_epi32(mullo_x3x1.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<uint32_t, N>{_mm_unpacklo_epi32(mul_20, mul_31)};
#else
  return Vec128<uint32_t, N>{_mm_mullo_epi32(a.raw, b.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator*(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  // Same as unsigned; avoid duplicating the SSSE3 code.
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, BitCast(du, a) * BitCast(du, b));
}

// ------------------------------ RotateRight (ShiftRight, Or)

template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> RotateRight(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint32_t, N>{_mm_ror_epi32(v.raw, kBits)};
#else
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(31, 32 - kBits)>(v));
#endif
}

template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> RotateRight(const Vec128<uint64_t, N> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_ror_epi64(v.raw, kBits)};
#else
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(63, 64 - kBits)>(v));
#endif
}

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

template <size_t N>
HWY_API Vec128<int8_t, N> BroadcastSignBit(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  return VecFromMask(v < Zero(d));
}

template <size_t N>
HWY_API Vec128<int16_t, N> BroadcastSignBit(const Vec128<int16_t, N> v) {
  return ShiftRight<15>(v);
}

template <size_t N>
HWY_API Vec128<int32_t, N> BroadcastSignBit(const Vec128<int32_t, N> v) {
  return ShiftRight<31>(v);
}

template <size_t N>
HWY_API Vec128<int64_t, N> BroadcastSignBit(const Vec128<int64_t, N> v) {
  const DFromV<decltype(v)> d;
#if HWY_TARGET <= HWY_AVX3
  (void)d;
  return Vec128<int64_t, N>{_mm_srai_epi64(v.raw, 63)};
#elif HWY_TARGET == HWY_AVX2 || HWY_TARGET == HWY_SSE4
  return VecFromMask(v < Zero(d));
#else
  // Efficient Lt() requires SSE4.2 and BLENDVPD requires SSE4.1. 32-bit shift
  // avoids generating a zero.
  const RepartitionToNarrow<decltype(d)> d32;
  const auto sign = ShiftRight<31>(BitCast(d32, v));
  return Vec128<int64_t, N>{
      _mm_shuffle_epi32(sign.raw, _MM_SHUFFLE(3, 3, 1, 1))};
#endif
}

template <size_t N>
HWY_API Vec128<int64_t, N> Abs(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_abs_epi64(v.raw)};
#else
  const auto zero = Zero(DFromV<decltype(v)>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftRight(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_srai_epi64(v.raw, kBits)};
#else
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto right = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto sign = ShiftLeft<64 - kBits>(BroadcastSignBit(v));
  return right | sign;
#endif
}

// ------------------------------ ZeroIfNegative (BroadcastSignBit)
template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only works for float");
  const DFromV<decltype(v)> d;
#if HWY_TARGET == HWY_SSSE3
  const RebindToSigned<decltype(d)> di;
  const auto mask = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
#else
  const auto mask = MaskFromVec(v);  // MSB is sufficient for BLENDVPS
#endif
  return IfThenElse(mask, Zero(d), v);
}

// ------------------------------ IfNegativeThenElse
template <size_t N>
HWY_API Vec128<int8_t, N> IfNegativeThenElse(const Vec128<int8_t, N> v,
                                             const Vec128<int8_t, N> yes,
                                             const Vec128<int8_t, N> no) {
  // int8: IfThenElse only looks at the MSB.
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;

  // 16-bit: no native blendv, so copy sign to lower byte's MSB.
  v = BitCast(d, BroadcastSignBit(BitCast(di, v)));
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(v)> d;
  const RebindToFloat<decltype(d)> df;

  // 32/64-bit: use float IfThenElse, which only looks at the MSB.
  return BitCast(d, IfThenElse(MaskFromVec(BitCast(df, v)), BitCast(df, yes),
                               BitCast(df, no)));
}

// ------------------------------ ShiftLeftSame

template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeftSame(const Vec128<uint16_t, N> v,
                                          const int bits) {
  return Vec128<uint16_t, N>{_mm_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeftSame(const Vec128<uint32_t, N> v,
                                          const int bits) {
  return Vec128<uint32_t, N>{_mm_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeftSame(const Vec128<uint64_t, N> v,
                                          const int bits) {
  return Vec128<uint64_t, N>{_mm_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int16_t, N> ShiftLeftSame(const Vec128<int16_t, N> v,
                                         const int bits) {
  return Vec128<int16_t, N>{_mm_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> ShiftLeftSame(const Vec128<int32_t, N> v,
                                         const int bits) {
  return Vec128<int32_t, N>{_mm_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int64_t, N> ShiftLeftSame(const Vec128<int64_t, N> v,
                                         const int bits) {
  return Vec128<int64_t, N>{_mm_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, const int bits) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{
      ShiftLeftSame(Vec128<MakeWide<T>>{v.raw}, bits).raw};
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

// ------------------------------ ShiftRightSame (BroadcastSignBit)

template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftRightSame(const Vec128<uint16_t, N> v,
                                           const int bits) {
  return Vec128<uint16_t, N>{_mm_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftRightSame(const Vec128<uint32_t, N> v,
                                           const int bits) {
  return Vec128<uint32_t, N>{_mm_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftRightSame(const Vec128<uint64_t, N> v,
                                           const int bits) {
  return Vec128<uint64_t, N>{_mm_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> ShiftRightSame(Vec128<uint8_t, N> v,
                                          const int bits) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRightSame(Vec128<uint16_t>{v.raw}, bits).raw};
  return shifted & Set(d8, static_cast<uint8_t>(0xFF >> bits));
}

template <size_t N>
HWY_API Vec128<int16_t, N> ShiftRightSame(const Vec128<int16_t, N> v,
                                          const int bits) {
  return Vec128<int16_t, N>{_mm_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> ShiftRightSame(const Vec128<int32_t, N> v,
                                          const int bits) {
  return Vec128<int32_t, N>{_mm_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<int64_t, N> ShiftRightSame(const Vec128<int64_t, N> v,
                                          const int bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
#else
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto right = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto sign = ShiftLeftSame(BroadcastSignBit(v), 64 - bits);
  return right | sign;
#endif
}

template <size_t N>
HWY_API Vec128<int8_t, N> ShiftRightSame(Vec128<int8_t, N> v, const int bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign =
      BitCast(di, Set(du, static_cast<uint8_t>(0x80 >> bits)));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Floating-point mul / div

template <size_t N>
HWY_API Vec128<float, N> operator*(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{_mm_mul_ps(a.raw, b.raw)};
}
HWY_API Vec128<float, 1> operator*(const Vec128<float, 1> a,
                                   const Vec128<float, 1> b) {
  return Vec128<float, 1>{_mm_mul_ss(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator*(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_mul_pd(a.raw, b.raw)};
}
HWY_API Vec64<double> operator*(const Vec64<double> a, const Vec64<double> b) {
  return Vec64<double>{_mm_mul_sd(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_div_ps(a.raw, b.raw)};
}
HWY_API Vec128<float, 1> operator/(const Vec128<float, 1> a,
                                   const Vec128<float, 1> b) {
  return Vec128<float, 1>{_mm_div_ss(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator/(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_div_pd(a.raw, b.raw)};
}
HWY_API Vec64<double> operator/(const Vec64<double> a, const Vec64<double> b) {
  return Vec64<double>{_mm_div_sd(a.raw, b.raw)};
}

// Approximate reciprocal
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_rcp_ps(v.raw)};
}
HWY_API Vec128<float, 1> ApproximateReciprocal(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_rcp_ss(v.raw)};
}

// Absolute value of difference.
template <size_t N>
HWY_API Vec128<float, N> AbsDiff(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
template <size_t N>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x + add;
#else
  return Vec128<float, N>{_mm_fmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> MulAdd(const Vec128<double, N> mul,
                                 const Vec128<double, N> x,
                                 const Vec128<double, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x + add;
#else
  return Vec128<double, N>{_mm_fmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns add - mul * x
template <size_t N>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return add - mul * x;
#else
  return Vec128<float, N>{_mm_fnmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> NegMulAdd(const Vec128<double, N> mul,
                                    const Vec128<double, N> x,
                                    const Vec128<double, N> add) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return add - mul * x;
#else
  return Vec128<double, N>{_mm_fnmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> MulSub(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x - sub;
#else
  return Vec128<float, N>{_mm_fmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> MulSub(const Vec128<double, N> mul,
                                 const Vec128<double, N> x,
                                 const Vec128<double, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return mul * x - sub;
#else
  return Vec128<double, N>{_mm_fmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// Returns -mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> NegMulSub(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return Neg(mul) * x - sub;
#else
  return Vec128<float, N>{_mm_fnmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> NegMulSub(const Vec128<double, N> mul,
                                    const Vec128<double, N> x,
                                    const Vec128<double, N> sub) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return Neg(mul) * x - sub;
#else
  return Vec128<double, N>{_mm_fnmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// ------------------------------ Floating-point square root

// Full precision square root
template <size_t N>
HWY_API Vec128<float, N> Sqrt(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_sqrt_ps(v.raw)};
}
HWY_API Vec128<float, 1> Sqrt(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_sqrt_ss(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Sqrt(const Vec128<double, N> v) {
  return Vec128<double, N>{_mm_sqrt_pd(v.raw)};
}
HWY_API Vec64<double> Sqrt(const Vec64<double> v) {
  return Vec64<double>{_mm_sqrt_sd(_mm_setzero_pd(), v.raw)};
}

// Approximate reciprocal square root
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_rsqrt_ps(v.raw)};
}
HWY_API Vec128<float, 1> ApproximateReciprocalSqrt(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_rsqrt_ss(v.raw)};
}

// ------------------------------ Min (Gt, IfThenElse)

namespace detail {

template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED Vec128<T, N> MinU(const Vec128<T, N> a,
                                              const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;
  const auto msb = Set(du, static_cast<T>(T(1) << (sizeof(T) * 8 - 1)));
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, b, a);
}

}  // namespace detail

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Min(const Vec128<uint8_t, N> a,
                               const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_min_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Min(const Vec128<uint16_t, N> a,
                                const Vec128<uint16_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MinU(a, b);
#else
  return Vec128<uint16_t, N>{_mm_min_epu16(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Min(const Vec128<uint32_t, N> a,
                                const Vec128<uint32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MinU(a, b);
#else
  return Vec128<uint32_t, N>{_mm_min_epu32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Min(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_min_epu64(a.raw, b.raw)};
#else
  return detail::MinU(a, b);
#endif
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Min(const Vec128<int8_t, N> a,
                              const Vec128<int8_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, a, b);
#else
  return Vec128<int8_t, N>{_mm_min_epi8(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Min(const Vec128<int16_t, N> a,
                               const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_min_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Min(const Vec128<int32_t, N> a,
                               const Vec128<int32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, a, b);
#else
  return Vec128<int32_t, N>{_mm_min_epi32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> Min(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_min_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, a, b);
#endif
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Min(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_min_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Min(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_min_pd(a.raw, b.raw)};
}

// ------------------------------ Max (Gt, IfThenElse)

namespace detail {
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED Vec128<T, N> MaxU(const Vec128<T, N> a,
                                              const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;
  const auto msb = Set(du, static_cast<T>(T(1) << (sizeof(T) * 8 - 1)));
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, a, b);
}

}  // namespace detail

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Max(const Vec128<uint8_t, N> a,
                               const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_max_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Max(const Vec128<uint16_t, N> a,
                                const Vec128<uint16_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MaxU(a, b);
#else
  return Vec128<uint16_t, N>{_mm_max_epu16(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Max(const Vec128<uint32_t, N> a,
                                const Vec128<uint32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return detail::MaxU(a, b);
#else
  return Vec128<uint32_t, N>{_mm_max_epu32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Max(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_max_epu64(a.raw, b.raw)};
#else
  return detail::MaxU(a, b);
#endif
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Max(const Vec128<int8_t, N> a,
                              const Vec128<int8_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, b, a);
#else
  return Vec128<int8_t, N>{_mm_max_epi8(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Max(const Vec128<int16_t, N> a,
                               const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_max_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Max(const Vec128<int32_t, N> a,
                               const Vec128<int32_t, N> b) {
#if HWY_TARGET == HWY_SSSE3
  return IfThenElse(a < b, b, a);
#else
  return Vec128<int32_t, N>{_mm_max_epi32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> Max(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_max_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, b, a);
#endif
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Max(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_max_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Max(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_max_pd(a.raw, b.raw)};
}

// ================================================== MEMORY (2)

// ------------------------------ Non-temporal stores

// On clang6, we see incorrect code generated for _mm_stream_pi, so
// round even partial vectors up to 16 bytes.
template <typename T, size_t N>
HWY_API void Stream(Vec128<T, N> v, Simd<T, N, 0> /* tag */,
                    T* HWY_RESTRICT aligned) {
  _mm_stream_si128(reinterpret_cast<__m128i*>(aligned), v.raw);
}
template <size_t N>
HWY_API void Stream(const Vec128<float, N> v, Simd<float, N, 0> /* tag */,
                    float* HWY_RESTRICT aligned) {
  _mm_stream_ps(aligned, v.raw);
}
template <size_t N>
HWY_API void Stream(const Vec128<double, N> v, Simd<double, N, 0> /* tag */,
                    double* HWY_RESTRICT aligned) {
  _mm_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

// Unfortunately the GCC/Clang intrinsics do not accept int64_t*.
using GatherIndex64 = long long int;  // NOLINT(runtime/int)
static_assert(sizeof(GatherIndex64) == 8, "Must be 64-bit type");

#if HWY_TARGET <= HWY_AVX3
namespace detail {

template <typename T, size_t N>
HWY_INLINE void ScatterOffset(hwy::SizeTag<4> /* tag */, Vec128<T, N> v,
                              Simd<T, N, 0> /* tag */, T* HWY_RESTRICT base,
                              const Vec128<int32_t, N> offset) {
  if (N == 4) {
    _mm_i32scatter_epi32(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_epi32(base, mask, offset.raw, v.raw, 1);
  }
}
template <typename T, size_t N>
HWY_INLINE void ScatterIndex(hwy::SizeTag<4> /* tag */, Vec128<T, N> v,
                             Simd<T, N, 0> /* tag */, T* HWY_RESTRICT base,
                             const Vec128<int32_t, N> index) {
  if (N == 4) {
    _mm_i32scatter_epi32(base, index.raw, v.raw, 4);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_epi32(base, mask, index.raw, v.raw, 4);
  }
}

template <typename T, size_t N>
HWY_INLINE void ScatterOffset(hwy::SizeTag<8> /* tag */, Vec128<T, N> v,
                              Simd<T, N, 0> /* tag */, T* HWY_RESTRICT base,
                              const Vec128<int64_t, N> offset) {
  if (N == 2) {
    _mm_i64scatter_epi64(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_epi64(base, mask, offset.raw, v.raw, 1);
  }
}
template <typename T, size_t N>
HWY_INLINE void ScatterIndex(hwy::SizeTag<8> /* tag */, Vec128<T, N> v,
                             Simd<T, N, 0> /* tag */, T* HWY_RESTRICT base,
                             const Vec128<int64_t, N> index) {
  if (N == 2) {
    _mm_i64scatter_epi64(base, index.raw, v.raw, 8);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_epi64(base, mask, index.raw, v.raw, 8);
  }
}

}  // namespace detail

template <typename T, size_t N, typename Offset>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N, 0> d,
                           T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::ScatterOffset(hwy::SizeTag<sizeof(T)>(), v, d, base, offset);
}
template <typename T, size_t N, typename Index>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::ScatterIndex(hwy::SizeTag<sizeof(T)>(), v, d, base, index);
}

template <size_t N>
HWY_API void ScatterOffset(Vec128<float, N> v, Simd<float, N, 0> /* tag */,
                           float* HWY_RESTRICT base,
                           const Vec128<int32_t, N> offset) {
  if (N == 4) {
    _mm_i32scatter_ps(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_ps(base, mask, offset.raw, v.raw, 1);
  }
}
template <size_t N>
HWY_API void ScatterIndex(Vec128<float, N> v, Simd<float, N, 0> /* tag */,
                          float* HWY_RESTRICT base,
                          const Vec128<int32_t, N> index) {
  if (N == 4) {
    _mm_i32scatter_ps(base, index.raw, v.raw, 4);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i32scatter_ps(base, mask, index.raw, v.raw, 4);
  }
}

template <size_t N>
HWY_API void ScatterOffset(Vec128<double, N> v, Simd<double, N, 0> /* tag */,
                           double* HWY_RESTRICT base,
                           const Vec128<int64_t, N> offset) {
  if (N == 2) {
    _mm_i64scatter_pd(base, offset.raw, v.raw, 1);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_pd(base, mask, offset.raw, v.raw, 1);
  }
}
template <size_t N>
HWY_API void ScatterIndex(Vec128<double, N> v, Simd<double, N, 0> /* tag */,
                          double* HWY_RESTRICT base,
                          const Vec128<int64_t, N> index) {
  if (N == 2) {
    _mm_i64scatter_pd(base, index.raw, v.raw, 8);
  } else {
    const __mmask8 mask = (1u << N) - 1;
    _mm_mask_i64scatter_pd(base, mask, index.raw, v.raw, 8);
  }
}
#else  // HWY_TARGET <= HWY_AVX3

template <typename T, size_t N, typename Offset, HWY_IF_LE128(T, N)>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N, 0> d,
                           T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Offset offset_lanes[N];
  Store(offset, Rebind<Offset, decltype(d)>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, size_t N, typename Index, HWY_IF_LE128(T, N)>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N, 0> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Index index_lanes[N];
  Store(index, Rebind<Index, decltype(d)>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

#endif

// ------------------------------ Gather (Load/Store)

#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(const Simd<T, N, 0> d,
                                  const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) Offset offset_lanes[N];
  Store(offset, Rebind<Offset, decltype(d)>(), offset_lanes);

  alignas(16) T lanes[N];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(const Simd<T, N, 0> d,
                                 const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) Index index_lanes[N];
  Store(index, Rebind<Index, decltype(d)>(), index_lanes);

  alignas(16) T lanes[N];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

#else

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherOffset(hwy::SizeTag<4> /* tag */,
                                     Simd<T, N, 0> /* d */,
                                     const T* HWY_RESTRICT base,
                                     const Vec128<int32_t, N> offset) {
  return Vec128<T, N>{_mm_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), offset.raw, 1)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherIndex(hwy::SizeTag<4> /* tag */,
                                    Simd<T, N, 0> /* d */,
                                    const T* HWY_RESTRICT base,
                                    const Vec128<int32_t, N> index) {
  return Vec128<T, N>{_mm_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), index.raw, 4)};
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherOffset(hwy::SizeTag<8> /* tag */,
                                     Simd<T, N, 0> /* d */,
                                     const T* HWY_RESTRICT base,
                                     const Vec128<int64_t, N> offset) {
  return Vec128<T, N>{_mm_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), offset.raw, 1)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> GatherIndex(hwy::SizeTag<8> /* tag */,
                                    Simd<T, N, 0> /* d */,
                                    const T* HWY_RESTRICT base,
                                    const Vec128<int64_t, N> index) {
  return Vec128<T, N>{_mm_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), index.raw, 8)};
}

}  // namespace detail

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(Simd<T, N, 0> d, const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  return detail::GatherOffset(hwy::SizeTag<sizeof(T)>(), d, base, offset);
}
template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(Simd<T, N, 0> d, const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  return detail::GatherIndex(hwy::SizeTag<sizeof(T)>(), d, base, index);
}

template <size_t N>
HWY_API Vec128<float, N> GatherOffset(Simd<float, N, 0> /* tag */,
                                      const float* HWY_RESTRICT base,
                                      const Vec128<int32_t, N> offset) {
  return Vec128<float, N>{_mm_i32gather_ps(base, offset.raw, 1)};
}
template <size_t N>
HWY_API Vec128<float, N> GatherIndex(Simd<float, N, 0> /* tag */,
                                     const float* HWY_RESTRICT base,
                                     const Vec128<int32_t, N> index) {
  return Vec128<float, N>{_mm_i32gather_ps(base, index.raw, 4)};
}

template <size_t N>
HWY_API Vec128<double, N> GatherOffset(Simd<double, N, 0> /* tag */,
                                       const double* HWY_RESTRICT base,
                                       const Vec128<int64_t, N> offset) {
  return Vec128<double, N>{_mm_i64gather_pd(base, offset.raw, 1)};
}
template <size_t N>
HWY_API Vec128<double, N> GatherIndex(Simd<double, N, 0> /* tag */,
                                      const double* HWY_RESTRICT base,
                                      const Vec128<int64_t, N> index) {
  return Vec128<double, N>{_mm_i64gather_pd(base, index.raw, 8)};
}

#endif  // HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE (2)

// ------------------------------ LowerHalf

// Returns upper/lower half of a vector.
template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Simd<T, N / 2, 0> /* tag */,
                                   Vec128<T, N> v) {
  return Vec128<T, N / 2>{v.raw};
}

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  return LowerHalf(Simd<T, N / 2, 0>(), v);
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec128<T, N>{_mm_slli_si128(v.raw, kBytes)};
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(const Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(const Vec128<T, N> v) {
  return ShiftLeftLanes<kLanes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(Simd<T, N, 0> /* tag */, Vec128<T, N> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (N != 16 / sizeof(T)) {
    const Vec128<T> vfull{v.raw};
    v = Vec128<T, N>{IfThenElseZero(FirstN(Full128<T>(), N), vfull).raw};
  }
  return Vec128<T, N>{_mm_srli_si128(v.raw, kBytes)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

// Full input: copy hi into lo (smaller instruction encoding than shifts).
template <typename T>
HWY_API Vec64<T> UpperHalf(Half<Full128<T>> /* tag */, Vec128<T> v) {
  return Vec64<T>{_mm_unpackhi_epi64(v.raw, v.raw)};
}
HWY_API Vec128<float, 2> UpperHalf(Full64<float> /* tag */, Vec128<float> v) {
  return Vec128<float, 2>{_mm_movehl_ps(v.raw, v.raw)};
}
HWY_API Vec64<double> UpperHalf(Full64<double> /* tag */, Vec128<double> v) {
  return Vec64<double>{_mm_unpackhi_pd(v.raw, v.raw)};
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, (N + 1) / 2> UpperHalf(Half<Simd<T, N, 0>> /* tag */,
                                         Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto vu = BitCast(du, v);
  const auto upper = BitCast(d, ShiftRightBytes<N * sizeof(T) / 2>(du, vu));
  return Vec128<T, (N + 1) / 2>{upper.raw};
}

// ------------------------------ ExtractLane (UpperHalf)

namespace detail {

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3
  const int pair = _mm_extract_epi16(v.raw, kLane / 2);
  constexpr int kShift = kLane & 1 ? 8 : 0;
  return static_cast<T>((pair >> kShift) & 0xFF);
#else
  return static_cast<T>(_mm_extract_epi8(v.raw, kLane) & 0xFF);
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
  return static_cast<T>(_mm_extract_epi16(v.raw, kLane) & 0xFFFF);
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[kLane];
#else
  return static_cast<T>(_mm_extract_epi32(v.raw, kLane));
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3 || HWY_ARCH_X86_32
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[kLane];
#else
  return static_cast<T>(_mm_extract_epi64(v.raw, kLane));
#endif
}

template <size_t kLane, size_t N>
HWY_INLINE float ExtractLane(const Vec128<float, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3
  alignas(16) float lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[kLane];
#else
  // Bug in the intrinsic, returns int but should be float.
  const int bits = _mm_extract_ps(v.raw, kLane);
  float ret;
  CopyBytes<4>(&bits, &ret);
  return ret;
#endif
}

// There is no extract_pd; two overloads because there is no UpperHalf for N=1.
template <size_t kLane>
HWY_INLINE double ExtractLane(const Vec128<double, 1> v) {
  static_assert(kLane == 0, "Lane index out of bounds");
  return GetLane(v);
}

template <size_t kLane>
HWY_INLINE double ExtractLane(const Vec128<double> v) {
  static_assert(kLane < 2, "Lane index out of bounds");
  const Half<DFromV<decltype(v)>> dh;
  return kLane == 0 ? GetLane(v) : GetLane(UpperHalf(dh, v));
}

}  // namespace detail

// Requires one overload per vector length because ExtractLane<3> may be a
// compile error if it calls _mm_extract_epi64.
template <typename T>
HWY_API T ExtractLane(const Vec128<T, 1> v, size_t i) {
  HWY_DASSERT(i == 0);
  (void)i;
  return GetLane(v);
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 2> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
    }
  }
#endif
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 4> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
    }
  }
#endif
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 8> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
    }
  }
#endif
  alignas(16) T lanes[8];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 16> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
      case 8:
        return detail::ExtractLane<8>(v);
      case 9:
        return detail::ExtractLane<9>(v);
      case 10:
        return detail::ExtractLane<10>(v);
      case 11:
        return detail::ExtractLane<11>(v);
      case 12:
        return detail::ExtractLane<12>(v);
      case 13:
        return detail::ExtractLane<13>(v);
      case 14:
        return detail::ExtractLane<14>(v);
      case 15:
        return detail::ExtractLane<15>(v);
    }
  }
#endif
  alignas(16) T lanes[16];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (UpperHalf)

namespace detail {

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[16];
  Store(v, d, lanes);
  lanes[kLane] = t;
  return Load(d, lanes);
#else
  return Vec128<T, N>{_mm_insert_epi8(v.raw, t, kLane)};
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{_mm_insert_epi16(v.raw, t, kLane)};
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3
  alignas(16) T lanes[4];
  const DFromV<decltype(v)> d;
  Store(v, d, lanes);
  lanes[kLane] = t;
  return Load(d, lanes);
#else
  MakeSigned<T> ti;
  CopyBytes<sizeof(T)>(&t, &ti);  // don't just cast because T might be float.
  return Vec128<T, N>{_mm_insert_epi32(v.raw, ti, kLane)};
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3 || HWY_ARCH_X86_32
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[2];
  Store(v, d, lanes);
  lanes[kLane] = t;
  return Load(d, lanes);
#else
  MakeSigned<T> ti;
  CopyBytes<sizeof(T)>(&t, &ti);  // don't just cast because T might be float.
  return Vec128<T, N>{_mm_insert_epi64(v.raw, ti, kLane)};
#endif
}

template <size_t kLane, size_t N>
HWY_INLINE Vec128<float, N> InsertLane(const Vec128<float, N> v, float t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET == HWY_SSSE3
  const DFromV<decltype(v)> d;
  alignas(16) float lanes[4];
  Store(v, d, lanes);
  lanes[kLane] = t;
  return Load(d, lanes);
#else
  return Vec128<float, N>{_mm_insert_ps(v.raw, _mm_set_ss(t), kLane << 4)};
#endif
}

// There is no insert_pd; two overloads because there is no UpperHalf for N=1.
template <size_t kLane>
HWY_INLINE Vec128<double, 1> InsertLane(const Vec128<double, 1> v, double t) {
  static_assert(kLane == 0, "Lane index out of bounds");
  return Set(DFromV<decltype(v)>(), t);
}

template <size_t kLane>
HWY_INLINE Vec128<double> InsertLane(const Vec128<double> v, double t) {
  static_assert(kLane < 2, "Lane index out of bounds");
  const DFromV<decltype(v)> d;
  const Vec128<double> vt = Set(d, t);
  if (kLane == 0) {
    return Vec128<double>{_mm_shuffle_pd(vt.raw, v.raw, 2)};
  }
  return Vec128<double>{_mm_shuffle_pd(v.raw, vt.raw, 0)};
}

}  // namespace detail

// Requires one overload per vector length because InsertLane<3> may be a
// compile error if it calls _mm_insert_epi64.

template <typename T>
HWY_API Vec128<T, 1> InsertLane(const Vec128<T, 1> v, size_t i, T t) {
  HWY_DASSERT(i == 0);
  (void)i;
  return Set(DFromV<decltype(v)>(), t);
}

template <typename T>
HWY_API Vec128<T, 2> InsertLane(const Vec128<T, 2> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[2];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 4> InsertLane(const Vec128<T, 4> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[4];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 8> InsertLane(const Vec128<T, 8> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[8];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

template <typename T>
HWY_API Vec128<T, 16> InsertLane(const Vec128<T, 16> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
      case 8:
        return detail::InsertLane<8>(v, t);
      case 9:
        return detail::InsertLane<9>(v, t);
      case 10:
        return detail::InsertLane<10>(v, t);
      case 11:
        return detail::InsertLane<11>(v, t);
      case 12:
        return detail::InsertLane<12>(v, t);
      case 13:
        return detail::InsertLane<13>(v, t);
      case 14:
        return detail::InsertLane<14>(v, t);
      case 15:
        return detail::InsertLane<15>(v, t);
    }
  }
#endif
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[16];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, typename T, class V = Vec128<T>>
HWY_API V CombineShiftRightBytes(Full128<T> d, V hi, V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec128<uint8_t>{_mm_alignr_epi8(
                        BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)});
}

template <int kBytes, typename T, size_t N, HWY_IF_LE64(T, N),
          class V = Vec128<T, N>>
HWY_API V CombineShiftRightBytes(Simd<T, N, 0> d, V hi, V lo) {
  constexpr size_t kSize = N * sizeof(T);
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");
  const Repartition<uint8_t, decltype(d)> d8;
  const Full128<uint8_t> d_full8;
  using V8 = VFromD<decltype(d_full8)>;
  const V8 hi8{BitCast(d8, hi).raw};
  // Move into most-significant bytes
  const V8 lo8 = ShiftLeftBytes<16 - kSize>(V8{BitCast(d8, lo).raw});
  const V8 r = CombineShiftRightBytes<16 - kSize + kBytes>(d_full8, hi8, lo8);
  return V{BitCast(Full128<T>(), r).raw};
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane, size_t N>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  if (kLane < 4) {
    const __m128i lo = _mm_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec128<uint16_t, N>{_mm_unpacklo_epi64(lo, lo)};
  } else {
    const __m128i hi = _mm_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec128<uint16_t, N>{_mm_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane, size_t N>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>{_mm_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<uint64_t, N> Broadcast(const Vec128<uint64_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint64_t, N>{_mm_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Signed
template <int kLane, size_t N>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  if (kLane < 4) {
    const __m128i lo = _mm_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec128<int16_t, N>{_mm_unpacklo_epi64(lo, lo)};
  } else {
    const __m128i hi = _mm_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec128<int16_t, N>{_mm_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane, size_t N>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>{_mm_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<int64_t, N> Broadcast(const Vec128<int64_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int64_t, N>{_mm_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Float
template <int kLane, size_t N>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>{_mm_shuffle_ps(v.raw, v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<double, N> Broadcast(const Vec128<double, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<double, N>{_mm_shuffle_pd(v.raw, v.raw, 3 * kLane)};
}

// ------------------------------ TableLookupLanes (Shuffle01)

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T, size_t N = 16 / sizeof(T)>
struct Indices128 {
  __m128i raw;
};

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N),
          HWY_IF_LANE_SIZE(T, 4)>
HWY_API Indices128<T, N> IndicesFromVec(Simd<T, N, 0> d, Vec128<TI, N> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, N))));
#endif

#if HWY_TARGET <= HWY_AVX2
  (void)d;
  return Indices128<T, N>{vec.raw};
#else
  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;
  alignas(16) constexpr uint8_t kByteOffsets[16] = {0, 1, 2, 3, 0, 1, 2, 3,
                                                    0, 1, 2, 3, 0, 1, 2, 3};

  // Broadcast each lane index to all 4 bytes of T
  alignas(16) constexpr uint8_t kBroadcastLaneBytes[16] = {
      0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12};
  const V8 lane_indices = TableLookupBytes(vec, Load(d8, kBroadcastLaneBytes));

  // Shift to bytes
  const Repartition<uint16_t, decltype(d)> d16;
  const V8 byte_indices = BitCast(d8, ShiftLeft<2>(BitCast(d16, lane_indices)));

  return Indices128<T, N>{Add(byte_indices, Load(d8, kByteOffsets)).raw};
#endif
}

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N),
          HWY_IF_LANE_SIZE(T, 8)>
HWY_API Indices128<T, N> IndicesFromVec(Simd<T, N, 0> d, Vec128<TI, N> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(N)))));
#else
  (void)d;
#endif

  // No change - even without AVX3, we can shuffle+blend.
  return Indices128<T, N>{vec.raw};
}

template <typename T, size_t N, typename TI, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> SetTableIndices(Simd<T, N, 0> d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
#if HWY_TARGET <= HWY_AVX2
  const DFromV<decltype(v)> d;
  const RebindToFloat<decltype(d)> df;
  const Vec128<float, N> perm{_mm_permutevar_ps(BitCast(df, v).raw, idx.raw)};
  return BitCast(d, perm);
#else
  return TableLookupBytes(v, Vec128<T, N>{idx.raw});
#endif
}

template <size_t N, HWY_IF_GE64(float, N)>
HWY_API Vec128<float, N> TableLookupLanes(Vec128<float, N> v,
                                          Indices128<float, N> idx) {
#if HWY_TARGET <= HWY_AVX2
  return Vec128<float, N>{_mm_permutevar_ps(v.raw, idx.raw)};
#else
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;
  return BitCast(df,
                 TableLookupBytes(BitCast(di, v), Vec128<int32_t, N>{idx.raw}));
#endif
}

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> TableLookupLanes(Vec128<T, 1> v,
                                      Indices128<T, 1> /* idx */) {
  return v;
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> TableLookupLanes(Vec128<T> v, Indices128<T> idx) {
  const Full128<T> d;
  Vec128<int64_t> vidx{idx.raw};
#if HWY_TARGET <= HWY_AVX2
  // There is no _mm_permute[x]var_epi64.
  vidx += vidx;  // bit1 is the decider (unusual)
  const Full128<double> df;
  return BitCast(
      d, Vec128<double>{_mm_permutevar_pd(BitCast(df, v).raw, vidx.raw)});
#else
  // Only 2 lanes: can swap+blend. Choose v if vidx == iota. To avoid a 64-bit
  // comparison (expensive on SSSE3), just invert the upper lane and subtract 1
  // to obtain an all-zero or all-one mask.
  const Full128<int64_t> di;
  const Vec128<int64_t> same = (vidx ^ Iota(di, 0)) - Set(di, 1);
  const Mask128<T> mask_same = RebindMask(d, MaskFromVec(same));
  return IfThenElse(mask_same, v, Shuffle01(v));
#endif
}

HWY_API Vec128<double> TableLookupLanes(Vec128<double> v,
                                        Indices128<double> idx) {
  Vec128<int64_t> vidx{idx.raw};
#if HWY_TARGET <= HWY_AVX2
  vidx += vidx;  // bit1 is the decider (unusual)
  return Vec128<double>{_mm_permutevar_pd(v.raw, vidx.raw)};
#else
  // Only 2 lanes: can swap+blend. Choose v if vidx == iota. To avoid a 64-bit
  // comparison (expensive on SSSE3), just invert the upper lane and subtract 1
  // to obtain an all-zero or all-one mask.
  const Full128<double> d;
  const Full128<int64_t> di;
  const Vec128<int64_t> same = (vidx ^ Iota(di, 0)) - Set(di, 1);
  const Mask128<double> mask_same = RebindMask(d, MaskFromVec(same));
  return IfThenElse(mask_same, v, Shuffle01(v));
#endif
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <typename T>
HWY_API Vec128<T> ReverseBlocks(Full128<T> /* tag */, const Vec128<T> v) {
  return v;
}

// ------------------------------ Reverse (Shuffle0123, Shuffle2301)

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> Reverse(Simd<T, 1, 0> /* tag */, const Vec128<T, 1> v) {
  return v;
}

// Two lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, 2> Reverse(Full64<T> /* tag */, const Vec128<T, 2> v) {
  return Vec128<T, 2>{Shuffle2301(Vec128<T>{v.raw}).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> Reverse(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle01(v);
}

// Four lanes: shuffle
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> Reverse(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle0123(v);
}

// 16-bit
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse(Simd<T, N, 0> d, const Vec128<T, N> v) {
#if HWY_TARGET <= HWY_AVX3
  if (N == 1) return v;
  if (N == 2) {
    const Repartition<uint32_t, decltype(d)> du32;
    return BitCast(d, RotateRight<16>(BitCast(du32, v)));
  }
  const RebindToSigned<decltype(d)> di;
  alignas(16) constexpr int16_t kReverse[8] = {7, 6, 5, 4, 3, 2, 1, 0};
  const Vec128<int16_t, N> idx = Load(di, kReverse + (N == 8 ? 0 : 4));
  return BitCast(d, Vec128<int16_t, N>{
                        _mm_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  return BitCast(d, RotateRight<16>(Reverse(du32, BitCast(du32, v))));
#endif
}

// ------------------------------ Reverse2

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const Repartition<uint32_t, decltype(d)> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle2301(v);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Reverse2(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> d, const Vec128<T, N> v) {
  const RebindToSigned<decltype(d)> di;
  // 4x 16-bit: a single shufflelo suffices.
  if (N == 4) {
    return BitCast(d, Vec128<int16_t, N>{_mm_shufflelo_epi16(
                          BitCast(di, v).raw, _MM_SHUFFLE(0, 1, 2, 3))});
  }

#if HWY_TARGET <= HWY_AVX3
  alignas(16) constexpr int16_t kReverse4[8] = {3, 2, 1, 0, 7, 6, 5, 4};
  const Vec128<int16_t, N> idx = Load(di, kReverse4);
  return BitCast(d, Vec128<int16_t, N>{
                        _mm_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<decltype(di)> dw;
  return Reverse2(d, BitCast(d, Shuffle2301(BitCast(dw, v))));
#endif
}

// 4x 32-bit: use Shuffle0123
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> Reverse4(Full128<T> /* tag */, const Vec128<T> v) {
  return Shuffle0123(v);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Reverse4(Simd<T, N, 0> /* tag */, Vec128<T, N> /* v */) {
  HWY_ASSERT(0);  // don't have 4 u64 lanes
}

// ------------------------------ Reverse8

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0> d, const Vec128<T, N> v) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToSigned<decltype(d)> di;
  alignas(32) constexpr int16_t kReverse8[16] = {7,  6,  5,  4,  3,  2,  1, 0,
                                                 15, 14, 13, 12, 11, 10, 9, 8};
  const Vec128<int16_t, N> idx = Load(di, kReverse8);
  return BitCast(d, Vec128<int16_t, N>{
                        _mm_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<decltype(d)> dw;
  return Reverse2(d, BitCast(d, Shuffle0123(BitCast(dw, v))));
#endif
}

template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Reverse8(Simd<T, N, 0> /* tag */, Vec128<T, N> /* v */) {
  HWY_ASSERT(0);  // don't have 8 lanes unless 16-bit
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

template <size_t N, HWY_IF_LE128(uint8_t, N)>
HWY_API Vec128<uint8_t, N> InterleaveLower(const Vec128<uint8_t, N> a,
                                           const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(uint16_t, N)>
HWY_API Vec128<uint16_t, N> InterleaveLower(const Vec128<uint16_t, N> a,
                                            const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(uint32_t, N)>
HWY_API Vec128<uint32_t, N> InterleaveLower(const Vec128<uint32_t, N> a,
                                            const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_unpacklo_epi32(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> InterleaveLower(const Vec128<uint64_t, N> a,
                                            const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_unpacklo_epi64(a.raw, b.raw)};
}

template <size_t N, HWY_IF_LE128(int8_t, N)>
HWY_API Vec128<int8_t, N> InterleaveLower(const Vec128<int8_t, N> a,
                                          const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(int16_t, N)>
HWY_API Vec128<int16_t, N> InterleaveLower(const Vec128<int16_t, N> a,
                                           const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(int32_t, N)>
HWY_API Vec128<int32_t, N> InterleaveLower(const Vec128<int32_t, N> a,
                                           const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_unpacklo_epi32(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(int64_t, N)>
HWY_API Vec128<int64_t, N> InterleaveLower(const Vec128<int64_t, N> a,
                                           const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_unpacklo_epi64(a.raw, b.raw)};
}

template <size_t N, HWY_IF_LE128(float, N)>
HWY_API Vec128<float, N> InterleaveLower(const Vec128<float, N> a,
                                         const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_unpacklo_ps(a.raw, b.raw)};
}
template <size_t N, HWY_IF_LE128(double, N)>
HWY_API Vec128<double, N> InterleaveLower(const Vec128<double, N> a,
                                          const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_unpacklo_pd(a.raw, b.raw)};
}

// Additional overload for the optional tag (also for 256/512).
template <class V>
HWY_API V InterleaveLower(DFromV<V> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec128<uint8_t> InterleaveUpper(const Vec128<uint8_t> a,
                                        const Vec128<uint8_t> b) {
  return Vec128<uint8_t>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec128<uint16_t> InterleaveUpper(const Vec128<uint16_t> a,
                                         const Vec128<uint16_t> b) {
  return Vec128<uint16_t>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec128<uint32_t> InterleaveUpper(const Vec128<uint32_t> a,
                                         const Vec128<uint32_t> b) {
  return Vec128<uint32_t>{_mm_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec128<uint64_t> InterleaveUpper(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>{_mm_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec128<int8_t> InterleaveUpper(const Vec128<int8_t> a,
                                       const Vec128<int8_t> b) {
  return Vec128<int8_t>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec128<int16_t> InterleaveUpper(const Vec128<int16_t> a,
                                        const Vec128<int16_t> b) {
  return Vec128<int16_t>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec128<int32_t> InterleaveUpper(const Vec128<int32_t> a,
                                        const Vec128<int32_t> b) {
  return Vec128<int32_t>{_mm_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec128<int64_t> InterleaveUpper(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return Vec128<int64_t>{_mm_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec128<float> InterleaveUpper(const Vec128<float> a,
                                      const Vec128<float> b) {
  return Vec128<float>{_mm_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec128<double> InterleaveUpper(const Vec128<double> a,
                                       const Vec128<double> b) {
  return Vec128<double>{_mm_unpackhi_pd(a.raw, b.raw)};
}

}  // namespace detail

// Full
template <typename T, class V = Vec128<T>>
HWY_API V InterleaveUpper(Full128<T> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N), class V = Vec128<T, N>>
HWY_API V InterleaveUpper(Simd<T, N, 0> d, V a, V b) {
  const Half<decltype(d)> d2;
  return InterleaveLower(d, V{UpperHalf(d2, a).raw}, V{UpperHalf(d2, b).raw});
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(V a, V b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// N = N/2 + N/2 (upper half undefined)
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> Combine(Simd<T, N, 0> d, Vec128<T, N / 2> hi_half,
                             Vec128<T, N / 2> lo_half) {
  const Half<decltype(d)> d2;
  const RebindToUnsigned<decltype(d2)> du2;
  // Treat half-width input as one lane, and expand to two lanes.
  using VU = Vec128<UnsignedFromSize<N * sizeof(T) / 2>, 2>;
  const VU lo{BitCast(du2, lo_half).raw};
  const VU hi{BitCast(du2, hi_half).raw};
  return BitCast(d, InterleaveLower(lo, hi));
}

// ------------------------------ ZeroExtendVector (Combine, IfThenElseZero)

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T>
HWY_INLINE Vec128<T> ZeroExtendVector(hwy::NonFloatTag /*tag*/,
                                      Full128<T> /* d */, Vec64<T> lo) {
  return Vec128<T>{_mm_move_epi64(lo.raw)};
}

template <typename T>
HWY_INLINE Vec128<T> ZeroExtendVector(hwy::FloatTag /*tag*/, Full128<T> d,
                                      Vec64<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, ZeroExtendVector(du, BitCast(Half<decltype(du)>(), lo)));
}

}  // namespace detail

template <typename T>
HWY_API Vec128<T> ZeroExtendVector(Full128<T> d, Vec64<T> lo) {
  return detail::ZeroExtendVector(hwy::IsFloatTag<T>(), d, lo);
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ZeroExtendVector(Simd<T, N, 0> d, Vec128<T, N / 2> lo) {
  return IfThenElseZero(FirstN(d, N / 2), Vec128<T, N>{lo.raw});
}

// ------------------------------ Concat full (InterleaveLower)

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec128<T> ConcatLowerLower(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint64_t, decltype(d)> d64;
  return BitCast(d, InterleaveLower(BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <typename T>
HWY_API Vec128<T> ConcatUpperUpper(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint64_t, decltype(d)> d64;
  return BitCast(d, InterleaveUpper(d64, BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves)
template <typename T>
HWY_API Vec128<T> ConcatLowerUpper(Full128<T> d, const Vec128<T> hi,
                                   const Vec128<T> lo) {
  return CombineShiftRightBytes<8>(d, hi, lo);
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <typename T>
HWY_API Vec128<T> ConcatUpperLower(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<double, decltype(d)> dd;
#if HWY_TARGET == HWY_SSSE3
  return BitCast(
      d, Vec128<double>{_mm_shuffle_pd(BitCast(dd, lo).raw, BitCast(dd, hi).raw,
                                       _MM_SHUFFLE2(1, 0))});
#else
  // _mm_blend_epi16 has throughput 1/cycle on SKX, whereas _pd can do 3/cycle.
  return BitCast(d, Vec128<double>{_mm_blend_pd(BitCast(dd, hi).raw,
                                                BitCast(dd, lo).raw, 1)});
#endif
}
HWY_API Vec128<float> ConcatUpperLower(Full128<float> d, Vec128<float> hi,
                                       Vec128<float> lo) {
#if HWY_TARGET == HWY_SSSE3
  (void)d;
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(3, 2, 1, 0))};
#else
  // _mm_shuffle_ps has throughput 1/cycle on SKX, whereas blend can do 3/cycle.
  const RepartitionToWide<decltype(d)> dd;
  return BitCast(d, Vec128<double>{_mm_blend_pd(BitCast(dd, hi).raw,
                                                BitCast(dd, lo).raw, 1)});
#endif
}
HWY_API Vec128<double> ConcatUpperLower(Full128<double> /* tag */,
                                        Vec128<double> hi, Vec128<double> lo) {
#if HWY_TARGET == HWY_SSSE3
  return Vec128<double>{_mm_shuffle_pd(lo.raw, hi.raw, _MM_SHUFFLE2(1, 0))};
#else
  // _mm_shuffle_pd has throughput 1/cycle on SKX, whereas blend can do 3/cycle.
  return Vec128<double>{_mm_blend_pd(hi.raw, lo.raw, 1)};
#endif
}

// ------------------------------ Concat partial (Combine, LowerHalf)

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), LowerHalf(d2, lo));
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), UpperHalf(d2, lo));
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerUpper(Simd<T, N, 0> d, const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), UpperHalf(d2, lo));
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> ConcatUpperLower(Simd<T, N, 0> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), LowerHalf(d2, lo));
}

// ------------------------------ ConcatOdd

// 8-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T> ConcatOdd(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint16_t, decltype(d)> dw;
  // Right-shift 8 bits per u16 so we can pack.
  const Vec128<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec128<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  return Vec128<T>{_mm_packus_epi16(uL.raw, uH.raw)};
}

// 8-bit x8
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec64<T> ConcatOdd(Simd<T, 8, 0> d, Vec64<T> hi, Vec64<T> lo) {
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactOddU8[8] = {1, 3, 5, 7};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactOddU8));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
}

// 8-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec32<T> ConcatOdd(Simd<T, 4, 0> d, Vec32<T> hi, Vec32<T> lo) {
  const Repartition<uint16_t, decltype(d)> du16;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactOddU8[4] = {1, 3};
  const Vec32<T> shuf = BitCast(d, Load(Full32<uint8_t>(), kCompactOddU8));
  const Vec32<T> L = TableLookupBytes(lo, shuf);
  const Vec32<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du16, BitCast(du16, L), BitCast(du16, H)));
}

// 16-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T> ConcatOdd(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  // Right-shift 16 bits per i32 - a *signed* shift of 0x8000xxxx returns
  // 0xFFFF8000, which correctly saturates to 0x8000.
  const Repartition<int32_t, decltype(d)> dw;
  const Vec128<int32_t> uH = ShiftRight<16>(BitCast(dw, hi));
  const Vec128<int32_t> uL = ShiftRight<16>(BitCast(dw, lo));
  return Vec128<T>{_mm_packs_epi32(uL.raw, uH.raw)};
}

// 16-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec64<T> ConcatOdd(Simd<T, 4, 0> d, Vec64<T> hi, Vec64<T> lo) {
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactOddU16[8] = {2, 3, 6, 7};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactOddU16));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
}

// 32-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> ConcatOdd(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec128<float>{_mm_shuffle_ps(BitCast(df, lo).raw, BitCast(df, hi).raw,
                                      _MM_SHUFFLE(3, 1, 3, 1))});
}
template <size_t N>
HWY_API Vec128<float> ConcatOdd(Full128<float> /* tag */, Vec128<float> hi,
                                Vec128<float> lo) {
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(3, 1, 3, 1))};
}

// Any type x2
template <typename T>
HWY_API Vec128<T, 2> ConcatOdd(Simd<T, 2, 0> d, Vec128<T, 2> hi,
                               Vec128<T, 2> lo) {
  return InterleaveUpper(d, lo, hi);
}

// ------------------------------ ConcatEven (InterleaveLower)

// 8-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec128<T> ConcatEven(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint16_t, decltype(d)> dw;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec128<uint16_t> mask = Set(dw, 0x00FF);
  const Vec128<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec128<uint16_t> uL = And(BitCast(dw, lo), mask);
  return Vec128<T>{_mm_packus_epi16(uL.raw, uH.raw)};
}

// 8-bit x8
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec64<T> ConcatEven(Simd<T, 8, 0> d, Vec64<T> hi, Vec64<T> lo) {
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactEvenU8[8] = {0, 2, 4, 6};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactEvenU8));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
}

// 8-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec32<T> ConcatEven(Simd<T, 4, 0> d, Vec32<T> hi, Vec32<T> lo) {
  const Repartition<uint16_t, decltype(d)> du16;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactEvenU8[4] = {0, 2};
  const Vec32<T> shuf = BitCast(d, Load(Full32<uint8_t>(), kCompactEvenU8));
  const Vec32<T> L = TableLookupBytes(lo, shuf);
  const Vec32<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du16, BitCast(du16, L), BitCast(du16, H)));
}

// 16-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T> ConcatEven(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
#if HWY_TARGET <= HWY_SSE4
  // Isolate lower 16 bits per u32 so we can pack.
  const Repartition<uint32_t, decltype(d)> dw;
  const Vec128<uint32_t> mask = Set(dw, 0x0000FFFF);
  const Vec128<uint32_t> uH = And(BitCast(dw, hi), mask);
  const Vec128<uint32_t> uL = And(BitCast(dw, lo), mask);
  return Vec128<T>{_mm_packus_epi32(uL.raw, uH.raw)};
#else
  // packs_epi32 saturates 0x8000 to 0x7FFF. Instead ConcatEven within the two
  // inputs, then concatenate them.
  alignas(16) const T kCompactEvenU16[8] = {0x0100, 0x0504, 0x0908, 0x0D0C};
  const Vec128<T> shuf = BitCast(d, Load(d, kCompactEvenU16));
  const Vec128<T> L = TableLookupBytes(lo, shuf);
  const Vec128<T> H = TableLookupBytes(hi, shuf);
  return ConcatLowerLower(d, H, L);
#endif
}

// 16-bit x4
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec64<T> ConcatEven(Simd<T, 4, 0> d, Vec64<T> hi, Vec64<T> lo) {
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactEvenU16[8] = {0, 1, 4, 5};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactEvenU16));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
}

// 32-bit full
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T> ConcatEven(Full128<T> d, Vec128<T> hi, Vec128<T> lo) {
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec128<float>{_mm_shuffle_ps(BitCast(df, lo).raw, BitCast(df, hi).raw,
                                      _MM_SHUFFLE(2, 0, 2, 0))});
}
HWY_API Vec128<float> ConcatEven(Full128<float> /* tag */, Vec128<float> hi,
                                 Vec128<float> lo) {
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(2, 0, 2, 0))};
}

// Any T x2
template <typename T>
HWY_API Vec128<T, 2> ConcatEven(Simd<T, 2, 0> d, Vec128<T, 2> hi,
                                Vec128<T, 2> lo) {
  return InterleaveLower(d, lo, hi);
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  return Vec128<T, N>{_mm_shuffle_epi32(v.raw, _MM_SHUFFLE(2, 2, 0, 0))};
}
template <size_t N>
HWY_API Vec128<float, N> DupEven(Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_shuffle_ps(v.raw, v.raw, _MM_SHUFFLE(2, 2, 0, 0))};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  return InterleaveLower(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  return Vec128<T, N>{_mm_shuffle_epi32(v.raw, _MM_SHUFFLE(3, 3, 1, 1))};
}
template <size_t N>
HWY_API Vec128<float, N> DupOdd(Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_shuffle_ps(v.raw, v.raw, _MM_SHUFFLE(3, 3, 1, 1))};
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  return InterleaveUpper(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ OddEven (IfThenElse)

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t mask[16] = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                            0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t mask[16] = {0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0,
                                            0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
#else
  return Vec128<T, N>{_mm_blend_epi16(a.raw, b.raw, 0x55)};
#endif
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i odd = _mm_shuffle_epi32(a.raw, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128i even = _mm_shuffle_epi32(b.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<T, N>{_mm_unpacklo_epi32(even, odd)};
#else
  // _mm_blend_epi16 has throughput 1/cycle on SKX, whereas _ps can do 3/cycle.
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(d, Vec128<float, N>{_mm_blend_ps(BitCast(df, a).raw,
                                                  BitCast(df, b).raw, 5)});
#endif
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  // Same as ConcatUpperLower for full vectors; do not call that because this
  // is more efficient for 64x1 vectors.
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> dd;
#if HWY_TARGET == HWY_SSSE3
  return BitCast(
      d, Vec128<double, N>{_mm_shuffle_pd(
             BitCast(dd, b).raw, BitCast(dd, a).raw, _MM_SHUFFLE2(1, 0))});
#else
  // _mm_shuffle_pd has throughput 1/cycle on SKX, whereas blend can do 3/cycle.
  return BitCast(d, Vec128<double, N>{_mm_blend_pd(BitCast(dd, a).raw,
                                                   BitCast(dd, b).raw, 1)});
#endif
}

template <size_t N>
HWY_API Vec128<float, N> OddEven(Vec128<float, N> a, Vec128<float, N> b) {
#if HWY_TARGET == HWY_SSSE3
  // SHUFPS must fill the lower half of the output from one input, so we
  // need another shuffle. Unpack avoids another immediate byte.
  const __m128 odd = _mm_shuffle_ps(a.raw, a.raw, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128 even = _mm_shuffle_ps(b.raw, b.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<float, N>{_mm_unpacklo_ps(even, odd)};
#else
  return Vec128<float, N>{_mm_blend_ps(a.raw, b.raw, 5)};
#endif
}

// ------------------------------ OddEvenBlocks
template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ Shl (ZipLower, Mul)

// Use AVX2/3 variable shifts where available, otherwise multiply by powers of
// two from loading float exponents, which is considerably faster (according
// to LLVM-MCA) than scalar or testing bits: https://gcc.godbolt.org/z/9G7Y9v.

namespace detail {
#if HWY_TARGET > HWY_AVX3  // AVX2 or older

// Returns 2^v for use as per-lane multipliers to emulate 16-bit shifts.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<MakeUnsigned<T>, N> Pow2(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> dw;
  const Rebind<float, decltype(dw)> df;
  const auto zero = Zero(d);
  // Move into exponent (this u16 will become the upper half of an f32)
  const auto exp = ShiftLeft<23 - 16>(v);
  const auto upper = exp + Set(d, 0x3F80);  // upper half of 1.0f
  // Insert 0 into lower halves for reinterpreting as binary32.
  const auto f0 = ZipLower(dw, zero, upper);
  const auto f1 = ZipUpper(dw, zero, upper);
  // See comment below.
  const Vec128<int32_t, N> bits0{_mm_cvtps_epi32(BitCast(df, f0).raw)};
  const Vec128<int32_t, N> bits1{_mm_cvtps_epi32(BitCast(df, f1).raw)};
  return Vec128<MakeUnsigned<T>, N>{_mm_packus_epi32(bits0.raw, bits1.raw)};
}

// Same, for 32-bit shifts.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<MakeUnsigned<T>, N> Pow2(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const auto exp = ShiftLeft<23>(v);
  const auto f = exp + Set(d, 0x3F800000);  // 1.0f
  // Do not use ConvertTo because we rely on the native 0x80..00 overflow
  // behavior. cvt instead of cvtt should be equivalent, but avoids test
  // failure under GCC 10.2.1.
  return Vec128<MakeUnsigned<T>, N>{_mm_cvtps_epi32(_mm_castsi128_ps(f.raw))};
}

#endif  // HWY_TARGET > HWY_AVX3

template <size_t N>
HWY_API Vec128<uint16_t, N> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint16_t, N> v,
                                Vec128<uint16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint16_t, N>{_mm_sllv_epi16(v.raw, bits.raw)};
#else
  return v * Pow2(bits);
#endif
}
HWY_API Vec128<uint16_t, 1> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint16_t, 1> v,
                                Vec128<uint16_t, 1> bits) {
  return Vec128<uint16_t, 1>{_mm_sll_epi16(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint32_t, N> v,
                                Vec128<uint32_t, N> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  return v * Pow2(bits);
#else
  return Vec128<uint32_t, N>{_mm_sllv_epi32(v.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint32_t, 1> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint32_t, 1> v,
                                const Vec128<uint32_t, 1> bits) {
  return Vec128<uint32_t, 1>{_mm_sll_epi32(v.raw, bits.raw)};
}

HWY_API Vec128<uint64_t> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint64_t> v,
                             Vec128<uint64_t> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  // Individual shifts and combine
  const Vec128<uint64_t> out0{_mm_sll_epi64(v.raw, bits.raw)};
  const __m128i bits1 = _mm_unpackhi_epi64(bits.raw, bits.raw);
  const Vec128<uint64_t> out1{_mm_sll_epi64(v.raw, bits1)};
  return ConcatUpperLower(Full128<uint64_t>(), out1, out0);
#else
  return Vec128<uint64_t>{_mm_sllv_epi64(v.raw, bits.raw)};
#endif
}
HWY_API Vec64<uint64_t> Shl(hwy::UnsignedTag /*tag*/, Vec64<uint64_t> v,
                            Vec64<uint64_t> bits) {
  return Vec64<uint64_t>{_mm_sll_epi64(v.raw, bits.raw)};
}

// Signed left shift is the same as unsigned.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shl(hwy::SignedTag /*tag*/, Vec128<T, N> v,
                         Vec128<T, N> bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  return BitCast(di,
                 Shl(hwy::UnsignedTag(), BitCast(du, v), BitCast(du, bits)));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return detail::Shl(hwy::TypeTag<T>(), v, bits);
}

// ------------------------------ Shr (mul, mask, BroadcastSignBit)

// Use AVX2+ variable shifts except for SSSE3/SSE4 or 16-bit. There, we use
// widening multiplication by powers of two obtained by loading float exponents,
// followed by a constant right-shift. This is still faster than a scalar or
// bit-test approach: https://gcc.godbolt.org/z/9G7Y9v.

template <size_t N>
HWY_API Vec128<uint16_t, N> operator>>(const Vec128<uint16_t, N> in,
                                       const Vec128<uint16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint16_t, N>{_mm_srlv_epi16(in.raw, bits.raw)};
#else
  const Simd<uint16_t, N, 0> d;
  // For bits=0, we cannot mul by 2^16, so fix the result later.
  const auto out = MulHigh(in, detail::Pow2(Set(d, 16) - bits));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d), in, out);
#endif
}
HWY_API Vec128<uint16_t, 1> operator>>(const Vec128<uint16_t, 1> in,
                                       const Vec128<uint16_t, 1> bits) {
  return Vec128<uint16_t, 1>{_mm_srl_epi16(in.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> operator>>(const Vec128<uint32_t, N> in,
                                       const Vec128<uint32_t, N> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  // 32x32 -> 64 bit mul, then shift right by 32.
  const Simd<uint32_t, N, 0> d32;
  // Move odd lanes into position for the second mul. Shuffle more gracefully
  // handles N=1 than repartitioning to u64 and shifting 32 bits right.
  const Vec128<uint32_t, N> in31{_mm_shuffle_epi32(in.raw, 0x31)};
  // For bits=0, we cannot mul by 2^32, so fix the result later.
  const auto mul = detail::Pow2(Set(d32, 32) - bits);
  const auto out20 = ShiftRight<32>(MulEven(in, mul));  // z 2 z 0
  const Vec128<uint32_t, N> mul31{_mm_shuffle_epi32(mul.raw, 0x31)};
  // No need to shift right, already in the correct position.
  const auto out31 = BitCast(d32, MulEven(in31, mul31));  // 3 ? 1 ?
  const Vec128<uint32_t, N> out = OddEven(out31, BitCast(d32, out20));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d32), in, out);
#else
  return Vec128<uint32_t, N>{_mm_srlv_epi32(in.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint32_t, 1> operator>>(const Vec128<uint32_t, 1> in,
                                       const Vec128<uint32_t, 1> bits) {
  return Vec128<uint32_t, 1>{_mm_srl_epi32(in.raw, bits.raw)};
}

HWY_API Vec128<uint64_t> operator>>(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
#if HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4
  // Individual shifts and combine
  const Vec128<uint64_t> out0{_mm_srl_epi64(v.raw, bits.raw)};
  const __m128i bits1 = _mm_unpackhi_epi64(bits.raw, bits.raw);
  const Vec128<uint64_t> out1{_mm_srl_epi64(v.raw, bits1)};
  return ConcatUpperLower(Full128<uint64_t>(), out1, out0);
#else
  return Vec128<uint64_t>{_mm_srlv_epi64(v.raw, bits.raw)};
#endif
}
HWY_API Vec64<uint64_t> operator>>(const Vec64<uint64_t> v,
                                   const Vec64<uint64_t> bits) {
  return Vec64<uint64_t>{_mm_srl_epi64(v.raw, bits.raw)};
}

#if HWY_TARGET > HWY_AVX3  // AVX2 or older
namespace detail {

// Also used in x86_256-inl.h.
template <class DI, class V>
HWY_INLINE V SignedShr(const DI di, const V v, const V count_i) {
  const RebindToUnsigned<DI> du;
  const auto count = BitCast(du, count_i);  // same type as value to shift
  // Clear sign and restore afterwards. This is preferable to shifting the MSB
  // downwards because Shr is somewhat more expensive than Shl.
  const auto sign = BroadcastSignBit(v);
  const auto abs = BitCast(du, v ^ sign);  // off by one, but fixed below
  return BitCast(di, abs >> count) ^ sign;
}

}  // namespace detail
#endif  // HWY_TARGET > HWY_AVX3

template <size_t N>
HWY_API Vec128<int16_t, N> operator>>(const Vec128<int16_t, N> v,
                                      const Vec128<int16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int16_t, N>{_mm_srav_epi16(v.raw, bits.raw)};
#else
  return detail::SignedShr(Simd<int16_t, N, 0>(), v, bits);
#endif
}
HWY_API Vec128<int16_t, 1> operator>>(const Vec128<int16_t, 1> v,
                                      const Vec128<int16_t, 1> bits) {
  return Vec128<int16_t, 1>{_mm_sra_epi16(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator>>(const Vec128<int32_t, N> v,
                                      const Vec128<int32_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int32_t, N>{_mm_srav_epi32(v.raw, bits.raw)};
#else
  return detail::SignedShr(Simd<int32_t, N, 0>(), v, bits);
#endif
}
HWY_API Vec128<int32_t, 1> operator>>(const Vec128<int32_t, 1> v,
                                      const Vec128<int32_t, 1> bits) {
  return Vec128<int32_t, 1>{_mm_sra_epi32(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, N> operator>>(const Vec128<int64_t, N> v,
                                      const Vec128<int64_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_srav_epi64(v.raw, bits.raw)};
#else
  return detail::SignedShr(Simd<int64_t, N, 0>(), v, bits);
#endif
}

// ------------------------------ MulEven/Odd 64x64 (UpperHalf)

HWY_INLINE Vec128<uint64_t> MulEven(const Vec128<uint64_t> a,
                                    const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(a), GetLane(b), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

HWY_INLINE Vec128<uint64_t> MulOdd(const Vec128<uint64_t> a,
                                   const Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  const Half<Full128<uint64_t>> d2;
  mul[0] =
      Mul128(GetLane(UpperHalf(d2, a)), GetLane(UpperHalf(d2, b)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <size_t N>
HWY_API Vec128<float, N> ReorderWidenMulAccumulate(Simd<float, N, 0> df32,
                                                   Vec128<bfloat16_t, 2 * N> a,
                                                   Vec128<bfloat16_t, 2 * N> b,
                                                   const Vec128<float, N> sum0,
                                                   Vec128<float, N>& sum1) {
  // TODO(janwas): _mm_dpbf16_ps when available
  const Repartition<uint16_t, decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  const Vec128<uint16_t, 2 * N> zero = Zero(du16);
  // Lane order within sum0/1 is undefined, hence we can avoid the
  // longer-latency lane-crossing PromoteTo.
  const Vec128<uint32_t, N> a0 = ZipLower(du32, zero, BitCast(du16, a));
  const Vec128<uint32_t, N> a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const Vec128<uint32_t, N> b0 = ZipLower(du32, zero, BitCast(du16, b));
  const Vec128<uint32_t, N> b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
template <size_t N>
HWY_API Vec128<uint16_t, N> PromoteTo(Simd<uint16_t, N, 0> /* tag */,
                                      const Vec128<uint8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i zero = _mm_setzero_si128();
  return Vec128<uint16_t, N>{_mm_unpacklo_epi8(v.raw, zero)};
#else
  return Vec128<uint16_t, N>{_mm_cvtepu8_epi16(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return Vec128<uint32_t, N>{_mm_unpacklo_epi16(v.raw, _mm_setzero_si128())};
#else
  return Vec128<uint32_t, N>{_mm_cvtepu16_epi32(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> PromoteTo(Simd<uint64_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return Vec128<uint64_t, N>{_mm_unpacklo_epi32(v.raw, _mm_setzero_si128())};
#else
  return Vec128<uint64_t, N>{_mm_cvtepu32_epi64(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N, 0> /* tag */,
                                      const Vec128<uint8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i zero = _mm_setzero_si128();
  const __m128i u16 = _mm_unpacklo_epi8(v.raw, zero);
  return Vec128<uint32_t, N>{_mm_unpacklo_epi16(u16, zero)};
#else
  return Vec128<uint32_t, N>{_mm_cvtepu8_epi32(v.raw)};
#endif
}

// Unsigned to signed: same plus cast.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N, 0> di,
                                     const Vec128<uint8_t, N> v) {
  return BitCast(di, PromoteTo(Simd<uint16_t, N, 0>(), v));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> di,
                                     const Vec128<uint16_t, N> v) {
  return BitCast(di, PromoteTo(Simd<uint32_t, N, 0>(), v));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> di,
                                     const Vec128<uint8_t, N> v) {
  return BitCast(di, PromoteTo(Simd<uint32_t, N, 0>(), v));
}

// Signed: replicate sign bit.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N, 0> /* tag */,
                                     const Vec128<int8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return ShiftRight<8>(Vec128<int16_t, N>{_mm_unpacklo_epi8(v.raw, v.raw)});
#else
  return Vec128<int16_t, N>{_mm_cvtepi8_epi16(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<int16_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return ShiftRight<16>(Vec128<int32_t, N>{_mm_unpacklo_epi16(v.raw, v.raw)});
#else
  return Vec128<int32_t, N>{_mm_cvtepi16_epi32(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> PromoteTo(Simd<int64_t, N, 0> /* tag */,
                                     const Vec128<int32_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  return ShiftRight<32>(Vec128<int64_t, N>{_mm_unpacklo_epi32(v.raw, v.raw)});
#else
  return Vec128<int64_t, N>{_mm_cvtepi32_epi64(v.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N, 0> /* tag */,
                                     const Vec128<int8_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const __m128i x2 = _mm_unpacklo_epi8(v.raw, v.raw);
  const __m128i x4 = _mm_unpacklo_epi16(x2, x2);
  return ShiftRight<24>(Vec128<int32_t, N>{x4});
#else
  return Vec128<int32_t, N>{_mm_cvtepi8_epi32(v.raw)};
#endif
}

// Workaround for origin tracking bug in Clang msan prior to 11.0
// (spurious "uninitialized memory" for TestF16 with "ORIGIN: invalid")
#if HWY_IS_MSAN && (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 1100)
#define HWY_INLINE_F16 HWY_NOINLINE
#else
#define HWY_INLINE_F16 HWY_INLINE
#endif
template <size_t N>
HWY_INLINE_F16 Vec128<float, N> PromoteTo(Simd<float, N, 0> df32,
                                          const Vec128<float16_t, N> v) {
#if HWY_TARGET >= HWY_SSE4 || defined(HWY_DISABLE_F16C)
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t, N>{v.raw});
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
  return Vec128<float, N>{_mm_cvtph_ps(v.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N, 0> df32,
                                   const Vec128<bfloat16_t, N> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

template <size_t N>
HWY_API Vec128<double, N> PromoteTo(Simd<double, N, 0> /* tag */,
                                    const Vec128<float, N> v) {
  return Vec128<double, N>{_mm_cvtps_pd(v.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> PromoteTo(Simd<double, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<double, N>{_mm_cvtepi32_pd(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <size_t N>
HWY_API Vec128<uint16_t, N> DemoteTo(Simd<uint16_t, N, 0> /* tag */,
                                     const Vec128<int32_t, N> v) {
#if HWY_TARGET == HWY_SSSE3
  const Simd<int32_t, N, 0> di32;
  const Simd<uint16_t, N * 2, 0> du16;
  const auto zero_if_neg = AndNot(ShiftRight<31>(v), v);
  const auto too_big = VecFromMask(di32, Gt(v, Set(di32, 0xFFFF)));
  const auto clamped = Or(zero_if_neg, too_big);
  // Lower 2 bytes from each 32-bit lane; same as return type for fewer casts.
  alignas(16) constexpr uint16_t kLower2Bytes[16] = {
      0x0100, 0x0504, 0x0908, 0x0D0C, 0x8080, 0x8080, 0x8080, 0x8080};
  const auto lo2 = Load(du16, kLower2Bytes);
  return Vec128<uint16_t, N>{TableLookupBytes(BitCast(du16, clamped), lo2).raw};
#else
  return Vec128<uint16_t, N>{_mm_packus_epi32(v.raw, v.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int16_t, N> DemoteTo(Simd<int16_t, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<int16_t, N>{_mm_packs_epi32(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int32_t, N> v) {
  const __m128i i16 = _mm_packs_epi32(v.raw, v.raw);
  return Vec128<uint8_t, N>{_mm_packus_epi16(i16, i16)};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N, 0> /* tag */,
                                    const Vec128<int16_t, N> v) {
  return Vec128<uint8_t, N>{_mm_packus_epi16(v.raw, v.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int32_t, N> v) {
  const __m128i i16 = _mm_packs_epi32(v.raw, v.raw);
  return Vec128<int8_t, N>{_mm_packs_epi16(i16, i16)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N, 0> /* tag */,
                                   const Vec128<int16_t, N> v) {
  return Vec128<int8_t, N>{_mm_packs_epi16(v.raw, v.raw)};
}

// Work around MSVC warning for _mm_cvtps_ph (8 is actually a valid immediate).
// clang-cl requires a non-empty string, so we 'ignore' the irrelevant -Wmain.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4556, ignored "-Wmain")

template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N, 0> df16,
                                      const Vec128<float, N> v) {
#if HWY_TARGET >= HWY_SSE4 || defined(HWY_DISABLE_F16C)
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
  return Vec128<float16_t, N>{_mm_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
#endif
}

HWY_DIAGNOSTICS(pop)

template <size_t N>
HWY_API Vec128<bfloat16_t, N> DemoteTo(Simd<bfloat16_t, N, 0> dbf16,
                                       const Vec128<float, N> v) {
  // TODO(janwas): _mm_cvtneps_pbh once we have avx512bf16.
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

template <size_t N>
HWY_API Vec128<bfloat16_t, 2 * N> ReorderDemote2To(
    Simd<bfloat16_t, 2 * N, 0> dbf16, Vec128<float, N> a, Vec128<float, N> b) {
  // TODO(janwas): _mm_cvtne2ps_pbh once we have avx512bf16.
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec128<uint32_t, N> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

template <size_t N>
HWY_API Vec128<float, N> DemoteTo(Simd<float, N, 0> /* tag */,
                                  const Vec128<double, N> v) {
  return Vec128<float, N>{_mm_cvtpd_ps(v.raw)};
}

namespace detail {

// For well-defined float->int demotion in all x86_*-inl.h.

template <size_t N>
HWY_INLINE auto ClampF64ToI32Max(Simd<double, N, 0> d, decltype(Zero(d)) v)
    -> decltype(Zero(d)) {
  // The max can be exactly represented in binary64, so clamping beforehand
  // prevents x86 conversion from raising an exception and returning 80..00.
  return Min(v, Set(d, 2147483647.0));
}

// For ConvertTo float->int of same size, clamping before conversion would
// change the result because the max integer value is not exactly representable.
// Instead detect the overflow result after conversion and fix it.
template <class DI, class DF = RebindToFloat<DI>>
HWY_INLINE auto FixConversionOverflow(DI di, VFromD<DF> original,
                                      decltype(Zero(di).raw) converted_raw)
    -> VFromD<DI> {
  // Combinations of original and output sign:
  //   --: normal <0 or -huge_val to 80..00: OK
  //   -+: -0 to 0                         : OK
  //   +-: +huge_val to 80..00             : xor with FF..FF to get 7F..FF
  //   ++: normal >0                       : OK
  const auto converted = VFromD<DI>{converted_raw};
  const auto sign_wrong = AndNot(BitCast(di, original), converted);
#if HWY_COMPILER_GCC_ACTUAL
  // Critical GCC 11 compiler bug (possibly also GCC 10): omits the Xor; also
  // Add() if using that instead. Work around with one more instruction.
  const RebindToUnsigned<DI> du;
  const VFromD<DI> mask = BroadcastSignBit(sign_wrong);
  const VFromD<DI> max = BitCast(di, ShiftRight<1>(BitCast(du, mask)));
  return IfVecThenElse(mask, max, converted);
#else
  return Xor(converted, BroadcastSignBit(sign_wrong));
#endif
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<int32_t, N> DemoteTo(Simd<int32_t, N, 0> /* tag */,
                                    const Vec128<double, N> v) {
  const auto clamped = detail::ClampF64ToI32Max(Simd<double, N, 0>(), v);
  return Vec128<int32_t, N>{_mm_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  const Simd<uint32_t, N, 0> d32;
  const Simd<uint8_t, N * 4, 0> d8;
  alignas(16) static constexpr uint32_t k8From32[4] = {
      0x0C080400u, 0x0C080400u, 0x0C080400u, 0x0C080400u};
  // Also replicate bytes into all 32 bit lanes for safety.
  const auto quad = TableLookupBytes(v, Load(d32, k8From32));
  return LowerHalf(LowerHalf(BitCast(d8, quad)));
}

// ------------------------------ Truncations

template <typename From, typename To,
          hwy::EnableIf<(sizeof(To) < sizeof(From))>* = nullptr>
HWY_API Vec128<To, 1> TruncateTo(Simd<To, 1, 0> /* tag */,
                                 const Vec128<From, 1> v) {
  static_assert(!IsSigned<To>() && !IsSigned<From>(), "Unsigned only");
  const Repartition<To, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  return Vec128<To, 1>{v1.raw};
}

HWY_API Vec128<uint8_t, 2> TruncateTo(Simd<uint8_t, 2, 0> /* tag */,
                                      const Vec128<uint64_t, 2> v) {
  const Full128<uint8_t> d8;
  alignas(16) static constexpr uint8_t kMap[16] = {0, 8, 0, 8, 0, 8, 0, 8,
                                                   0, 8, 0, 8, 0, 8, 0, 8};
  return LowerHalf(LowerHalf(LowerHalf(TableLookupBytes(v, Load(d8, kMap)))));
}

HWY_API Vec128<uint16_t, 2> TruncateTo(Simd<uint16_t, 2, 0> /* tag */,
                                       const Vec128<uint64_t, 2> v) {
  const Full128<uint16_t> d16;
  alignas(16) static constexpr uint16_t kMap[8] = {
      0x100u, 0x908u, 0x100u, 0x908u, 0x100u, 0x908u, 0x100u, 0x908u};
  return LowerHalf(LowerHalf(TableLookupBytes(v, Load(d16, kMap))));
}

HWY_API Vec128<uint32_t, 2> TruncateTo(Simd<uint32_t, 2, 0> /* tag */,
                                       const Vec128<uint64_t, 2> v) {
  return Vec128<uint32_t, 2>{_mm_shuffle_epi32(v.raw, 0x88)};
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  const Repartition<uint8_t, DFromV<decltype(v)>> d;
  alignas(16) static constexpr uint8_t kMap[16] = {
      0x0u, 0x4u, 0x8u, 0xCu, 0x0u, 0x4u, 0x8u, 0xCu,
      0x0u, 0x4u, 0x8u, 0xCu, 0x0u, 0x4u, 0x8u, 0xCu};
  return LowerHalf(LowerHalf(TableLookupBytes(v, Load(d, kMap))));
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint16_t, N> TruncateTo(Simd<uint16_t, N, 0> /* tag */,
                                       const Vec128<uint32_t, N> v) {
  const Repartition<uint16_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  return LowerHalf(ConcatEven(d, v1, v1));
}

template <size_t N, hwy::EnableIf<N >= 2>* = nullptr>
HWY_API Vec128<uint8_t, N> TruncateTo(Simd<uint8_t, N, 0> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  const Repartition<uint8_t, DFromV<decltype(v)>> d;
  const auto v1 = BitCast(d, v);
  return LowerHalf(ConcatEven(d, v1, v1));
}

// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

template <size_t N>
HWY_API Vec128<float, N> ConvertTo(Simd<float, N, 0> /* tag */,
                                   const Vec128<int32_t, N> v) {
  return Vec128<float, N>{_mm_cvtepi32_ps(v.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> ConvertTo(Simd<double, N, 0> dd,
                                    const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  (void)dd;
  return Vec128<double, N>{_mm_cvtepi64_pd(v.raw)};
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
template <size_t N>
HWY_API Vec128<int32_t, N> ConvertTo(const Simd<int32_t, N, 0> di,
                                     const Vec128<float, N> v) {
  return detail::FixConversionOverflow(di, v, _mm_cvttps_epi32(v.raw));
}

// Full (partial handled below)
HWY_API Vec128<int64_t> ConvertTo(Full128<int64_t> di, const Vec128<double> v) {
#if HWY_TARGET <= HWY_AVX3 && HWY_ARCH_X86_64
  return detail::FixConversionOverflow(di, v, _mm_cvttpd_epi64(v.raw));
#elif HWY_ARCH_X86_64
  const __m128i i0 = _mm_cvtsi64_si128(_mm_cvttsd_si64(v.raw));
  const Half<Full128<double>> dd2;
  const __m128i i1 = _mm_cvtsi64_si128(_mm_cvttsd_si64(UpperHalf(dd2, v).raw));
  return detail::FixConversionOverflow(di, v, _mm_unpacklo_epi64(i0, i1));
#else
  using VI = VFromD<decltype(di)>;
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
HWY_API Vec64<int64_t> ConvertTo(Full64<int64_t> di, const Vec64<double> v) {
  // Only need to specialize for non-AVX3, 64-bit (single scalar op)
#if HWY_TARGET > HWY_AVX3 && HWY_ARCH_X86_64
  const Vec64<int64_t> i0{_mm_cvtsi64_si128(_mm_cvttsd_si64(v.raw))};
  return detail::FixConversionOverflow(di, v, i0.raw);
#else
  (void)di;
  const auto full = ConvertTo(Full128<int64_t>(), Vec128<double>{v.raw});
  return Vec64<int64_t>{full.raw};
#endif
}

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  const Simd<int32_t, N, 0> di;
  return detail::FixConversionOverflow(di, v, _mm_cvtps_epi32(v.raw));
}

// ------------------------------ Floating-point rounding (ConvertTo)

#if HWY_TARGET == HWY_SSSE3

// Toward nearest integer, ties to even
template <typename T, size_t N>
HWY_API Vec128<T, N> Round(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  // Rely on rounding after addition with a large value such that no mantissa
  // bits remain (assuming the current mode is nearest-even). We may need a
  // compiler flag for precise floating-point to prevent "optimizing" this out.
  const Simd<T, N, 0> df;
  const auto max = Set(df, MantissaEnd<T>());
  const auto large = CopySignToAbs(max, v);
  const auto added = large + v;
  const auto rounded = added - large;
  // Keep original if NaN or the magnitude is large (already an int).
  return IfThenElse(Abs(v) < max, rounded, v);
}

namespace detail {

// Truncating to integer and converting back to float is correct except when the
// input magnitude is large, in which case the input was already an integer
// (because mantissa >> exponent is zero).
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> UseInt(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  return Abs(v) < Set(Simd<T, N, 0>(), MantissaEnd<T>());
}

}  // namespace detail

// Toward zero, aka truncate
template <typename T, size_t N>
HWY_API Vec128<T, N> Trunc(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// Toward +infinity, aka ceiling
template <typename T, size_t N>
HWY_API Vec128<T, N> Ceil(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a positive non-integer ends up smaller; if so, add 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f < v)));

  return IfThenElse(detail::UseInt(v), int_f - neg1, v);
}

// Toward -infinity, aka floor
template <typename T, size_t N>
HWY_API Vec128<T, N> Floor(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a negative non-integer ends up larger; if so, subtract 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f > v)));

  return IfThenElse(detail::UseInt(v), int_f + neg1, v);
}

#else

// Toward nearest integer, ties to even
template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Round(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Trunc(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Ceil(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Floor(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

#endif  // !HWY_SSSE3

// ------------------------------ Floating-point classification

template <size_t N>
HWY_API Mask128<float, N> IsNaN(const Vec128<float, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Mask128<float, N>{_mm_fpclass_ps_mask(v.raw, 0x81)};
#else
  return Mask128<float, N>{_mm_cmpunord_ps(v.raw, v.raw)};
#endif
}
template <size_t N>
HWY_API Mask128<double, N> IsNaN(const Vec128<double, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Mask128<double, N>{_mm_fpclass_pd_mask(v.raw, 0x81)};
#else
  return Mask128<double, N>{_mm_cmpunord_pd(v.raw, v.raw)};
#endif
}

#if HWY_TARGET <= HWY_AVX3

template <size_t N>
HWY_API Mask128<float, N> IsInf(const Vec128<float, N> v) {
  return Mask128<float, N>{_mm_fpclass_ps_mask(v.raw, 0x18)};
}
template <size_t N>
HWY_API Mask128<double, N> IsInf(const Vec128<double, N> v) {
  return Mask128<double, N>{_mm_fpclass_pd_mask(v.raw, 0x18)};
}

// Returns whether normal/subnormal/zero.
template <size_t N>
HWY_API Mask128<float, N> IsFinite(const Vec128<float, N> v) {
  // fpclass doesn't have a flag for positive, so we have to check for inf/NaN
  // and negate the mask.
  return Not(Mask128<float, N>{_mm_fpclass_ps_mask(v.raw, 0x99)});
}
template <size_t N>
HWY_API Mask128<double, N> IsFinite(const Vec128<double, N> v) {
  return Not(Mask128<double, N>{_mm_fpclass_pd_mask(v.raw, 0x99)});
}

#else

template <typename T, size_t N>
HWY_API Mask128<T, N> IsInf(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N>
HWY_API Mask128<T, N> IsFinite(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const Simd<T, N, 0> d;
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

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES) && HWY_TARGET != HWY_SSSE3

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec128<uint8_t> AESRound(Vec128<uint8_t> state,
                                 Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesenc_si128(state.raw, round_key.raw)};
}

HWY_API Vec128<uint8_t> AESLastRound(Vec128<uint8_t> state,
                                     Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesenclast_si128(state.raw, round_key.raw)};
}

template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> CLMulLower(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_clmulepi64_si128(a.raw, b.raw, 0x00)};
}

template <size_t N, HWY_IF_LE128(uint64_t, N)>
HWY_API Vec128<uint64_t, N> CLMulUpper(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_clmulepi64_si128(a.raw, b.raw, 0x11)};
}

#endif  // !defined(HWY_DISABLE_PCLMUL_AES) && HWY_TARGET != HWY_SSSE3

// ================================================== MISC

template <typename T>
struct CompressIsPartition {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 supports native compress, but a table-based approach allows
  // 'partitioning' (also moving mask=false lanes to the top), which helps
  // vqsort. This is only feasible for eight or less lanes, i.e. sizeof(T) == 8
  // on AVX3. For simplicity, we only use tables for 64-bit lanes (not AVX3
  // u32x8 etc.).
  enum { value = (sizeof(T) == 8) };
#else
  enum { value = 1 };
#endif
};

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ LoadMaskBits

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> LoadMaskBits(Simd<T, N, 0> /* tag */,
                                   const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return Mask128<T, N>::FromBits(mask_bits);
}

// ------------------------------ StoreMaskBits

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(const Simd<T, N, 0> /* tag */,
                             const Mask128<T, N> mask, uint8_t* bits) {
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(&mask.raw, bits);

  // Non-full byte, need to clear the undefined upper bits.
  if (N < 8) {
    const int mask = (1 << N) - 1;
    bits[0] = static_cast<uint8_t>(bits[0] & mask);
  }

  return kNumBytes;
}

// ------------------------------ Mask testing

// Beware: the suffix indicates the number of mask bits, not lane size!

template <typename T, size_t N>
HWY_API size_t CountTrue(const Simd<T, N, 0> /* tag */,
                         const Mask128<T, N> mask) {
  const uint64_t mask_bits = static_cast<uint64_t>(mask.raw) & ((1u << N) - 1);
  return PopCount(mask_bits);
}

template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(const Simd<T, N, 0> /* tag */,
                               const Mask128<T, N> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(mask.raw) & ((1u << N) - 1);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask_bits)) : -1;
}

template <typename T, size_t N>
HWY_API bool AllFalse(const Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  const uint64_t mask_bits = static_cast<uint64_t>(mask.raw) & ((1u << N) - 1);
  return mask_bits == 0;
}

template <typename T, size_t N>
HWY_API bool AllTrue(const Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  const uint64_t mask_bits = static_cast<uint64_t>(mask.raw) & ((1u << N) - 1);
  // Cannot use _kortestc because we may have less than 8 mask bits.
  return mask_bits == (1u << N) - 1;
}

// ------------------------------ Compress

#if HWY_TARGET != HWY_AVX3_DL
namespace detail {

// Returns permutevar_epi16 indices for 16-bit Compress. Also used by x86_256.
HWY_INLINE Vec128<uint16_t> IndicesForCompress16(uint64_t mask_bits) {
  Full128<uint16_t> du16;
  // Table of u16 indices packed into bytes to reduce L1 usage. Will be unpacked
  // to u16. Ideally we would broadcast 8*3 (half of the 8 bytes currently used)
  // bits into each lane and then varshift, but that does not fit in 16 bits.
  Rebind<uint8_t, decltype(du16)> du8;
  alignas(16) constexpr uint8_t tbl[2048] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 2,
      0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0,
      0, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 2, 3, 0, 0,
      0, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 1, 2, 3, 0,
      0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 1, 4, 0, 0, 0, 0,
      0, 0, 0, 1, 4, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 0, 0, 0, 0, 2, 4, 0, 0, 0, 0,
      0, 1, 2, 4, 0, 0, 0, 0, 0, 0, 1, 2, 4, 0, 0, 0, 0, 3, 4, 0, 0, 0, 0, 0, 0,
      0, 3, 4, 0, 0, 0, 0, 0, 1, 3, 4, 0, 0, 0, 0, 0, 0, 1, 3, 4, 0, 0, 0, 0, 2,
      3, 4, 0, 0, 0, 0, 0, 0, 2, 3, 4, 0, 0, 0, 0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 1,
      2, 3, 4, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 1, 5, 0,
      0, 0, 0, 0, 0, 0, 1, 5, 0, 0, 0, 0, 0, 2, 5, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0,
      0, 0, 0, 0, 1, 2, 5, 0, 0, 0, 0, 0, 0, 1, 2, 5, 0, 0, 0, 0, 3, 5, 0, 0, 0,
      0, 0, 0, 0, 3, 5, 0, 0, 0, 0, 0, 1, 3, 5, 0, 0, 0, 0, 0, 0, 1, 3, 5, 0, 0,
      0, 0, 2, 3, 5, 0, 0, 0, 0, 0, 0, 2, 3, 5, 0, 0, 0, 0, 1, 2, 3, 5, 0, 0, 0,
      0, 0, 1, 2, 3, 5, 0, 0, 0, 4, 5, 0, 0, 0, 0, 0, 0, 0, 4, 5, 0, 0, 0, 0, 0,
      1, 4, 5, 0, 0, 0, 0, 0, 0, 1, 4, 5, 0, 0, 0, 0, 2, 4, 5, 0, 0, 0, 0, 0, 0,
      2, 4, 5, 0, 0, 0, 0, 1, 2, 4, 5, 0, 0, 0, 0, 0, 1, 2, 4, 5, 0, 0, 0, 3, 4,
      5, 0, 0, 0, 0, 0, 0, 3, 4, 5, 0, 0, 0, 0, 1, 3, 4, 5, 0, 0, 0, 0, 0, 1, 3,
      4, 5, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 0, 0, 2, 3, 4, 5, 0, 0, 0, 1, 2, 3, 4,
      5, 0, 0, 0, 0, 1, 2, 3, 4, 5, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0,
      0, 0, 0, 1, 6, 0, 0, 0, 0, 0, 0, 0, 1, 6, 0, 0, 0, 0, 0, 2, 6, 0, 0, 0, 0,
      0, 0, 0, 2, 6, 0, 0, 0, 0, 0, 1, 2, 6, 0, 0, 0, 0, 0, 0, 1, 2, 6, 0, 0, 0,
      0, 3, 6, 0, 0, 0, 0, 0, 0, 0, 3, 6, 0, 0, 0, 0, 0, 1, 3, 6, 0, 0, 0, 0, 0,
      0, 1, 3, 6, 0, 0, 0, 0, 2, 3, 6, 0, 0, 0, 0, 0, 0, 2, 3, 6, 0, 0, 0, 0, 1,
      2, 3, 6, 0, 0, 0, 0, 0, 1, 2, 3, 6, 0, 0, 0, 4, 6, 0, 0, 0, 0, 0, 0, 0, 4,
      6, 0, 0, 0, 0, 0, 1, 4, 6, 0, 0, 0, 0, 0, 0, 1, 4, 6, 0, 0, 0, 0, 2, 4, 6,
      0, 0, 0, 0, 0, 0, 2, 4, 6, 0, 0, 0, 0, 1, 2, 4, 6, 0, 0, 0, 0, 0, 1, 2, 4,
      6, 0, 0, 0, 3, 4, 6, 0, 0, 0, 0, 0, 0, 3, 4, 6, 0, 0, 0, 0, 1, 3, 4, 6, 0,
      0, 0, 0, 0, 1, 3, 4, 6, 0, 0, 0, 2, 3, 4, 6, 0, 0, 0, 0, 0, 2, 3, 4, 6, 0,
      0, 0, 1, 2, 3, 4, 6, 0, 0, 0, 0, 1, 2, 3, 4, 6, 0, 0, 5, 6, 0, 0, 0, 0, 0,
      0, 0, 5, 6, 0, 0, 0, 0, 0, 1, 5, 6, 0, 0, 0, 0, 0, 0, 1, 5, 6, 0, 0, 0, 0,
      2, 5, 6, 0, 0, 0, 0, 0, 0, 2, 5, 6, 0, 0, 0, 0, 1, 2, 5, 6, 0, 0, 0, 0, 0,
      1, 2, 5, 6, 0, 0, 0, 3, 5, 6, 0, 0, 0, 0, 0, 0, 3, 5, 6, 0, 0, 0, 0, 1, 3,
      5, 6, 0, 0, 0, 0, 0, 1, 3, 5, 6, 0, 0, 0, 2, 3, 5, 6, 0, 0, 0, 0, 0, 2, 3,
      5, 6, 0, 0, 0, 1, 2, 3, 5, 6, 0, 0, 0, 0, 1, 2, 3, 5, 6, 0, 0, 4, 5, 6, 0,
      0, 0, 0, 0, 0, 4, 5, 6, 0, 0, 0, 0, 1, 4, 5, 6, 0, 0, 0, 0, 0, 1, 4, 5, 6,
      0, 0, 0, 2, 4, 5, 6, 0, 0, 0, 0, 0, 2, 4, 5, 6, 0, 0, 0, 1, 2, 4, 5, 6, 0,
      0, 0, 0, 1, 2, 4, 5, 6, 0, 0, 3, 4, 5, 6, 0, 0, 0, 0, 0, 3, 4, 5, 6, 0, 0,
      0, 1, 3, 4, 5, 6, 0, 0, 0, 0, 1, 3, 4, 5, 6, 0, 0, 2, 3, 4, 5, 6, 0, 0, 0,
      0, 2, 3, 4, 5, 6, 0, 0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 1, 2, 3, 4, 5, 6, 0, 7,
      0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 1, 7, 0, 0, 0, 0, 0, 0, 0, 1,
      7, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 2, 7, 0, 0, 0, 0, 0, 1, 2, 7,
      0, 0, 0, 0, 0, 0, 1, 2, 7, 0, 0, 0, 0, 3, 7, 0, 0, 0, 0, 0, 0, 0, 3, 7, 0,
      0, 0, 0, 0, 1, 3, 7, 0, 0, 0, 0, 0, 0, 1, 3, 7, 0, 0, 0, 0, 2, 3, 7, 0, 0,
      0, 0, 0, 0, 2, 3, 7, 0, 0, 0, 0, 1, 2, 3, 7, 0, 0, 0, 0, 0, 1, 2, 3, 7, 0,
      0, 0, 4, 7, 0, 0, 0, 0, 0, 0, 0, 4, 7, 0, 0, 0, 0, 0, 1, 4, 7, 0, 0, 0, 0,
      0, 0, 1, 4, 7, 0, 0, 0, 0, 2, 4, 7, 0, 0, 0, 0, 0, 0, 2, 4, 7, 0, 0, 0, 0,
      1, 2, 4, 7, 0, 0, 0, 0, 0, 1, 2, 4, 7, 0, 0, 0, 3, 4, 7, 0, 0, 0, 0, 0, 0,
      3, 4, 7, 0, 0, 0, 0, 1, 3, 4, 7, 0, 0, 0, 0, 0, 1, 3, 4, 7, 0, 0, 0, 2, 3,
      4, 7, 0, 0, 0, 0, 0, 2, 3, 4, 7, 0, 0, 0, 1, 2, 3, 4, 7, 0, 0, 0, 0, 1, 2,
      3, 4, 7, 0, 0, 5, 7, 0, 0, 0, 0, 0, 0, 0, 5, 7, 0, 0, 0, 0, 0, 1, 5, 7, 0,
      0, 0, 0, 0, 0, 1, 5, 7, 0, 0, 0, 0, 2, 5, 7, 0, 0, 0, 0, 0, 0, 2, 5, 7, 0,
      0, 0, 0, 1, 2, 5, 7, 0, 0, 0, 0, 0, 1, 2, 5, 7, 0, 0, 0, 3, 5, 7, 0, 0, 0,
      0, 0, 0, 3, 5, 7, 0, 0, 0, 0, 1, 3, 5, 7, 0, 0, 0, 0, 0, 1, 3, 5, 7, 0, 0,
      0, 2, 3, 5, 7, 0, 0, 0, 0, 0, 2, 3, 5, 7, 0, 0, 0, 1, 2, 3, 5, 7, 0, 0, 0,
      0, 1, 2, 3, 5, 7, 0, 0, 4, 5, 7, 0, 0, 0, 0, 0, 0, 4, 5, 7, 0, 0, 0, 0, 1,
      4, 5, 7, 0, 0, 0, 0, 0, 1, 4, 5, 7, 0, 0, 0, 2, 4, 5, 7, 0, 0, 0, 0, 0, 2,
      4, 5, 7, 0, 0, 0, 1, 2, 4, 5, 7, 0, 0, 0, 0, 1, 2, 4, 5, 7, 0, 0, 3, 4, 5,
      7, 0, 0, 0, 0, 0, 3, 4, 5, 7, 0, 0, 0, 1, 3, 4, 5, 7, 0, 0, 0, 0, 1, 3, 4,
      5, 7, 0, 0, 2, 3, 4, 5, 7, 0, 0, 0, 0, 2, 3, 4, 5, 7, 0, 0, 1, 2, 3, 4, 5,
      7, 0, 0, 0, 1, 2, 3, 4, 5, 7, 0, 6, 7, 0, 0, 0, 0, 0, 0, 0, 6, 7, 0, 0, 0,
      0, 0, 1, 6, 7, 0, 0, 0, 0, 0, 0, 1, 6, 7, 0, 0, 0, 0, 2, 6, 7, 0, 0, 0, 0,
      0, 0, 2, 6, 7, 0, 0, 0, 0, 1, 2, 6, 7, 0, 0, 0, 0, 0, 1, 2, 6, 7, 0, 0, 0,
      3, 6, 7, 0, 0, 0, 0, 0, 0, 3, 6, 7, 0, 0, 0, 0, 1, 3, 6, 7, 0, 0, 0, 0, 0,
      1, 3, 6, 7, 0, 0, 0, 2, 3, 6, 7, 0, 0, 0, 0, 0, 2, 3, 6, 7, 0, 0, 0, 1, 2,
      3, 6, 7, 0, 0, 0, 0, 1, 2, 3, 6, 7, 0, 0, 4, 6, 7, 0, 0, 0, 0, 0, 0, 4, 6,
      7, 0, 0, 0, 0, 1, 4, 6, 7, 0, 0, 0, 0, 0, 1, 4, 6, 7, 0, 0, 0, 2, 4, 6, 7,
      0, 0, 0, 0, 0, 2, 4, 6, 7, 0, 0, 0, 1, 2, 4, 6, 7, 0, 0, 0, 0, 1, 2, 4, 6,
      7, 0, 0, 3, 4, 6, 7, 0, 0, 0, 0, 0, 3, 4, 6, 7, 0, 0, 0, 1, 3, 4, 6, 7, 0,
      0, 0, 0, 1, 3, 4, 6, 7, 0, 0, 2, 3, 4, 6, 7, 0, 0, 0, 0, 2, 3, 4, 6, 7, 0,
      0, 1, 2, 3, 4, 6, 7, 0, 0, 0, 1, 2, 3, 4, 6, 7, 0, 5, 6, 7, 0, 0, 0, 0, 0,
      0, 5, 6, 7, 0, 0, 0, 0, 1, 5, 6, 7, 0, 0, 0, 0, 0, 1, 5, 6, 7, 0, 0, 0, 2,
      5, 6, 7, 0, 0, 0, 0, 0, 2, 5, 6, 7, 0, 0, 0, 1, 2, 5, 6, 7, 0, 0, 0, 0, 1,
      2, 5, 6, 7, 0, 0, 3, 5, 6, 7, 0, 0, 0, 0, 0, 3, 5, 6, 7, 0, 0, 0, 1, 3, 5,
      6, 7, 0, 0, 0, 0, 1, 3, 5, 6, 7, 0, 0, 2, 3, 5, 6, 7, 0, 0, 0, 0, 2, 3, 5,
      6, 7, 0, 0, 1, 2, 3, 5, 6, 7, 0, 0, 0, 1, 2, 3, 5, 6, 7, 0, 4, 5, 6, 7, 0,
      0, 0, 0, 0, 4, 5, 6, 7, 0, 0, 0, 1, 4, 5, 6, 7, 0, 0, 0, 0, 1, 4, 5, 6, 7,
      0, 0, 2, 4, 5, 6, 7, 0, 0, 0, 0, 2, 4, 5, 6, 7, 0, 0, 1, 2, 4, 5, 6, 7, 0,
      0, 0, 1, 2, 4, 5, 6, 7, 0, 3, 4, 5, 6, 7, 0, 0, 0, 0, 3, 4, 5, 6, 7, 0, 0,
      1, 3, 4, 5, 6, 7, 0, 0, 0, 1, 3, 4, 5, 6, 7, 0, 2, 3, 4, 5, 6, 7, 0, 0, 0,
      2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 1, 2, 3, 4, 5, 6, 7};
  return PromoteTo(du16, Load(du8, tbl + mask_bits * 8));
}

}  // namespace detail
#endif  // HWY_TARGET != HWY_AVX3_DL

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  const Vec128<uint16_t, N> cu{_mm_maskz_compress_epi16(mask.raw, vu.raw)};
#else
  const auto idx = detail::IndicesForCompress16(uint64_t{mask.raw});
  const Vec128<uint16_t, N> cu{_mm_permutexvar_epi16(idx.raw, vu.raw)};
#endif  // HWY_TARGET != HWY_AVX3_DL
  return BitCast(d, cu);
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  return Vec128<T, N>{_mm_maskz_compress_epi32(mask.raw, v.raw)};
}

template <size_t N, HWY_IF_GE64(float, N)>
HWY_API Vec128<float, N> Compress(Vec128<float, N> v, Mask128<float, N> mask) {
  return Vec128<float, N>{_mm_maskz_compress_ps(mask.raw, v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> Compress(Vec128<T> v, Mask128<T> mask) {
  HWY_DASSERT(mask.raw < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[64] = {
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Full128<T> d;
  const Repartition<uint8_t, decltype(d)> d8;
  const auto index = Load(d8, u8_indices + 16 * mask.raw);
  return BitCast(d, TableLookupBytes(BitCast(d8, v), index));
}

// ------------------------------ CompressNot (Compress)

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  return Compress(v, Not(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

// ------------------------------ CompressBits (LoadMaskBits)

template <typename T, size_t N>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v,
                                  const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(Simd<T, N, 0>(), bits));
}

// ------------------------------ CompressStore

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CompressStore(Vec128<T, N> v, Mask128<T, N> mask,
                             Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

  const uint64_t mask_bits{mask.raw};

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  _mm_mask_compressstoreu_epi16(unaligned, mask.raw, vu.raw);
#else
  const auto idx = detail::IndicesForCompress16(mask_bits);
  const Vec128<uint16_t, N> cu{_mm_permutexvar_epi16(idx.raw, vu.raw)};
  StoreU(BitCast(d, cu), d, unaligned);
#endif  // HWY_TARGET == HWY_AVX3_DL

  const size_t count = PopCount(mask_bits & ((1ull << N) - 1));
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API size_t CompressStore(Vec128<T, N> v, Mask128<T, N> mask,
                             Simd<T, N, 0> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw} & ((1ull << N) - 1));
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API size_t CompressStore(Vec128<T, N> v, Mask128<T, N> mask,
                             Simd<T, N, 0> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw} & ((1ull << N) - 1));
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <size_t N, HWY_IF_LE128(float, N)>
HWY_API size_t CompressStore(Vec128<float, N> v, Mask128<float, N> mask,
                             Simd<float, N, 0> /* tag */,
                             float* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw} & ((1ull << N) - 1));
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(float));
#endif
  return count;
}

template <size_t N, HWY_IF_LE128(double, N)>
HWY_API size_t CompressStore(Vec128<double, N> v, Mask128<double, N> mask,
                             Simd<double, N, 0> /* tag */,
                             double* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
  const size_t count = PopCount(uint64_t{mask.raw} & ((1ull << N) - 1));
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(double));
#endif
  return count;
}

// ------------------------------ CompressBlendedStore (CompressStore)
template <typename T, size_t N>
HWY_API size_t CompressBlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  // AVX-512 already does the blending at no extra cost (latency 11,
  // rthroughput 2 - same as compress plus store).
  if (HWY_TARGET == HWY_AVX3_DL || sizeof(T) != 2) {
    // We're relying on the mask to blend. Clear the undefined upper bits.
    if (N != 16 / sizeof(T)) {
      m = And(m, FirstN(d, N));
    }
    return CompressStore(v, m, d, unaligned);
  } else {
    const size_t count = CountTrue(d, m);
    const Vec128<T, N> compressed = Compress(v, m);
#if HWY_MEM_OPS_MIGHT_FAULT
    // BlendedStore tests mask for each lane, but we know that the mask is
    // FirstN, so we can just copy.
    alignas(16) T buf[N];
    Store(compressed, d, buf);
    memcpy(unaligned, buf, count * sizeof(T));
#else
    BlendedStore(compressed, FirstN(d, count), d, unaligned);
#endif
    // Workaround: as of 2022-02-23 MSAN does not mark the output as
    // initialized.
#if HWY_IS_MSAN
    __msan_unpoison(unaligned, count * sizeof(T));
#endif
    return count;
  }
}

// ------------------------------ CompressBitsStore (LoadMaskBits)

template <typename T, size_t N>
HWY_API size_t CompressBitsStore(Vec128<T, N> v,
                                 const uint8_t* HWY_RESTRICT bits,
                                 Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

#else  // AVX2 or below

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, N=1.
  const Vec128<T, N> vbits{_mm_cvtsi32_si128(static_cast<int>(mask_bits))};

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(16) constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                             1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vbits, Load(du, kRep8));

  alignas(16) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> LoadMaskBits(Simd<T, N, 0> d,
                                   const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::LoadMaskBits(d, mask_bits);
}

// ------------------------------ StoreMaskBits

namespace detail {

constexpr HWY_INLINE uint64_t U64FromInt(int mask_bits) {
  return static_cast<uint64_t>(static_cast<unsigned>(mask_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const auto sign_bits = BitCast(d, VecFromMask(d, mask)).raw;
  return U64FromInt(_mm_movemask_epi8(sign_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Remove useless lower half of each u16 while preserving the sign bit.
  const auto sign_bits = _mm_packs_epi16(mask.raw, _mm_setzero_si128());
  return U64FromInt(_mm_movemask_epi8(sign_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const Simd<float, N, 0> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask));
  return U64FromInt(_mm_movemask_ps(sign_bits.raw));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const Simd<double, N, 0> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask));
  return U64FromInt(_mm_movemask_pd(sign_bits.raw));
}

// Returns the lowest N of the _mm_movemask* bits.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t mask_bits) {
  return ((N * sizeof(T)) == 16) ? mask_bits : mask_bits & ((1ull << N) - 1);
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(const Simd<T, N, 0> /* tag */,
                             const Mask128<T, N> mask, uint8_t* bits) {
  constexpr size_t kNumBytes = (N + 7) / 8;
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

// ------------------------------ Mask testing

template <typename T, size_t N>
HWY_API bool AllFalse(const Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  // Cheaper than PTEST, which is 2 uop / 3L.
  return detail::BitsFromMask(mask) == 0;
}

template <typename T, size_t N>
HWY_API bool AllTrue(const Simd<T, N, 0> /* tag */, const Mask128<T, N> mask) {
  constexpr uint64_t kAllBits =
      detail::OnlyActive<T, N>((1ull << (16 / sizeof(T))) - 1);
  return detail::BitsFromMask(mask) == kAllBits;
}

template <typename T, size_t N>
HWY_API size_t CountTrue(const Simd<T, N, 0> /* tag */,
                         const Mask128<T, N> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(const Simd<T, N, 0> /* tag */,
                               const Mask128<T, N> mask) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero64(mask_bits)) : -1;
}

// ------------------------------ Compress, CompressBits

namespace detail {

// Also works for N < 8 because the first 16 4-tuples only reference bytes 0-6.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> IndicesFromBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N, 0> du;

  // compress_epi16 requires VBMI2 and there is no permutevar_epi16, so we need
  // byte indices for PSHUFB (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[2048] = {
      // PrintCompress16x8Tables
      0,  2,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      2,  0,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      4,  0,  2,  6,  8,  10, 12, 14, /**/ 0, 4,  2,  6,  8,  10, 12, 14,  //
      2,  4,  0,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      6,  0,  2,  4,  8,  10, 12, 14, /**/ 0, 6,  2,  4,  8,  10, 12, 14,  //
      2,  6,  0,  4,  8,  10, 12, 14, /**/ 0, 2,  6,  4,  8,  10, 12, 14,  //
      4,  6,  0,  2,  8,  10, 12, 14, /**/ 0, 4,  6,  2,  8,  10, 12, 14,  //
      2,  4,  6,  0,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      8,  0,  2,  4,  6,  10, 12, 14, /**/ 0, 8,  2,  4,  6,  10, 12, 14,  //
      2,  8,  0,  4,  6,  10, 12, 14, /**/ 0, 2,  8,  4,  6,  10, 12, 14,  //
      4,  8,  0,  2,  6,  10, 12, 14, /**/ 0, 4,  8,  2,  6,  10, 12, 14,  //
      2,  4,  8,  0,  6,  10, 12, 14, /**/ 0, 2,  4,  8,  6,  10, 12, 14,  //
      6,  8,  0,  2,  4,  10, 12, 14, /**/ 0, 6,  8,  2,  4,  10, 12, 14,  //
      2,  6,  8,  0,  4,  10, 12, 14, /**/ 0, 2,  6,  8,  4,  10, 12, 14,  //
      4,  6,  8,  0,  2,  10, 12, 14, /**/ 0, 4,  6,  8,  2,  10, 12, 14,  //
      2,  4,  6,  8,  0,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      10, 0,  2,  4,  6,  8,  12, 14, /**/ 0, 10, 2,  4,  6,  8,  12, 14,  //
      2,  10, 0,  4,  6,  8,  12, 14, /**/ 0, 2,  10, 4,  6,  8,  12, 14,  //
      4,  10, 0,  2,  6,  8,  12, 14, /**/ 0, 4,  10, 2,  6,  8,  12, 14,  //
      2,  4,  10, 0,  6,  8,  12, 14, /**/ 0, 2,  4,  10, 6,  8,  12, 14,  //
      6,  10, 0,  2,  4,  8,  12, 14, /**/ 0, 6,  10, 2,  4,  8,  12, 14,  //
      2,  6,  10, 0,  4,  8,  12, 14, /**/ 0, 2,  6,  10, 4,  8,  12, 14,  //
      4,  6,  10, 0,  2,  8,  12, 14, /**/ 0, 4,  6,  10, 2,  8,  12, 14,  //
      2,  4,  6,  10, 0,  8,  12, 14, /**/ 0, 2,  4,  6,  10, 8,  12, 14,  //
      8,  10, 0,  2,  4,  6,  12, 14, /**/ 0, 8,  10, 2,  4,  6,  12, 14,  //
      2,  8,  10, 0,  4,  6,  12, 14, /**/ 0, 2,  8,  10, 4,  6,  12, 14,  //
      4,  8,  10, 0,  2,  6,  12, 14, /**/ 0, 4,  8,  10, 2,  6,  12, 14,  //
      2,  4,  8,  10, 0,  6,  12, 14, /**/ 0, 2,  4,  8,  10, 6,  12, 14,  //
      6,  8,  10, 0,  2,  4,  12, 14, /**/ 0, 6,  8,  10, 2,  4,  12, 14,  //
      2,  6,  8,  10, 0,  4,  12, 14, /**/ 0, 2,  6,  8,  10, 4,  12, 14,  //
      4,  6,  8,  10, 0,  2,  12, 14, /**/ 0, 4,  6,  8,  10, 2,  12, 14,  //
      2,  4,  6,  8,  10, 0,  12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      12, 0,  2,  4,  6,  8,  10, 14, /**/ 0, 12, 2,  4,  6,  8,  10, 14,  //
      2,  12, 0,  4,  6,  8,  10, 14, /**/ 0, 2,  12, 4,  6,  8,  10, 14,  //
      4,  12, 0,  2,  6,  8,  10, 14, /**/ 0, 4,  12, 2,  6,  8,  10, 14,  //
      2,  4,  12, 0,  6,  8,  10, 14, /**/ 0, 2,  4,  12, 6,  8,  10, 14,  //
      6,  12, 0,  2,  4,  8,  10, 14, /**/ 0, 6,  12, 2,  4,  8,  10, 14,  //
      2,  6,  12, 0,  4,  8,  10, 14, /**/ 0, 2,  6,  12, 4,  8,  10, 14,  //
      4,  6,  12, 0,  2,  8,  10, 14, /**/ 0, 4,  6,  12, 2,  8,  10, 14,  //
      2,  4,  6,  12, 0,  8,  10, 14, /**/ 0, 2,  4,  6,  12, 8,  10, 14,  //
      8,  12, 0,  2,  4,  6,  10, 14, /**/ 0, 8,  12, 2,  4,  6,  10, 14,  //
      2,  8,  12, 0,  4,  6,  10, 14, /**/ 0, 2,  8,  12, 4,  6,  10, 14,  //
      4,  8,  12, 0,  2,  6,  10, 14, /**/ 0, 4,  8,  12, 2,  6,  10, 14,  //
      2,  4,  8,  12, 0,  6,  10, 14, /**/ 0, 2,  4,  8,  12, 6,  10, 14,  //
      6,  8,  12, 0,  2,  4,  10, 14, /**/ 0, 6,  8,  12, 2,  4,  10, 14,  //
      2,  6,  8,  12, 0,  4,  10, 14, /**/ 0, 2,  6,  8,  12, 4,  10, 14,  //
      4,  6,  8,  12, 0,  2,  10, 14, /**/ 0, 4,  6,  8,  12, 2,  10, 14,  //
      2,  4,  6,  8,  12, 0,  10, 14, /**/ 0, 2,  4,  6,  8,  12, 10, 14,  //
      10, 12, 0,  2,  4,  6,  8,  14, /**/ 0, 10, 12, 2,  4,  6,  8,  14,  //
      2,  10, 12, 0,  4,  6,  8,  14, /**/ 0, 2,  10, 12, 4,  6,  8,  14,  //
      4,  10, 12, 0,  2,  6,  8,  14, /**/ 0, 4,  10, 12, 2,  6,  8,  14,  //
      2,  4,  10, 12, 0,  6,  8,  14, /**/ 0, 2,  4,  10, 12, 6,  8,  14,  //
      6,  10, 12, 0,  2,  4,  8,  14, /**/ 0, 6,  10, 12, 2,  4,  8,  14,  //
      2,  6,  10, 12, 0,  4,  8,  14, /**/ 0, 2,  6,  10, 12, 4,  8,  14,  //
      4,  6,  10, 12, 0,  2,  8,  14, /**/ 0, 4,  6,  10, 12, 2,  8,  14,  //
      2,  4,  6,  10, 12, 0,  8,  14, /**/ 0, 2,  4,  6,  10, 12, 8,  14,  //
      8,  10, 12, 0,  2,  4,  6,  14, /**/ 0, 8,  10, 12, 2,  4,  6,  14,  //
      2,  8,  10, 12, 0,  4,  6,  14, /**/ 0, 2,  8,  10, 12, 4,  6,  14,  //
      4,  8,  10, 12, 0,  2,  6,  14, /**/ 0, 4,  8,  10, 12, 2,  6,  14,  //
      2,  4,  8,  10, 12, 0,  6,  14, /**/ 0, 2,  4,  8,  10, 12, 6,  14,  //
      6,  8,  10, 12, 0,  2,  4,  14, /**/ 0, 6,  8,  10, 12, 2,  4,  14,  //
      2,  6,  8,  10, 12, 0,  4,  14, /**/ 0, 2,  6,  8,  10, 12, 4,  14,  //
      4,  6,  8,  10, 12, 0,  2,  14, /**/ 0, 4,  6,  8,  10, 12, 2,  14,  //
      2,  4,  6,  8,  10, 12, 0,  14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      14, 0,  2,  4,  6,  8,  10, 12, /**/ 0, 14, 2,  4,  6,  8,  10, 12,  //
      2,  14, 0,  4,  6,  8,  10, 12, /**/ 0, 2,  14, 4,  6,  8,  10, 12,  //
      4,  14, 0,  2,  6,  8,  10, 12, /**/ 0, 4,  14, 2,  6,  8,  10, 12,  //
      2,  4,  14, 0,  6,  8,  10, 12, /**/ 0, 2,  4,  14, 6,  8,  10, 12,  //
      6,  14, 0,  2,  4,  8,  10, 12, /**/ 0, 6,  14, 2,  4,  8,  10, 12,  //
      2,  6,  14, 0,  4,  8,  10, 12, /**/ 0, 2,  6,  14, 4,  8,  10, 12,  //
      4,  6,  14, 0,  2,  8,  10, 12, /**/ 0, 4,  6,  14, 2,  8,  10, 12,  //
      2,  4,  6,  14, 0,  8,  10, 12, /**/ 0, 2,  4,  6,  14, 8,  10, 12,  //
      8,  14, 0,  2,  4,  6,  10, 12, /**/ 0, 8,  14, 2,  4,  6,  10, 12,  //
      2,  8,  14, 0,  4,  6,  10, 12, /**/ 0, 2,  8,  14, 4,  6,  10, 12,  //
      4,  8,  14, 0,  2,  6,  10, 12, /**/ 0, 4,  8,  14, 2,  6,  10, 12,  //
      2,  4,  8,  14, 0,  6,  10, 12, /**/ 0, 2,  4,  8,  14, 6,  10, 12,  //
      6,  8,  14, 0,  2,  4,  10, 12, /**/ 0, 6,  8,  14, 2,  4,  10, 12,  //
      2,  6,  8,  14, 0,  4,  10, 12, /**/ 0, 2,  6,  8,  14, 4,  10, 12,  //
      4,  6,  8,  14, 0,  2,  10, 12, /**/ 0, 4,  6,  8,  14, 2,  10, 12,  //
      2,  4,  6,  8,  14, 0,  10, 12, /**/ 0, 2,  4,  6,  8,  14, 10, 12,  //
      10, 14, 0,  2,  4,  6,  8,  12, /**/ 0, 10, 14, 2,  4,  6,  8,  12,  //
      2,  10, 14, 0,  4,  6,  8,  12, /**/ 0, 2,  10, 14, 4,  6,  8,  12,  //
      4,  10, 14, 0,  2,  6,  8,  12, /**/ 0, 4,  10, 14, 2,  6,  8,  12,  //
      2,  4,  10, 14, 0,  6,  8,  12, /**/ 0, 2,  4,  10, 14, 6,  8,  12,  //
      6,  10, 14, 0,  2,  4,  8,  12, /**/ 0, 6,  10, 14, 2,  4,  8,  12,  //
      2,  6,  10, 14, 0,  4,  8,  12, /**/ 0, 2,  6,  10, 14, 4,  8,  12,  //
      4,  6,  10, 14, 0,  2,  8,  12, /**/ 0, 4,  6,  10, 14, 2,  8,  12,  //
      2,  4,  6,  10, 14, 0,  8,  12, /**/ 0, 2,  4,  6,  10, 14, 8,  12,  //
      8,  10, 14, 0,  2,  4,  6,  12, /**/ 0, 8,  10, 14, 2,  4,  6,  12,  //
      2,  8,  10, 14, 0,  4,  6,  12, /**/ 0, 2,  8,  10, 14, 4,  6,  12,  //
      4,  8,  10, 14, 0,  2,  6,  12, /**/ 0, 4,  8,  10, 14, 2,  6,  12,  //
      2,  4,  8,  10, 14, 0,  6,  12, /**/ 0, 2,  4,  8,  10, 14, 6,  12,  //
      6,  8,  10, 14, 0,  2,  4,  12, /**/ 0, 6,  8,  10, 14, 2,  4,  12,  //
      2,  6,  8,  10, 14, 0,  4,  12, /**/ 0, 2,  6,  8,  10, 14, 4,  12,  //
      4,  6,  8,  10, 14, 0,  2,  12, /**/ 0, 4,  6,  8,  10, 14, 2,  12,  //
      2,  4,  6,  8,  10, 14, 0,  12, /**/ 0, 2,  4,  6,  8,  10, 14, 12,  //
      12, 14, 0,  2,  4,  6,  8,  10, /**/ 0, 12, 14, 2,  4,  6,  8,  10,  //
      2,  12, 14, 0,  4,  6,  8,  10, /**/ 0, 2,  12, 14, 4,  6,  8,  10,  //
      4,  12, 14, 0,  2,  6,  8,  10, /**/ 0, 4,  12, 14, 2,  6,  8,  10,  //
      2,  4,  12, 14, 0,  6,  8,  10, /**/ 0, 2,  4,  12, 14, 6,  8,  10,  //
      6,  12, 14, 0,  2,  4,  8,  10, /**/ 0, 6,  12, 14, 2,  4,  8,  10,  //
      2,  6,  12, 14, 0,  4,  8,  10, /**/ 0, 2,  6,  12, 14, 4,  8,  10,  //
      4,  6,  12, 14, 0,  2,  8,  10, /**/ 0, 4,  6,  12, 14, 2,  8,  10,  //
      2,  4,  6,  12, 14, 0,  8,  10, /**/ 0, 2,  4,  6,  12, 14, 8,  10,  //
      8,  12, 14, 0,  2,  4,  6,  10, /**/ 0, 8,  12, 14, 2,  4,  6,  10,  //
      2,  8,  12, 14, 0,  4,  6,  10, /**/ 0, 2,  8,  12, 14, 4,  6,  10,  //
      4,  8,  12, 14, 0,  2,  6,  10, /**/ 0, 4,  8,  12, 14, 2,  6,  10,  //
      2,  4,  8,  12, 14, 0,  6,  10, /**/ 0, 2,  4,  8,  12, 14, 6,  10,  //
      6,  8,  12, 14, 0,  2,  4,  10, /**/ 0, 6,  8,  12, 14, 2,  4,  10,  //
      2,  6,  8,  12, 14, 0,  4,  10, /**/ 0, 2,  6,  8,  12, 14, 4,  10,  //
      4,  6,  8,  12, 14, 0,  2,  10, /**/ 0, 4,  6,  8,  12, 14, 2,  10,  //
      2,  4,  6,  8,  12, 14, 0,  10, /**/ 0, 2,  4,  6,  8,  12, 14, 10,  //
      10, 12, 14, 0,  2,  4,  6,  8,  /**/ 0, 10, 12, 14, 2,  4,  6,  8,   //
      2,  10, 12, 14, 0,  4,  6,  8,  /**/ 0, 2,  10, 12, 14, 4,  6,  8,   //
      4,  10, 12, 14, 0,  2,  6,  8,  /**/ 0, 4,  10, 12, 14, 2,  6,  8,   //
      2,  4,  10, 12, 14, 0,  6,  8,  /**/ 0, 2,  4,  10, 12, 14, 6,  8,   //
      6,  10, 12, 14, 0,  2,  4,  8,  /**/ 0, 6,  10, 12, 14, 2,  4,  8,   //
      2,  6,  10, 12, 14, 0,  4,  8,  /**/ 0, 2,  6,  10, 12, 14, 4,  8,   //
      4,  6,  10, 12, 14, 0,  2,  8,  /**/ 0, 4,  6,  10, 12, 14, 2,  8,   //
      2,  4,  6,  10, 12, 14, 0,  8,  /**/ 0, 2,  4,  6,  10, 12, 14, 8,   //
      8,  10, 12, 14, 0,  2,  4,  6,  /**/ 0, 8,  10, 12, 14, 2,  4,  6,   //
      2,  8,  10, 12, 14, 0,  4,  6,  /**/ 0, 2,  8,  10, 12, 14, 4,  6,   //
      4,  8,  10, 12, 14, 0,  2,  6,  /**/ 0, 4,  8,  10, 12, 14, 2,  6,   //
      2,  4,  8,  10, 12, 14, 0,  6,  /**/ 0, 2,  4,  8,  10, 12, 14, 6,   //
      6,  8,  10, 12, 14, 0,  2,  4,  /**/ 0, 6,  8,  10, 12, 14, 2,  4,   //
      2,  6,  8,  10, 12, 14, 0,  4,  /**/ 0, 2,  6,  8,  10, 12, 14, 4,   //
      4,  6,  8,  10, 12, 14, 0,  2,  /**/ 0, 4,  6,  8,  10, 12, 14, 2,   //
      2,  4,  6,  8,  10, 12, 14, 0,  /**/ 0, 2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> IndicesFromNotBits(Simd<T, N, 0> d,
                                           uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N, 0> du;

  // compress_epi16 requires VBMI2 and there is no permutevar_epi16, so we need
  // byte indices for PSHUFB (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[2048] = {
      // PrintCompressNot16x8Tables
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 14, 0,   //
      0, 4,  6,  8,  10, 12, 14, 2,  /**/ 4,  6,  8,  10, 12, 14, 0,  2,   //
      0, 2,  6,  8,  10, 12, 14, 4,  /**/ 2,  6,  8,  10, 12, 14, 0,  4,   //
      0, 6,  8,  10, 12, 14, 2,  4,  /**/ 6,  8,  10, 12, 14, 0,  2,  4,   //
      0, 2,  4,  8,  10, 12, 14, 6,  /**/ 2,  4,  8,  10, 12, 14, 0,  6,   //
      0, 4,  8,  10, 12, 14, 2,  6,  /**/ 4,  8,  10, 12, 14, 0,  2,  6,   //
      0, 2,  8,  10, 12, 14, 4,  6,  /**/ 2,  8,  10, 12, 14, 0,  4,  6,   //
      0, 8,  10, 12, 14, 2,  4,  6,  /**/ 8,  10, 12, 14, 0,  2,  4,  6,   //
      0, 2,  4,  6,  10, 12, 14, 8,  /**/ 2,  4,  6,  10, 12, 14, 0,  8,   //
      0, 4,  6,  10, 12, 14, 2,  8,  /**/ 4,  6,  10, 12, 14, 0,  2,  8,   //
      0, 2,  6,  10, 12, 14, 4,  8,  /**/ 2,  6,  10, 12, 14, 0,  4,  8,   //
      0, 6,  10, 12, 14, 2,  4,  8,  /**/ 6,  10, 12, 14, 0,  2,  4,  8,   //
      0, 2,  4,  10, 12, 14, 6,  8,  /**/ 2,  4,  10, 12, 14, 0,  6,  8,   //
      0, 4,  10, 12, 14, 2,  6,  8,  /**/ 4,  10, 12, 14, 0,  2,  6,  8,   //
      0, 2,  10, 12, 14, 4,  6,  8,  /**/ 2,  10, 12, 14, 0,  4,  6,  8,   //
      0, 10, 12, 14, 2,  4,  6,  8,  /**/ 10, 12, 14, 0,  2,  4,  6,  8,   //
      0, 2,  4,  6,  8,  12, 14, 10, /**/ 2,  4,  6,  8,  12, 14, 0,  10,  //
      0, 4,  6,  8,  12, 14, 2,  10, /**/ 4,  6,  8,  12, 14, 0,  2,  10,  //
      0, 2,  6,  8,  12, 14, 4,  10, /**/ 2,  6,  8,  12, 14, 0,  4,  10,  //
      0, 6,  8,  12, 14, 2,  4,  10, /**/ 6,  8,  12, 14, 0,  2,  4,  10,  //
      0, 2,  4,  8,  12, 14, 6,  10, /**/ 2,  4,  8,  12, 14, 0,  6,  10,  //
      0, 4,  8,  12, 14, 2,  6,  10, /**/ 4,  8,  12, 14, 0,  2,  6,  10,  //
      0, 2,  8,  12, 14, 4,  6,  10, /**/ 2,  8,  12, 14, 0,  4,  6,  10,  //
      0, 8,  12, 14, 2,  4,  6,  10, /**/ 8,  12, 14, 0,  2,  4,  6,  10,  //
      0, 2,  4,  6,  12, 14, 8,  10, /**/ 2,  4,  6,  12, 14, 0,  8,  10,  //
      0, 4,  6,  12, 14, 2,  8,  10, /**/ 4,  6,  12, 14, 0,  2,  8,  10,  //
      0, 2,  6,  12, 14, 4,  8,  10, /**/ 2,  6,  12, 14, 0,  4,  8,  10,  //
      0, 6,  12, 14, 2,  4,  8,  10, /**/ 6,  12, 14, 0,  2,  4,  8,  10,  //
      0, 2,  4,  12, 14, 6,  8,  10, /**/ 2,  4,  12, 14, 0,  6,  8,  10,  //
      0, 4,  12, 14, 2,  6,  8,  10, /**/ 4,  12, 14, 0,  2,  6,  8,  10,  //
      0, 2,  12, 14, 4,  6,  8,  10, /**/ 2,  12, 14, 0,  4,  6,  8,  10,  //
      0, 12, 14, 2,  4,  6,  8,  10, /**/ 12, 14, 0,  2,  4,  6,  8,  10,  //
      0, 2,  4,  6,  8,  10, 14, 12, /**/ 2,  4,  6,  8,  10, 14, 0,  12,  //
      0, 4,  6,  8,  10, 14, 2,  12, /**/ 4,  6,  8,  10, 14, 0,  2,  12,  //
      0, 2,  6,  8,  10, 14, 4,  12, /**/ 2,  6,  8,  10, 14, 0,  4,  12,  //
      0, 6,  8,  10, 14, 2,  4,  12, /**/ 6,  8,  10, 14, 0,  2,  4,  12,  //
      0, 2,  4,  8,  10, 14, 6,  12, /**/ 2,  4,  8,  10, 14, 0,  6,  12,  //
      0, 4,  8,  10, 14, 2,  6,  12, /**/ 4,  8,  10, 14, 0,  2,  6,  12,  //
      0, 2,  8,  10, 14, 4,  6,  12, /**/ 2,  8,  10, 14, 0,  4,  6,  12,  //
      0, 8,  10, 14, 2,  4,  6,  12, /**/ 8,  10, 14, 0,  2,  4,  6,  12,  //
      0, 2,  4,  6,  10, 14, 8,  12, /**/ 2,  4,  6,  10, 14, 0,  8,  12,  //
      0, 4,  6,  10, 14, 2,  8,  12, /**/ 4,  6,  10, 14, 0,  2,  8,  12,  //
      0, 2,  6,  10, 14, 4,  8,  12, /**/ 2,  6,  10, 14, 0,  4,  8,  12,  //
      0, 6,  10, 14, 2,  4,  8,  12, /**/ 6,  10, 14, 0,  2,  4,  8,  12,  //
      0, 2,  4,  10, 14, 6,  8,  12, /**/ 2,  4,  10, 14, 0,  6,  8,  12,  //
      0, 4,  10, 14, 2,  6,  8,  12, /**/ 4,  10, 14, 0,  2,  6,  8,  12,  //
      0, 2,  10, 14, 4,  6,  8,  12, /**/ 2,  10, 14, 0,  4,  6,  8,  12,  //
      0, 10, 14, 2,  4,  6,  8,  12, /**/ 10, 14, 0,  2,  4,  6,  8,  12,  //
      0, 2,  4,  6,  8,  14, 10, 12, /**/ 2,  4,  6,  8,  14, 0,  10, 12,  //
      0, 4,  6,  8,  14, 2,  10, 12, /**/ 4,  6,  8,  14, 0,  2,  10, 12,  //
      0, 2,  6,  8,  14, 4,  10, 12, /**/ 2,  6,  8,  14, 0,  4,  10, 12,  //
      0, 6,  8,  14, 2,  4,  10, 12, /**/ 6,  8,  14, 0,  2,  4,  10, 12,  //
      0, 2,  4,  8,  14, 6,  10, 12, /**/ 2,  4,  8,  14, 0,  6,  10, 12,  //
      0, 4,  8,  14, 2,  6,  10, 12, /**/ 4,  8,  14, 0,  2,  6,  10, 12,  //
      0, 2,  8,  14, 4,  6,  10, 12, /**/ 2,  8,  14, 0,  4,  6,  10, 12,  //
      0, 8,  14, 2,  4,  6,  10, 12, /**/ 8,  14, 0,  2,  4,  6,  10, 12,  //
      0, 2,  4,  6,  14, 8,  10, 12, /**/ 2,  4,  6,  14, 0,  8,  10, 12,  //
      0, 4,  6,  14, 2,  8,  10, 12, /**/ 4,  6,  14, 0,  2,  8,  10, 12,  //
      0, 2,  6,  14, 4,  8,  10, 12, /**/ 2,  6,  14, 0,  4,  8,  10, 12,  //
      0, 6,  14, 2,  4,  8,  10, 12, /**/ 6,  14, 0,  2,  4,  8,  10, 12,  //
      0, 2,  4,  14, 6,  8,  10, 12, /**/ 2,  4,  14, 0,  6,  8,  10, 12,  //
      0, 4,  14, 2,  6,  8,  10, 12, /**/ 4,  14, 0,  2,  6,  8,  10, 12,  //
      0, 2,  14, 4,  6,  8,  10, 12, /**/ 2,  14, 0,  4,  6,  8,  10, 12,  //
      0, 14, 2,  4,  6,  8,  10, 12, /**/ 14, 0,  2,  4,  6,  8,  10, 12,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 0,  14,  //
      0, 4,  6,  8,  10, 12, 2,  14, /**/ 4,  6,  8,  10, 12, 0,  2,  14,  //
      0, 2,  6,  8,  10, 12, 4,  14, /**/ 2,  6,  8,  10, 12, 0,  4,  14,  //
      0, 6,  8,  10, 12, 2,  4,  14, /**/ 6,  8,  10, 12, 0,  2,  4,  14,  //
      0, 2,  4,  8,  10, 12, 6,  14, /**/ 2,  4,  8,  10, 12, 0,  6,  14,  //
      0, 4,  8,  10, 12, 2,  6,  14, /**/ 4,  8,  10, 12, 0,  2,  6,  14,  //
      0, 2,  8,  10, 12, 4,  6,  14, /**/ 2,  8,  10, 12, 0,  4,  6,  14,  //
      0, 8,  10, 12, 2,  4,  6,  14, /**/ 8,  10, 12, 0,  2,  4,  6,  14,  //
      0, 2,  4,  6,  10, 12, 8,  14, /**/ 2,  4,  6,  10, 12, 0,  8,  14,  //
      0, 4,  6,  10, 12, 2,  8,  14, /**/ 4,  6,  10, 12, 0,  2,  8,  14,  //
      0, 2,  6,  10, 12, 4,  8,  14, /**/ 2,  6,  10, 12, 0,  4,  8,  14,  //
      0, 6,  10, 12, 2,  4,  8,  14, /**/ 6,  10, 12, 0,  2,  4,  8,  14,  //
      0, 2,  4,  10, 12, 6,  8,  14, /**/ 2,  4,  10, 12, 0,  6,  8,  14,  //
      0, 4,  10, 12, 2,  6,  8,  14, /**/ 4,  10, 12, 0,  2,  6,  8,  14,  //
      0, 2,  10, 12, 4,  6,  8,  14, /**/ 2,  10, 12, 0,  4,  6,  8,  14,  //
      0, 10, 12, 2,  4,  6,  8,  14, /**/ 10, 12, 0,  2,  4,  6,  8,  14,  //
      0, 2,  4,  6,  8,  12, 10, 14, /**/ 2,  4,  6,  8,  12, 0,  10, 14,  //
      0, 4,  6,  8,  12, 2,  10, 14, /**/ 4,  6,  8,  12, 0,  2,  10, 14,  //
      0, 2,  6,  8,  12, 4,  10, 14, /**/ 2,  6,  8,  12, 0,  4,  10, 14,  //
      0, 6,  8,  12, 2,  4,  10, 14, /**/ 6,  8,  12, 0,  2,  4,  10, 14,  //
      0, 2,  4,  8,  12, 6,  10, 14, /**/ 2,  4,  8,  12, 0,  6,  10, 14,  //
      0, 4,  8,  12, 2,  6,  10, 14, /**/ 4,  8,  12, 0,  2,  6,  10, 14,  //
      0, 2,  8,  12, 4,  6,  10, 14, /**/ 2,  8,  12, 0,  4,  6,  10, 14,  //
      0, 8,  12, 2,  4,  6,  10, 14, /**/ 8,  12, 0,  2,  4,  6,  10, 14,  //
      0, 2,  4,  6,  12, 8,  10, 14, /**/ 2,  4,  6,  12, 0,  8,  10, 14,  //
      0, 4,  6,  12, 2,  8,  10, 14, /**/ 4,  6,  12, 0,  2,  8,  10, 14,  //
      0, 2,  6,  12, 4,  8,  10, 14, /**/ 2,  6,  12, 0,  4,  8,  10, 14,  //
      0, 6,  12, 2,  4,  8,  10, 14, /**/ 6,  12, 0,  2,  4,  8,  10, 14,  //
      0, 2,  4,  12, 6,  8,  10, 14, /**/ 2,  4,  12, 0,  6,  8,  10, 14,  //
      0, 4,  12, 2,  6,  8,  10, 14, /**/ 4,  12, 0,  2,  6,  8,  10, 14,  //
      0, 2,  12, 4,  6,  8,  10, 14, /**/ 2,  12, 0,  4,  6,  8,  10, 14,  //
      0, 12, 2,  4,  6,  8,  10, 14, /**/ 12, 0,  2,  4,  6,  8,  10, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 0,  12, 14,  //
      0, 4,  6,  8,  10, 2,  12, 14, /**/ 4,  6,  8,  10, 0,  2,  12, 14,  //
      0, 2,  6,  8,  10, 4,  12, 14, /**/ 2,  6,  8,  10, 0,  4,  12, 14,  //
      0, 6,  8,  10, 2,  4,  12, 14, /**/ 6,  8,  10, 0,  2,  4,  12, 14,  //
      0, 2,  4,  8,  10, 6,  12, 14, /**/ 2,  4,  8,  10, 0,  6,  12, 14,  //
      0, 4,  8,  10, 2,  6,  12, 14, /**/ 4,  8,  10, 0,  2,  6,  12, 14,  //
      0, 2,  8,  10, 4,  6,  12, 14, /**/ 2,  8,  10, 0,  4,  6,  12, 14,  //
      0, 8,  10, 2,  4,  6,  12, 14, /**/ 8,  10, 0,  2,  4,  6,  12, 14,  //
      0, 2,  4,  6,  10, 8,  12, 14, /**/ 2,  4,  6,  10, 0,  8,  12, 14,  //
      0, 4,  6,  10, 2,  8,  12, 14, /**/ 4,  6,  10, 0,  2,  8,  12, 14,  //
      0, 2,  6,  10, 4,  8,  12, 14, /**/ 2,  6,  10, 0,  4,  8,  12, 14,  //
      0, 6,  10, 2,  4,  8,  12, 14, /**/ 6,  10, 0,  2,  4,  8,  12, 14,  //
      0, 2,  4,  10, 6,  8,  12, 14, /**/ 2,  4,  10, 0,  6,  8,  12, 14,  //
      0, 4,  10, 2,  6,  8,  12, 14, /**/ 4,  10, 0,  2,  6,  8,  12, 14,  //
      0, 2,  10, 4,  6,  8,  12, 14, /**/ 2,  10, 0,  4,  6,  8,  12, 14,  //
      0, 10, 2,  4,  6,  8,  12, 14, /**/ 10, 0,  2,  4,  6,  8,  12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  0,  10, 12, 14,  //
      0, 4,  6,  8,  2,  10, 12, 14, /**/ 4,  6,  8,  0,  2,  10, 12, 14,  //
      0, 2,  6,  8,  4,  10, 12, 14, /**/ 2,  6,  8,  0,  4,  10, 12, 14,  //
      0, 6,  8,  2,  4,  10, 12, 14, /**/ 6,  8,  0,  2,  4,  10, 12, 14,  //
      0, 2,  4,  8,  6,  10, 12, 14, /**/ 2,  4,  8,  0,  6,  10, 12, 14,  //
      0, 4,  8,  2,  6,  10, 12, 14, /**/ 4,  8,  0,  2,  6,  10, 12, 14,  //
      0, 2,  8,  4,  6,  10, 12, 14, /**/ 2,  8,  0,  4,  6,  10, 12, 14,  //
      0, 8,  2,  4,  6,  10, 12, 14, /**/ 8,  0,  2,  4,  6,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  0,  8,  10, 12, 14,  //
      0, 4,  6,  2,  8,  10, 12, 14, /**/ 4,  6,  0,  2,  8,  10, 12, 14,  //
      0, 2,  6,  4,  8,  10, 12, 14, /**/ 2,  6,  0,  4,  8,  10, 12, 14,  //
      0, 6,  2,  4,  8,  10, 12, 14, /**/ 6,  0,  2,  4,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  0,  6,  8,  10, 12, 14,  //
      0, 4,  2,  6,  8,  10, 12, 14, /**/ 4,  0,  2,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  0,  4,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 0,  2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4), HWY_IF_LE128(T, N)>
HWY_INLINE Vec128<T, N> IndicesFromBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[256] = {
      // PrintCompress32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      4,  5,  6,  7,  0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      8,  9,  10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15,  //
      0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,  //
      0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  8,  9,  10, 11,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,   //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,   //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,   //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4), HWY_IF_LE128(T, N)>
HWY_INLINE Vec128<T, N> IndicesFromNotBits(Simd<T, N, 0> d,
                                           uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[256] = {
      // PrintCompressNot32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
      14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
      12, 13, 14, 15, 8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8), HWY_IF_LE128(T, N)>
HWY_INLINE Vec128<T, N> IndicesFromBits(Simd<T, N, 0> d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[64] = {
      // PrintCompress64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8), HWY_IF_LE128(T, N)>
HWY_INLINE Vec128<T, N> IndicesFromNotBits(Simd<T, N, 0> d,
                                           uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t u8_indices[64] = {
      // PrintCompressNot64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v, uint64_t mask_bits) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;

  HWY_DASSERT(mask_bits < (1ull << N));
  const auto indices = BitCast(du, detail::IndicesFromBits(d, mask_bits));
  return BitCast(d, TableLookupBytes(BitCast(du, v), indices));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CompressNotBits(Vec128<T, N> v, uint64_t mask_bits) {
  const Simd<T, N, 0> d;
  const RebindToUnsigned<decltype(d)> du;

  HWY_DASSERT(mask_bits < (1ull << N));
  const auto indices = BitCast(du, detail::IndicesFromNotBits(d, mask_bits));
  return BitCast(d, TableLookupBytes(BitCast(du, v), indices));
}

}  // namespace detail

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> Compress(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 1 and mask[0] = 0, then swap both halves, else keep.
  const Full128<T> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskL, maskH);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case
template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  return detail::CompressBits(v, detail::BitsFromMask(mask));
}

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec128<T> CompressNot(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 0 and mask[0] = 1, then swap both halves, else keep.
  const Full128<T> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskH, maskL);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case
template <typename T, size_t N, HWY_IF_NOT_LANE_SIZE(T, 8)>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  // For partial vectors, we cannot pull the Not() into the table because
  // BitsFromMask clears the upper bits.
  if (N < 16 / sizeof(T)) {
    return detail::CompressBits(v, detail::BitsFromMask(Not(mask)));
  }
  return detail::CompressNotBits(v, detail::BitsFromMask(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v,
                                  const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::CompressBits(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <typename T, size_t N>
HWY_API size_t CompressStore(Vec128<T, N> v, Mask128<T, N> m, Simd<T, N, 0> d,
                             T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = detail::BitsFromMask(m);
  HWY_DASSERT(mask_bits < (1ull << N));
  const size_t count = PopCount(mask_bits);

  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  const auto indices = BitCast(du, detail::IndicesFromBits(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif

  return count;
}

template <typename T, size_t N>
HWY_API size_t CompressBlendedStore(Vec128<T, N> v, Mask128<T, N> m,
                                    Simd<T, N, 0> d,
                                    T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = detail::BitsFromMask(m);
  HWY_DASSERT(mask_bits < (1ull << N));
  const size_t count = PopCount(mask_bits);

  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  const auto indices = BitCast(du, detail::IndicesFromBits(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  BlendedStore(compressed, FirstN(d, count), d, unaligned);
  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

template <typename T, size_t N>
HWY_API size_t CompressBitsStore(Vec128<T, N> v,
                                 const uint8_t* HWY_RESTRICT bits,
                                 Simd<T, N, 0> d, T* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }
  const size_t count = PopCount(mask_bits);

  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  const auto indices = BitCast(du, detail::IndicesFromBits(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);

  // Workaround for MSAN not marking output as initialized (b/233326619)
#if HWY_IS_MSAN
  __msan_unpoison(unaligned, count * sizeof(T));
#endif
  return count;
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ StoreInterleaved2/3/4

// HWY_NATIVE_LOAD_STORE_INTERLEAVED not set, hence defined in
// generic_ops-inl.h.

// ------------------------------ Reductions

namespace detail {

// N=1 for any T: no-op
template <typename T>
HWY_INLINE Vec128<T, 1> SumOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MinOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MaxOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}

// u32/i32/f32:

// N=2
template <typename T>
HWY_INLINE Vec128<T, 2> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return v10 + Shuffle2301(v10);
}
template <typename T>
HWY_INLINE Vec128<T, 2> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Min(v10, Shuffle2301(v10));
}
template <typename T>
HWY_INLINE Vec128<T, 2> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Max(v10, Shuffle2301(v10));
}

// N=4 (full)
template <typename T>
HWY_INLINE Vec128<T> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = v3210 + v1032;
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// u64/i64/f64:

// N=2 (full)
template <typename T>
HWY_INLINE Vec128<T> SumOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

// u16/i16
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_GE32(T, N)>
HWY_API Vec128<T, N> MinOfLanes(hwy::SizeTag<2> /* tag */, Vec128<T, N> v) {
  const Repartition<int32_t, Simd<T, N, 0>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Simd<T, N, 0>(), Or(min, ShiftLeft<16>(min)));
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2), HWY_IF_GE32(T, N)>
HWY_API Vec128<T, N> MaxOfLanes(hwy::SizeTag<2> /* tag */, Vec128<T, N> v) {
  const Repartition<int32_t, Simd<T, N, 0>> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return BitCast(Simd<T, N, 0>(), Or(min, ShiftLeft<16>(min)));
}

}  // namespace detail

// Supported for u/i/f 32/64. Returns the same value in each lane.
template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::SumOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(Simd<T, N, 0> /* tag */, const Vec128<T, N> v) {
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}

// ------------------------------ Lt128

namespace detail {

// Returns vector-mask for Lt128. Also used by x86_256/x86_512.
template <class D, class V = VFromD<D>>
HWY_INLINE V Lt128Vec(const D d, const V a, const V b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  // Truth table of Eq and Lt for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL)
  //  0  0  0  0  |  0
  //  0  0  0  1  |  0
  //  0  0  1  0  |  1
  //  0  0  1  1  |  1
  //  0  1  0  0  |  0
  //  0  1  0  1  |  0
  //  0  1  1  0  |  1
  //  1  0  0  0  |  0
  //  1  0  0  1  |  1
  //  1  1  0  0  |  0
  const auto eqHL = Eq(a, b);
  const V ltHL = VecFromMask(d, Lt(a, b));
  const V ltLX = ShiftLeftLanes<1>(ltHL);
  const V vecHx = IfThenElse(eqHL, ltLX, ltHL);
  return InterleaveUpper(d, vecHx, vecHx);
}

// Returns vector-mask for Eq128. Also used by x86_256/x86_512.
template <class D, class V = VFromD<D>>
HWY_INLINE V Eq128Vec(const D d, const V a, const V b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  const auto eqHL = VecFromMask(d, Eq(a, b));
  const auto eqLH = Reverse2(d, eqHL);
  return And(eqHL, eqLH);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Lt128UpperVec(const D d, const V a, const V b) {
  // No specialization required for AVX-512: Mask <-> Vec is fast, and
  // copying mask bits to their neighbor seems infeasible.
  const V ltHL = VecFromMask(d, Lt(a, b));
  return InterleaveUpper(d, ltHL, ltHL);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Eq128UpperVec(const D d, const V a, const V b) {
  // No specialization required for AVX-512: Mask <-> Vec is fast, and
  // copying mask bits to their neighbor seems infeasible.
  const V eqHL = VecFromMask(d, Eq(a, b));
  return InterleaveUpper(d, eqHL, eqHL);
}

}  // namespace detail

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Lt128(D d, const V a, const V b) {
  return MaskFromVec(detail::Lt128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Eq128(D d, const V a, const V b) {
  return MaskFromVec(detail::Eq128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Lt128Upper(D d, const V a, const V b) {
  return MaskFromVec(detail::Lt128UpperVec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Eq128Upper(D d, const V a, const V b) {
  return MaskFromVec(detail::Eq128UpperVec(d, a, b));
}

// ------------------------------ Min128, Max128 (Lt128)

// Avoids the extra MaskFromVec in Lt128.
template <class D, class V = VFromD<D>>
HWY_API V Min128(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128Vec(d, a, b), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Max128(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128Vec(d, b, a), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Min128Upper(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128UpperVec(d, a, b), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Max128Upper(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128UpperVec(d, b, a), a, b);
}

// ================================================== Operator wrapper

// These apply to all x86_*-inl.h because there are no restrictions on V.

template <class V>
HWY_API V Add(V a, V b) {
  return a + b;
}
template <class V>
HWY_API V Sub(V a, V b) {
  return a - b;
}

template <class V>
HWY_API V Mul(V a, V b) {
  return a * b;
}
template <class V>
HWY_API V Div(V a, V b) {
  return a / b;
}

template <class V>
V Shl(V a, V b) {
  return a << b;
}
template <class V>
V Shr(V a, V b) {
  return a >> b;
}

template <class V>
HWY_API auto Eq(V a, V b) -> decltype(a == b) {
  return a == b;
}
template <class V>
HWY_API auto Ne(V a, V b) -> decltype(a == b) {
  return a != b;
}
template <class V>
HWY_API auto Lt(V a, V b) -> decltype(a == b) {
  return a < b;
}

template <class V>
HWY_API auto Gt(V a, V b) -> decltype(a == b) {
  return a > b;
}
template <class V>
HWY_API auto Ge(V a, V b) -> decltype(a == b) {
  return a >= b;
}

template <class V>
HWY_API auto Le(V a, V b) -> decltype(a == b) {
  return a <= b;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
