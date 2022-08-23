// Copyright 2021 Google LLC
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

// Per-target
#if defined(HIGHWAY_HWY_CONTRIB_SORT_TRAITS_TOGGLE) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_TRAITS_TOGGLE
#undef HIGHWAY_HWY_CONTRIB_SORT_TRAITS_TOGGLE
#else
#define HIGHWAY_HWY_CONTRIB_SORT_TRAITS_TOGGLE
#endif

#include <string>

#include "hwy/contrib/sort/shared-inl.h"  // SortConstants
#include "hwy/contrib/sort/vqsort.h"      // SortDescending
#include "hwy/highway.h"
#include "hwy/print.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

#if VQSORT_ENABLED || HWY_IDE

// Highway does not provide a lane type for 128-bit keys, so we use uint64_t
// along with an abstraction layer for single-lane vs. lane-pair, which is
// independent of the order.
template <typename T>
struct KeyLane {
  constexpr bool Is128() const { return false; }
  constexpr size_t LanesPerKey() const { return 1; }

  // What type bench_sort should allocate for generating inputs.
  using LaneType = T;
  // What type to pass to Sorter::operator().
  using KeyType = T;

  std::string KeyString() const {
    char string100[100];
    hwy::detail::TypeName(hwy::detail::MakeTypeInfo<KeyType>(), 1, string100);
    return string100;
  }

  // For HeapSort
  HWY_INLINE void Swap(T* a, T* b) const {
    const T temp = *a;
    *a = *b;
    *b = temp;
  }

  template <class V, class M>
  HWY_INLINE V CompressKeys(V keys, M mask) const {
    return CompressNot(keys, mask);
  }

  // Broadcasts one key into a vector
  template <class D>
  HWY_INLINE Vec<D> SetKey(D d, const T* key) const {
    return Set(d, *key);
  }

  template <class D>
  HWY_INLINE Mask<D> EqualKeys(D /*tag*/, Vec<D> a, Vec<D> b) const {
    return Eq(a, b);
  }

