// Copyright 2020 Google LLC
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

// Per-target definitions shared by ops/*.h and user code.

#include <cmath>

#include "hwy/base.h"

// Separate header because foreach_target.h re-enables its include guard.
#include "hwy/ops/set_macros-inl.h"

// Relies on the external include guard in highway.h.
HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// Highway operations are implemented as overloaded functions selected using an
// internal-only tag type D := Simd<T, N, kPow2>. T is the lane type. kPow2 is a
// shift count applied to scalable vectors. Instead of referring to Simd<>
// directly, users create D via aliases ScalableTag<T[, kPow2]>() (defaults to a
// full vector, or fractions/groups if the argument is negative/positive),
// CappedTag<T, kLimit> or FixedTag<T, kNumLanes>. The actual number of lanes is
// Lanes(D()), a power of two. For scalable vectors, N is either HWY_LANES or a
// cap. For constexpr-size vectors, N is the actual number of lanes. This
// ensures Half<Full512<T>> is the same type as Full256<T>, as required by x86.
template <typename Lane, size_t N, int kPow2>
struct Simd {
  constexpr Simd() = default;
  using T = Lane;
  static_assert((N & (N - 1)) == 0 && N != 0, "N must be a power of two");

  // Only for use by MaxLanes, required by MSVC. Cannot be enum because GCC
  // warns when using enums and non-enums in the same expression. Cannot be
  // static constexpr function (another MSVC limitation).
  static constexpr size_t kPrivateN = N;
  static constexpr int kPrivatePow2 = kPow2;

  template <typename NewT>
  static constexpr size_t NewN() {
    // Round up to correctly handle scalars with N=1.
    return (N * sizeof(T) + sizeof(NewT) - 1) / sizeof(NewT);
  }

#if HWY_HAVE_SCALABLE
  template <typename NewT>
  static constexpr int Pow2Ratio() {
    return (sizeof(NewT) > sizeof(T))
               ? static_cast<int>(CeilLog2(sizeof(NewT) / sizeof(T)))
               : -static_cast<int>(CeilLog2(sizeof(T) / sizeof(NewT)));
  }
#endif

  // Widening/narrowing ops change the number of lanes and/or their type.
  // To initialize such vectors, we need the corresponding tag types:

// PromoteTo/DemoteTo() with another lane type, but same number of lanes.
#if HWY_HAVE_SCALABLE
  template <typename NewT>
  using Rebind = Simd<NewT, N, kPow2 + Pow2Ratio<NewT>()>;
#else
  template <typename NewT>
  using Rebind = Simd<NewT, N, kPow2>;
#endif

  // Change lane type while keeping the same vector size, e.g. for MulEven.
  template <typename NewT>
  using Repartition = Simd<NewT, NewN<NewT>(), kPow2>;

// Half the lanes while keeping the same lane type, e.g. for LowerHalf.
// Round up to correctly handle scalars with N=1.
#if HWY_HAVE_SCALABLE
  // Reducing the cap (N) is required for SVE - if N is the limiter for f32xN,
  // then we expect Half<Rebind<u16>> to have N/2 lanes (rounded up).
  using Half = Simd<T, (N + 1) / 2, kPow2 - 1>;
#else
  using Half = Simd<T, (N + 1) / 2, kPow2>;
#endif

// Twice the lanes while keeping the same lane type, e.g. for Combine.
#if HWY_HAVE_SCALABLE
  using Twice = Simd<T, 2 * N, kPow2 + 1>;
#else
  using Twice = Simd<T, 2 * N, kPow2>;
#endif
};

