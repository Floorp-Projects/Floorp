// Copyright 2021 Google LLC
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

// Per-target
#if defined(HIGHWAY_HWY_CONTRIB_SORT_TRAITS128_TOGGLE) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_TRAITS128_TOGGLE
#undef HIGHWAY_HWY_CONTRIB_SORT_TRAITS128_TOGGLE
#else
#define HIGHWAY_HWY_CONTRIB_SORT_TRAITS128_TOGGLE
#endif

#include "hwy/contrib/sort/vqsort.h"  // SortDescending
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

#if HWY_TARGET == HWY_SCALAR

struct OrderAscending128 {
  using Order = SortAscending;

  template <typename T>
  HWY_INLINE bool Compare1(const T* a, const T* b) {
    return (a[1] == b[1]) ? a[0] < b[0] : a[1] < b[1];
  }
};

struct OrderDescending128 {
  using Order = SortDescending;

  template <typename T>
  HWY_INLINE bool Compare1(const T* a, const T* b) {
    return (a[1] == b[1]) ? b[0] < a[0] : b[1] < a[1];
  }
};

template <class Order>
struct Traits128 : public Order {
  constexpr bool Is128() const { return true; }
  constexpr size_t LanesPerKey() const { return 2; }
};

#else

// Highway does not provide a lane type for 128-bit keys, so we use uint64_t
// along with an abstraction layer for single-lane vs. lane-pair, which is
// independent of the order.
struct Key128 {
  constexpr size_t LanesPerKey() const { return 2; }

  template <typename T>
  HWY_INLINE void Swap(T* a, T* b) const {
    const FixedTag<T, 2> d;
    const auto temp = LoadU(d, a);
    StoreU(LoadU(d, b), d, a);
    StoreU(temp, d, b);
  }