  HWY_INLINE bool Equal1(const T* a, const T* b) { return *a == *b; }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys(D d, Vec<D> v) const {
    return Reverse(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys2(D d, Vec<D> v) const {
    return Reverse2(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys4(D d, Vec<D> v) const {
    return Reverse4(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys8(D d, Vec<D> v) const {
    return Reverse8(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> ReverseKeys16(D d, Vec<D> v) const {
    static_assert(SortConstants::kMaxCols <= 16, "Assumes u32x16 = 512 bit");
    return ReverseKeys(d, v);
  }

  template <class V>
  HWY_INLINE V OddEvenKeys(const V odd, const V even) const {
    return OddEven(odd, even);
  }

  template <class D, HWY_IF_LANE_SIZE_D(D, 2)>
  HWY_INLINE Vec<D> SwapAdjacentPairs(D d, const Vec<D> v) const {
    const Repartition<uint32_t, D> du32;
    return BitCast(d, Shuffle2301(BitCast(du32, v)));
  }
  template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
  HWY_INLINE Vec<D> SwapAdjacentPairs(D /* tag */, const Vec<D> v) const {
    return Shuffle1032(v);
  }
  template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> SwapAdjacentPairs(D /* tag */, const Vec<D> v) const {
    return SwapAdjacentBlocks(v);
  }

  template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> SwapAdjacentQuads(D d, const Vec<D> v) const {
#if HWY_HAVE_FLOAT64  // in case D is float32
    const RepartitionToWide<D> dw;
#else
    const RepartitionToWide<RebindToUnsigned<D>> dw;
#endif
    return BitCast(d, SwapAdjacentPairs(dw, BitCast(dw, v)));
  }
  template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> SwapAdjacentQuads(D d, const Vec<D> v) const {
    // Assumes max vector size = 512
    return ConcatLowerUpper(d, v, v);
  }

  template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> OddEvenPairs(D d, const Vec<D> odd,
                                 const Vec<D> even) const {
#if HWY_HAVE_FLOAT64  // in case D is float32
    const RepartitionToWide<D> dw;
#else
    const RepartitionToWide<RebindToUnsigned<D>> dw;
#endif
    return BitCast(d, OddEven(BitCast(dw, odd), BitCast(dw, even)));
  }
  template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> OddEvenPairs(D /* tag */, Vec<D> odd, Vec<D> even) const {
    return OddEvenBlocks(odd, even);
  }

  template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> OddEvenQuads(D d, Vec<D> odd, Vec<D> even) const {
#if HWY_HAVE_FLOAT64  // in case D is float32
    const RepartitionToWide<D> dw;
#else
    const RepartitionToWide<RebindToUnsigned<D>> dw;
#endif
    return BitCast(d, OddEvenPairs(dw, BitCast(dw, odd), BitCast(dw, even)));
  }
  template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> OddEvenQuads(D d, Vec<D> odd, Vec<D> even) const {
    return ConcatUpperLower(d, odd, even);
  }
};

// Anything order-related depends on the key traits *and* the order (see
// FirstOfLanes). We cannot implement just one Compare function because Lt128
// only compiles if the lane type is u64. Thus we need either overloaded
// functions with a tag type, class specializations, or separate classes.
// We avoid overloaded functions because we want all functions to be callable
// from a SortTraits without per-function wrappers. Specializing would work, but
// we are anyway going to specialize at a higher level.
template <typename T>
struct OrderAscending : public KeyLane<T> {
  using Order = SortAscending;

  HWY_INLINE bool Compare1(const T* a, const T* b) {
    return *a < *b;
  }

  template <class D>
  HWY_INLINE Mask<D> Compare(D /* tag */, Vec<D> a, Vec<D> b) const {
    return Lt(a, b);
  }

  // Two halves of Sort2, used in ScanMinMax.
  template <class D>
  HWY_INLINE Vec<D> First(D /* tag */, const Vec<D> a, const Vec<D> b) const {
    return Min(a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> Last(D /* tag */, const Vec<D> a, const Vec<D> b) const {
    return Max(a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> FirstOfLanes(D d, Vec<D> v,
                                 T* HWY_RESTRICT /* buf */) const {
    return MinOfLanes(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> LastOfLanes(D d, Vec<D> v,
                                T* HWY_RESTRICT /* buf */) const {
    return MaxOfLanes(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> FirstValue(D d) const {
    return Set(d, hwy::LowestValue<T>());
  }

  template <class D>
  HWY_INLINE Vec<D> LastValue(D d) const {
    return Set(d, hwy::HighestValue<T>());
  }
};

template <typename T>
struct OrderDescending : public KeyLane<T> {
  using Order = SortDescending;

  HWY_INLINE bool Compare1(const T* a, const T* b) {
    return *b < *a;
  }

  template <class D>
  HWY_INLINE Mask<D> Compare(D /* tag */, Vec<D> a, Vec<D> b) const {
    return Lt(b, a);
  }

  template <class D>
  HWY_INLINE Vec<D> First(D /* tag */, const Vec<D> a, const Vec<D> b) const {
    return Max(a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> Last(D /* tag */, const Vec<D> a, const Vec<D> b) const {
    return Min(a, b);
  }

  template <class D>
  HWY_INLINE Vec<D> FirstOfLanes(D d, Vec<D> v,
                                 T* HWY_RESTRICT /* buf */) const {
    return MaxOfLanes(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> LastOfLanes(D d, Vec<D> v,
                                T* HWY_RESTRICT /* buf */) const {
    return MinOfLanes(d, v);
  }

  template <class D>
  HWY_INLINE Vec<D> FirstValue(D d) const {
    return Set(d, hwy::HighestValue<T>());
  }

  template <class D>
  HWY_INLINE Vec<D> LastValue(D d) const {
    return Set(d, hwy::LowestValue<T>());
  }
};

// Shared code that depends on Order.
template <class Base>
struct TraitsLane : public Base {
  // For each lane i: replaces a[i] with the first and b[i] with the second
  // according to Base.
  // Corresponds to a conditional swap, which is one "node" of a sorting
  // network. Min/Max are cheaper than compare + blend at least for integers.
  template <class D>
  HWY_INLINE void Sort2(D d, Vec<D>& a, Vec<D>& b) const {
    const Base* base = static_cast<const Base*>(this);

    const Vec<D> a_copy = a;
    // Prior to AVX3, there is no native 64-bit Min/Max, so they compile to 4
    // instructions. We can reduce it to a compare + 2 IfThenElse.
#if HWY_AVX3 < HWY_TARGET && HWY_TARGET <= HWY_SSSE3
    if (sizeof(TFromD<D>) == 8) {
      const Mask<D> cmp = base->Compare(d, a, b);
      a = IfThenElse(cmp, a, b);
      b = IfThenElse(cmp, b, a_copy);
      return;
    }
#endif
    a = base->First(d, a, b);
    b = base->Last(d, a_copy, b);
  }

  // Conditionally swaps even-numbered lanes with their odd-numbered neighbor.
  template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> SortPairsDistance1(D d, Vec<D> v) const {
    const Base* base = static_cast<const Base*>(this);
    Vec<D> swapped = base->ReverseKeys2(d, v);
    // Further to the above optimization, Sort2+OddEvenKeys compile to four
    // instructions; we can save one by combining two blends.
#if HWY_AVX3 < HWY_TARGET && HWY_TARGET <= HWY_SSSE3
    const Vec<D> cmp = VecFromMask(d, base->Compare(d, v, swapped));
    return IfVecThenElse(DupOdd(cmp), swapped, v);
#else
    Sort2(d, v, swapped);
    return base->OddEvenKeys(swapped, v);
#endif
  }

  // (See above - we use Sort2 for non-64-bit types.)
  template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 8)>
  HWY_INLINE Vec<D> SortPairsDistance1(D d, Vec<D> v) const {
    const Base* base = static_cast<const Base*>(this);
    Vec<D> swapped = base->ReverseKeys2(d, v);
    Sort2(d, v, swapped);
    return base->OddEvenKeys(swapped, v);
  }

  // Swaps with the vector formed by reversing contiguous groups of 4 keys.
  template <class D>
  HWY_INLINE Vec<D> SortPairsReverse4(D d, Vec<D> v) const {
    const Base* base = static_cast<const Base*>(this);
    Vec<D> swapped = base->ReverseKeys4(d, v);
    Sort2(d, v, swapped);
    return base->OddEvenPairs(d, swapped, v);
  }

  // Conditionally swaps lane 0 with 4, 1 with 5 etc.
  template <class D>
  HWY_INLINE Vec<D> SortPairsDistance4(D d, Vec<D> v) const {
    const Base* base = static_cast<const Base*>(this);
    Vec<D> swapped = base->SwapAdjacentQuads(d, v);
    // Only used in Merge16, so this will not be used on AVX2 (which only has 4
    // u64 lanes), so skip the above optimization for 64-bit AVX2.
    Sort2(d, v, swapped);
    return base->OddEvenQuads(d, swapped, v);
  }
};

#else

// Base class shared between OrderAscending, OrderDescending.
template <typename T>
struct KeyLane {
  constexpr bool Is128() const { return false; }
  constexpr size_t LanesPerKey() const { return 1; }

  using LaneType = T;
  using KeyType = T;

  std::string KeyString() const {
    char string100[100];
    hwy::detail::TypeName(hwy::detail::MakeTypeInfo<KeyType>(), 1, string100);
    return string100;
  }
};

template <typename T>
struct OrderAscending : public KeyLane<T> {
  using Order = SortAscending;

  HWY_INLINE bool Compare1(const T* a, const T* b) { return *a < *b; }

  template <class D>
  HWY_INLINE Mask<D> Compare(D /* tag */, Vec<D> a, Vec<D> b) {
    return Lt(a, b);
  }
};

template <typename T>
struct OrderDescending : public KeyLane<T> {
  using Order = SortDescending;

  HWY_INLINE bool Compare1(const T* a, const T* b) { return *b < *a; }

  template <class D>
  HWY_INLINE Mask<D> Compare(D /* tag */, Vec<D> a, Vec<D> b) {
    return Lt(b, a);
  }
};

template <class Order>
struct TraitsLane : public Order {
  // For HeapSort
  template <typename T>  // MSVC doesn't find typename Order::LaneType.
  HWY_INLINE void Swap(T* a, T* b) const {
    const T temp = *a;
    *a = *b;
    *b = temp;
  }

  template <class D>
  HWY_INLINE Vec<D> SetKey(D d, const TFromD<D>* key) const {
    return Set(d, *key);
  }
};

#endif  // VQSORT_ENABLED

}  // namespace detail
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_SORT_TRAITS_TOGGLE