namespace detail {

template <typename T, size_t N, int kPow2>
constexpr bool IsFull(Simd<T, N, kPow2> /* d */) {
  return N == HWY_LANES(T) && kPow2 == 0;
}

// Returns the number of lanes (possibly zero) after applying a shift:
// - 0: no change;
// - [1,3]: a group of 2,4,8 [fractional] vectors;
// - [-3,-1]: a fraction of a vector from 1/8 to 1/2.
constexpr size_t ScaleByPower(size_t N, int pow2) {
#if HWY_TARGET == HWY_RVV
  return pow2 >= 0 ? (N << pow2) : (N >> (-pow2));
#else
  return pow2 >= 0 ? N : (N >> (-pow2));
#endif
}

// Struct wrappers enable validation of arguments via static_assert.
template <typename T, int kPow2>
struct ScalableTagChecker {
  static_assert(-3 <= kPow2 && kPow2 <= 3, "Fraction must be 1/8 to 8");
#if HWY_TARGET == HWY_RVV
  // Only RVV supports register groups.
  using type = Simd<T, HWY_LANES(T), kPow2>;
#elif HWY_HAVE_SCALABLE
  // For SVE[2], only allow full or fractions.
  using type = Simd<T, HWY_LANES(T), HWY_MIN(kPow2, 0)>;
#elif HWY_TARGET == HWY_SCALAR
  using type = Simd<T, /*N=*/1, 0>;
#else
  // Only allow full or fractions.
  using type = Simd<T, ScaleByPower(HWY_LANES(T), HWY_MIN(kPow2, 0)), 0>;
#endif
};

template <typename T, size_t kLimit>
struct CappedTagChecker {
  static_assert(kLimit != 0, "Does not make sense to have zero lanes");
  // Safely handle non-power-of-two inputs by rounding down, which is allowed by
  // CappedTag. Otherwise, Simd<T, 3, 0> would static_assert.
  static constexpr size_t kLimitPow2 = size_t{1} << hwy::FloorLog2(kLimit);
  using type = Simd<T, HWY_MIN(kLimitPow2, HWY_LANES(T)), 0>;
};

template <typename T, size_t kNumLanes>
struct FixedTagChecker {
  static_assert(kNumLanes != 0, "Does not make sense to have zero lanes");
  static_assert(kNumLanes <= HWY_LANES(T), "Too many lanes");
  using type = Simd<T, kNumLanes, 0>;
};

}  // namespace detail

// Alias for a tag describing a full vector (kPow2 == 0: the most common usage,
// e.g. 1D loops where the application does not care about the vector size) or a
// fraction/multiple of one. Multiples are the same as full vectors for all
// targets except RVV. Fractions (kPow2 < 0) are useful as the argument/return
// value of type promotion and demotion.
template <typename T, int kPow2 = 0>
using ScalableTag = typename detail::ScalableTagChecker<T, kPow2>::type;

// Alias for a tag describing a vector with *up to* kLimit active lanes, even on
// targets with scalable vectors and HWY_SCALAR. The runtime lane count
// `Lanes(tag)` may be less than kLimit, and is 1 on HWY_SCALAR. This alias is
// typically used for 1D loops with a relatively low application-defined upper
// bound, e.g. for 8x8 DCTs. However, it is better if data structures are
// designed to be vector-length-agnostic (e.g. a hybrid SoA where there are
// chunks of `M >= MaxLanes(d)` DC components followed by M AC1, .., and M AC63;
// this would enable vector-length-agnostic loops using ScalableTag).
template <typename T, size_t kLimit>
using CappedTag = typename detail::CappedTagChecker<T, kLimit>::type;

// Alias for a tag describing a vector with *exactly* kNumLanes active lanes,
// even on targets with scalable vectors. Requires `kNumLanes` to be a power of
// two not exceeding `HWY_LANES(T)`.
//
// NOTE: if the application does not need to support HWY_SCALAR (+), use this
// instead of CappedTag to emphasize that there will be exactly kNumLanes lanes.
// This is useful for data structures that rely on exactly 128-bit SIMD, but
// these are discouraged because they cannot benefit from wider vectors.
// Instead, applications would ideally define a larger problem size and loop
// over it with the (unknown size) vectors from ScalableTag.
//
// + e.g. if the baseline is known to support SIMD, or the application requires
//   ops such as TableLookupBytes not supported by HWY_SCALAR.
template <typename T, size_t kNumLanes>
using FixedTag = typename detail::FixedTagChecker<T, kNumLanes>::type;

template <class D>
using TFromD = typename D::T;

// Tag for the same number of lanes as D, but with the LaneType T.
template <class T, class D>
using Rebind = typename D::template Rebind<T>;

template <class D>
using RebindToSigned = Rebind<MakeSigned<TFromD<D>>, D>;
template <class D>
using RebindToUnsigned = Rebind<MakeUnsigned<TFromD<D>>, D>;
template <class D>
using RebindToFloat = Rebind<MakeFloat<TFromD<D>>, D>;

// Tag for the same total size as D, but with the LaneType T.
template <class T, class D>
using Repartition = typename D::template Repartition<T>;

template <class D>
using RepartitionToWide = Repartition<MakeWide<TFromD<D>>, D>;
template <class D>
using RepartitionToNarrow = Repartition<MakeNarrow<TFromD<D>>, D>;

// Tag for the same lane type as D, but half the lanes.
template <class D>
using Half = typename D::Half;

// Tag for the same lane type as D, but twice the lanes.
template <class D>
using Twice = typename D::Twice;

template <typename T>
using Full32 = Simd<T, 4 / sizeof(T), 0>;