  template <class D>
  HWY_INLINE Vec<D> SetKey(D d, const TFromD<D>* key) const {
    return LoadDup128(d, key);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys(D d, Vec<D> v) const {
    return ReverseBlocks(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys2(D /* tag */, const Vec<D> v) const {
    return SwapAdjacentBlocks(v);
  }

  // Only called for 4 keys because we do not support >512-bit vectors.
  template <class D>
  HWY_INLINE Vec<D> ReverseKeys4(D d, const Vec<D> v) const {
    HWY_DASSERT(Lanes(d) <= 64 / sizeof(TFromD<D>));
    return ReverseKeys(d, v);
  }

  // Only called for 4 keys because we do not support >512-bit vectors.
  template <class D>
  HWY_INLINE Vec<D> OddEvenPairs(D d, const Vec<D> odd,
                                 const Vec<D> even) const {
    HWY_DASSERT(Lanes(d) <= 64 / sizeof(TFromD<D>));
    return ConcatUpperLower(d, odd, even);
  }

  template <class V>
  HWY_INLINE V OddEvenKeys(const V odd, const V even) const {
    return OddEvenBlocks(odd, even);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys8(D, Vec<D>) const {
    HWY_ASSERT(0);  // not supported: would require 1024-bit vectors
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys16(D, Vec<D>) const {
    HWY_ASSERT(0);  // not supported: would require 2048-bit vectors
  }

  // This is only called for 8/16 col networks (not supported).
  template <class D>
  HWY_INLINE Vec<D> SwapAdjacentPairs(D, Vec<D>) const {
    HWY_ASSERT(0);
  }

  // This is only called for 16 col networks (not supported).
  template <class D>
  HWY_INLINE Vec<D> SwapAdjacentQuads(D, Vec<D>) const {
    HWY_ASSERT(0);
  }

  // This is only called for 8 col networks (not supported).
  template <class D>
  HWY_INLINE Vec<D> OddEvenQuads(D, Vec<D>, Vec<D>) const {
    HWY_ASSERT(0);
  }
};

// Anything order-related depends on the key traits *and* the order (see
// FirstOfLanes). We cannot implement just one Compare function because Lt128
// only compiles if the lane type is u64. Thus we need either overloaded
// functions with a tag type, class specializations, or separate classes.
// We avoid overloaded functions because we want all functions to be callable
// from a SortTraits without per-function wrappers. Specializing would work, but
// we are anyway going to specialize at a higher level.
struct OrderAscending128 : public Key128 {
  using Order = SortAscending;

  template <typename T>
  HWY_INLINE bool Compare1(const T* a, const T* b) {
    return (a[1] == b[1]) ? a[0] < b[0] : a[1] < b[1];
  }

  template <class D>
  HWY_INLINE Mask<D> Compare(D d, Vec<D> a, Vec<D> b) const {
    return Lt128(d, a, b);
  }

  // Used by CompareTop
  template <class V>
  HWY_INLINE Mask<DFromV<V> > CompareLanes(V a, V b) const {
    return Lt(a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> First(D d, const Vec<D> a, const Vec<D> b) const {
    return Min128(d, a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> Last(D d, const Vec<D> a, const Vec<D> b) const {
    return Max128(d, a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> FirstOfLanes(D d, Vec<D> v,
                                 TFromD<D>* HWY_RESTRICT buf) const {
    const size_t N = Lanes(d);
    Store(v, d, buf);
    v = SetKey(d, buf + 0);  // result must be broadcasted
    for (size_t i = LanesPerKey(); i < N; i += LanesPerKey()) {
      v = First(d, v, SetKey(d, buf + i));
    }
    return v;
  }

  template <class D>
  HWY_INLINE Vec<D> LastOfLanes(D d, Vec<D> v,
                                TFromD<D>* HWY_RESTRICT buf) const {
    const size_t N = Lanes(d);
    Store(v, d, buf);
    v = SetKey(d, buf + 0);  // result must be broadcasted
    for (size_t i = LanesPerKey(); i < N; i += LanesPerKey()) {
      v = Last(d, v, SetKey(d, buf + i));
    }
    return v;
  }

  // Same as for regular lanes because 128-bit lanes are u64.
  template <class D>
  HWY_INLINE Vec<D> FirstValue(D d) const {
    return Set(d, hwy::LowestValue<TFromD<D> >());
  }

  template <class D>
  HWY_INLINE Vec<D> LastValue(D d) const {
    return Set(d, hwy::HighestValue<TFromD<D> >());
  }
};

struct OrderDescending128 : public Key128 {
  using Order = SortDescending;

  template <typename T>
  HWY_INLINE bool Compare1(const T* a, const T* b) {
    return (a[1] == b[1]) ? b[0] < a[0] : b[1] < a[1];
  }

  template <class D>
  HWY_INLINE Mask<D> Compare(D d, Vec<D> a, Vec<D> b) const {
    return Lt128(d, b, a);
  }

  // Used by CompareTop
  template <class V>
  HWY_INLINE Mask<DFromV<V> > CompareLanes(V a, V b) const {
    return Lt(b, a);
  }

  template <class D>
  HWY_INLINE Vec<D> First(D d, const Vec<D> a, const Vec<D> b) const {
    return Max128(d, a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> Last(D d, const Vec<D> a, const Vec<D> b) const {
    return Min128(d, a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> FirstOfLanes(D d, Vec<D> v,
                                 TFromD<D>* HWY_RESTRICT buf) const {
    const size_t N = Lanes(d);
    Store(v, d, buf);
    v = SetKey(d, buf + 0);  // result must be broadcasted
    for (size_t i = LanesPerKey(); i < N; i += LanesPerKey()) {
      v = First(d, v, SetKey(d, buf + i));
    }
    return v;
  }

  template <class D>
  HWY_INLINE Vec<D> LastOfLanes(D d, Vec<D> v,
                                TFromD<D>* HWY_RESTRICT buf) const {
    const size_t N = Lanes(d);
    Store(v, d, buf);
    v = SetKey(d, buf + 0);  // result must be broadcasted
    for (size_t i = LanesPerKey(); i < N; i += LanesPerKey()) {
      v = Last(d, v, SetKey(d, buf + i));
    }
    return v;
  }

  // Same as for regular lanes because 128-bit lanes are u64.
  template <class D>
  HWY_INLINE Vec<D> FirstValue(D d) const {
    return Set(d, hwy::HighestValue<TFromD<D> >());
  }

  template <class D>
  HWY_INLINE Vec<D> LastValue(D d) const {
    return Set(d, hwy::LowestValue<TFromD<D> >());
  }
};

// Shared code that depends on Order.
template <class Base>
class Traits128 : public Base {
#if HWY_TARGET <= HWY_AVX2
  // Returns vector with only the top u64 lane valid. Useful when the next step
  // is to replicate the mask anyway.
  template <class D>
  HWY_INLINE HWY_MAYBE_UNUSED Vec<D> CompareTop(D d, Vec<D> a, Vec<D> b) const {
    const Base* base = static_cast<const Base*>(this);
    const Vec<D> eqHL = VecFromMask(d, Eq(a, b));
    const Vec<D> ltHL = VecFromMask(d, base->CompareLanes(a, b));
    const Vec<D> ltLX = ShiftLeftLanes<1>(ltHL);
    return OrAnd(ltHL, eqHL, ltLX);
  }

  // We want to swap 2 u128, i.e. 4 u64 lanes, based on the 0 or FF..FF mask in
  // the most-significant of those lanes (the result of CompareTop), so
  // replicate it 4x. Only called for >= 256-bit vectors.
  template <class V>
  HWY_INLINE V ReplicateTop4x(V v) const {
#if HWY_TARGET <= HWY_AVX3
    return V{_mm512_permutex_epi64(v.raw, _MM_SHUFFLE(3, 3, 3, 3))};
#else  // AVX2
    return V{_mm256_permute4x64_epi64(v.raw, _MM_SHUFFLE(3, 3, 3, 3))};
#endif
  }
#endif

 public:
  constexpr bool Is128() const { return true; }

  template <class D>
  HWY_INLINE void Sort2(D d, Vec<D>& a, Vec<D>& b) const {
    const Base* base = static_cast<const Base*>(this);

    const Vec<D> a_copy = a;
    const auto lt = base->Compare(d, a, b);
    a = IfThenElse(lt, a, b);
    b = IfThenElse(lt, b, a_copy);
  }

  // Conditionally swaps even-numbered lanes with their odd-numbered neighbor.
  template <class D>
  HWY_INLINE Vec<D> SortPairsDistance1(D d, Vec<D> v) const {
    const Base* base = static_cast<const Base*>(this);
    Vec<D> swapped = base->ReverseKeys2(d, v);

#if HWY_TARGET <= HWY_AVX2
    const Vec<D> select = ReplicateTop4x(CompareTop(d, v, swapped));
    return IfVecThenElse(select, swapped, v);
#else
    Sort2(d, v, swapped);
    return base->OddEvenKeys(swapped, v);
#endif
  }

  // Swaps with the vector formed by reversing contiguous groups of 4 keys.
  template <class D>
  HWY_INLINE Vec<D> SortPairsReverse4(D d, Vec<D> v) const {
    const Base* base = static_cast<const Base*>(this);
    Vec<D> swapped = base->ReverseKeys4(d, v);

    // Only specialize for AVX3 because this requires 512-bit vectors.
#if HWY_TARGET <= HWY_AVX3
    const Vec512<uint64_t> outHx = CompareTop(d, v, swapped);
    // Similar to ReplicateTop4x, we want to gang together 2 comparison results
    // (4 lanes). They are not contiguous, so use permute to replicate 4x.
    alignas(64) uint64_t kIndices[8] = {7, 7, 5, 5, 5, 5, 7, 7};
    const Vec512<uint64_t> select =
        TableLookupLanes(outHx, SetTableIndices(d, kIndices));
    return IfVecThenElse(select, swapped, v);
#else
    Sort2(d, v, swapped);
    return base->OddEvenPairs(d, swapped, v);
#endif
  }

  // Conditionally swaps lane 0 with 4, 1 with 5 etc.
  template <class D>
  HWY_INLINE Vec<D> SortPairsDistance4(D, Vec<D>) const {
    // Only used by Merge16, which would require 2048 bit vectors (unsupported).
    HWY_ASSERT(0);
  }
};

#endif  // HWY_TARGET != HWY_SCALAR

}  // namespace detail
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_SORT_TRAITS128_TOGGLE
