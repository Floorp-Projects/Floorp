// Copyright 2020 Google LLC
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

// SIMD operations are implemented as overloaded functions selected using a tag
// type D := Simd<T, N>. T is the lane type, N an opaque integer for internal
// use only. Users create D via aliases ScalableTag<T>() (a full vector),
// CappedTag<T, kLimit> or FixedTag<T, kNumLanes>. The actual number of lanes
// (always a power of two) is Lanes(D()).
template <typename Lane, size_t N>
struct Simd {
  constexpr Simd() = default;
  using T = Lane;
  static_assert((N & (N - 1)) == 0 && N != 0, "N must be a power of two");

  // Widening/narrowing ops change the number of lanes and/or their type.
  // To initialize such vectors, we need the corresponding tag types:

  // PromoteTo/DemoteTo() with another lane type, but same number of lanes.
  template <typename NewLane>
  using Rebind = Simd<NewLane, N>;

  // MulEven() with another lane type, but same total size.
  // Round up to correctly handle scalars with N=1.
  template <typename NewLane>
  using Repartition =
      Simd<NewLane, (N * sizeof(Lane) + sizeof(NewLane) - 1) / sizeof(NewLane)>;

  // LowerHalf() with the same lane type, but half the lanes.
  // Round up to correctly handle scalars with N=1.
  using Half = Simd<T, (N + 1) / 2>;

  // Combine() with the same lane type, but twice the lanes.
  using Twice = Simd<T, 2 * N>;
};

namespace detail {

// Given N from HWY_LANES(T), returns N for use in Simd<T, N> to describe:
// - a full vector (pow2 = 0);
// - 2,4,8 regs on RVV, otherwise a full vector (pow2 [1,3]);
// - a fraction of a register from 1/8 to 1/2 (pow2 [-3,-1]).
constexpr size_t ScaleByPower(size_t N, int pow2) {
#if HWY_TARGET == HWY_RVV
  // For fractions, if N == 1 ensure we still return at least one lane.
  return pow2 >= 0 ? (N << pow2) : HWY_MAX(1, (N >> (-pow2)));
#else
  // If pow2 > 0, replace it with 0 (there is nothing wider than a full vector).
  return HWY_MAX(1, N >> HWY_MAX(-pow2, 0));
#endif
}

// Struct wrappers enable validation of arguments via static_assert.
template <typename T, int kPow2>
struct ScalableTagChecker {
  static_assert(-3 <= kPow2 && kPow2 <= 3, "Fraction must be 1/8 to 8");
  using type = Simd<T, ScaleByPower(HWY_LANES(T), kPow2)>;
};

template <typename T, size_t kLimit>
struct CappedTagChecker {
  static_assert(kLimit != 0, "Does not make sense to have zero lanes");
  using type = Simd<T, HWY_MIN(kLimit, HWY_LANES(T))>;
};

template <typename T, size_t kNumLanes>
struct FixedTagChecker {
  static_assert(kNumLanes != 0, "Does not make sense to have zero lanes");
  static_assert(kNumLanes * sizeof(T) <= HWY_MAX_BYTES, "Too many lanes");
#if HWY_TARGET == HWY_SCALAR
  // HWY_MAX_BYTES would still allow uint8x8, which is not supported.
  static_assert(kNumLanes == 1, "Scalar only supports one lane");
#endif
  using type = Simd<T, kNumLanes>;
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
// chunks of say 256 DC components followed by 256 AC1 and finally 256 AC63;
// this would enable vector-length-agnostic loops using ScalableTag).
template <typename T, size_t kLimit>
using CappedTag = typename detail::CappedTagChecker<T, kLimit>::type;

// Alias for a tag describing a vector with *exactly* kNumLanes active lanes,
// even on targets with scalable vectors. All targets except HWY_SCALAR support
// up to 16 / sizeof(T). Other targets may allow larger kNumLanes, but relying
// on that is non-portable and discouraged.
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

// Descriptor for the same lane type as D, but twice the lanes.
template <class D>
using Twice = typename D::Twice;

// Same as base.h macros but with a Simd<T, N> argument instead of T.
#define HWY_IF_UNSIGNED_D(D) HWY_IF_UNSIGNED(TFromD<D>)
#define HWY_IF_SIGNED_D(D) HWY_IF_SIGNED(TFromD<D>)
#define HWY_IF_FLOAT_D(D) HWY_IF_FLOAT(TFromD<D>)
#define HWY_IF_NOT_FLOAT_D(D) HWY_IF_NOT_FLOAT(TFromD<D>)
#define HWY_IF_LANE_SIZE_D(D, bytes) HWY_IF_LANE_SIZE(TFromD<D>, bytes)
#define HWY_IF_NOT_LANE_SIZE_D(D, bytes) HWY_IF_NOT_LANE_SIZE(TFromD<D>, bytes)

// Same, but with a vector argument.
#define HWY_IF_UNSIGNED_V(V) HWY_IF_UNSIGNED(TFromV<V>)
#define HWY_IF_SIGNED_V(V) HWY_IF_SIGNED(TFromV<V>)
#define HWY_IF_FLOAT_V(V) HWY_IF_FLOAT(TFromV<V>)
#define HWY_IF_LANE_SIZE_V(V, bytes) HWY_IF_LANE_SIZE(TFromV<V>, bytes)

// For implementing functions for a specific type.
// IsSame<...>() in template arguments is broken on MSVC2015.
#define HWY_IF_LANES_ARE(T, V) \
  EnableIf<IsSameT<T, TFromD<DFromV<V>>>::value>* = nullptr

// Compile-time-constant, (typically but not guaranteed) an upper bound on the
// number of lanes.
// Prefer instead using Lanes() and dynamic allocation, or Rebind, or
// `#if HWY_CAP_GE*`.
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED constexpr size_t MaxLanes(Simd<T, N>) {
  return N;
}

// Targets with non-constexpr Lanes define this themselves.
#if HWY_TARGET != HWY_RVV && HWY_TARGET != HWY_SVE2 && HWY_TARGET != HWY_SVE

// (Potentially) non-constant actual size of the vector at runtime, subject to
// the limit imposed by the Simd. Useful for advancing loop counters.
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED size_t Lanes(Simd<T, N>) {
  return N;
}

#endif

// NOTE: GCC generates incorrect code for vector arguments to non-inlined
// functions in two situations:
// - on Windows and GCC 10.3, passing by value crashes due to unaligned loads:
//   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54412.
// - on ARM64 and GCC 9.3.0 or 11.2.1, passing by const& causes many (but not
//   all) tests to fail.
//
// We therefore pass by const& only on GCC and (Windows or ARM64). This alias
// must be used for all vector/mask parameters of functions marked HWY_NOINLINE,
// and possibly also other functions that are not inlined.
#if HWY_COMPILER_GCC && !HWY_COMPILER_CLANG && \
    ((defined(_WIN32) || defined(_WIN64)) || HWY_ARCH_ARM64)
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