template <typename T>
using Full64 = Simd<T, 8 / sizeof(T), 0>;

template <typename T>
using Full128 = Simd<T, 16 / sizeof(T), 0>;

// Same as base.h macros but with a Simd<T, N, kPow2> argument instead of T.
#define HWY_IF_UNSIGNED_D(D) HWY_IF_UNSIGNED(TFromD<D>)
#define HWY_IF_SIGNED_D(D) HWY_IF_SIGNED(TFromD<D>)
#define HWY_IF_FLOAT_D(D) HWY_IF_FLOAT(TFromD<D>)
#define HWY_IF_NOT_FLOAT_D(D) HWY_IF_NOT_FLOAT(TFromD<D>)
#define HWY_IF_LANE_SIZE_D(D, bytes) HWY_IF_LANE_SIZE(TFromD<D>, bytes)
#define HWY_IF_NOT_LANE_SIZE_D(D, bytes) HWY_IF_NOT_LANE_SIZE(TFromD<D>, bytes)

// MSVC workaround: use PrivateN directly instead of MaxLanes.
#define HWY_IF_LT128_D(D) \
  hwy::EnableIf<D::kPrivateN * sizeof(TFromD<D>) < 16>* = nullptr
#define HWY_IF_GE128_D(D) \
  hwy::EnableIf<D::kPrivateN * sizeof(TFromD<D>) >= 16>* = nullptr

// Same, but with a vector argument. ops/*-inl.h define their own TFromV.
#define HWY_IF_UNSIGNED_V(V) HWY_IF_UNSIGNED(TFromV<V>)
#define HWY_IF_SIGNED_V(V) HWY_IF_SIGNED(TFromV<V>)
#define HWY_IF_FLOAT_V(V) HWY_IF_FLOAT(TFromV<V>)
#define HWY_IF_LANE_SIZE_V(V, bytes) HWY_IF_LANE_SIZE(TFromV<V>, bytes)
#define HWY_IF_NOT_LANE_SIZE_V(V, bytes) HWY_IF_NOT_LANE_SIZE(TFromV<V>, bytes)

template <class D>
HWY_INLINE HWY_MAYBE_UNUSED constexpr int Pow2(D /* d */) {
  return D::kPrivatePow2;
}

// MSVC requires the explicit <D>.
#define HWY_IF_POW2_GE(D, MIN) hwy::EnableIf<Pow2<D>(D()) >= (MIN)>* = nullptr

#if HWY_HAVE_SCALABLE

// Upper bound on the number of lanes. Intended for template arguments and
// reducing code size (e.g. for SSE4, we know at compile-time that vectors will
// not exceed 16 bytes). WARNING: this may be a loose bound, use Lanes() as the
// actual size for allocating storage. WARNING: MSVC might not be able to deduce
// arguments if this is used in EnableIf. See HWY_IF_LT128_D above.
template <class D>
HWY_INLINE HWY_MAYBE_UNUSED constexpr size_t MaxLanes(D) {
  return detail::ScaleByPower(HWY_MIN(D::kPrivateN, HWY_LANES(TFromD<D>)),
                              D::kPrivatePow2);
}

#else
// Workaround for MSVC 2017: T,N,kPow2 argument deduction fails, so returning N
// is not an option, nor does a member function work.
template <class D>
HWY_INLINE HWY_MAYBE_UNUSED constexpr size_t MaxLanes(D) {
  return D::kPrivateN;
}

// (Potentially) non-constant actual size of the vector at runtime, subject to
// the limit imposed by the Simd. Useful for advancing loop counters.
// Targets with scalable vectors define this themselves.
template <typename T, size_t N, int kPow2>
HWY_INLINE HWY_MAYBE_UNUSED size_t Lanes(Simd<T, N, kPow2>) {
  return N;
}

#endif  // !HWY_HAVE_SCALABLE

// NOTE: GCC generates incorrect code for vector arguments to non-inlined
// functions in two situations:
// - on Windows and GCC 10.3, passing by value crashes due to unaligned loads:
//   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54412.
// - on ARM64 and GCC 9.3.0 or 11.2.1, passing by value causes many (but not
//   all) tests to fail.
//
// We therefore pass by const& only on GCC and (Windows or ARM64). This alias
// must be used for all vector/mask parameters of functions marked HWY_NOINLINE,
// and possibly also other functions that are not inlined.
#if HWY_COMPILER_GCC_ACTUAL && (HWY_OS_WIN || HWY_ARCH_ARM_A64)
template <class V>
using VecArg = const V&;
#else
template <class V>
using VecArg = V;
#endif

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
