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

// Normal include guard for target-independent parts
#ifndef HIGHWAY_HWY_CONTRIB_SORT_VQSORT_INL_H_
#define HIGHWAY_HWY_CONTRIB_SORT_VQSORT_INL_H_

#ifndef VQSORT_PRINT
#define VQSORT_PRINT 0
#endif

// Makes it harder for adversaries to predict our sampling locations, at the
// cost of 1-2% increased runtime.
#ifndef VQSORT_SECURE_RNG
#define VQSORT_SECURE_RNG 0
#endif

#if VQSORT_SECURE_RNG
#include "third_party/absl/random/random.h"
#endif

#include <stdio.h>  // unconditional #include so we can use if(VQSORT_PRINT).
#include <string.h>  // memcpy

#include "hwy/cache_control.h"        // Prefetch
#include "hwy/contrib/sort/vqsort.h"  // Fill24Bytes

#if HWY_IS_MSAN
#include <sanitizer/msan_interface.h>
#endif

#endif  // HIGHWAY_HWY_CONTRIB_SORT_VQSORT_INL_H_

// Per-target
#if defined(HIGHWAY_HWY_CONTRIB_SORT_VQSORT_TOGGLE) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_VQSORT_TOGGLE
#undef HIGHWAY_HWY_CONTRIB_SORT_VQSORT_TOGGLE
#else
#define HIGHWAY_HWY_CONTRIB_SORT_VQSORT_TOGGLE
#endif

#if VQSORT_PRINT
#include "hwy/print-inl.h"
#endif

#include "hwy/contrib/sort/shared-inl.h"
#include "hwy/contrib/sort/sorting_networks-inl.h"
// Placeholder for internal instrumentation. Do not remove.
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

using Constants = hwy::SortConstants;

// Wrappers to avoid #if in user code (interferes with code folding)

HWY_INLINE void UnpoisonIfMemorySanitizer(void* p, size_t bytes) {
#if HWY_IS_MSAN
  __msan_unpoison(p, bytes);
#else
  (void)p;
  (void)bytes;
#endif
}

template <class D>
HWY_INLINE void MaybePrintVector(D d, const char* label, Vec<D> v,
                                 size_t start = 0, size_t max_lanes = 16) {
#if VQSORT_PRINT >= 2  // Print is only defined #if
  Print(d, label, v, start, max_lanes);
#else
  (void)d;
  (void)label;
  (void)v;
  (void)start;
  (void)max_lanes;
#endif
}

// ------------------------------ HeapSort

template <class Traits, typename T>
void SiftDown(Traits st, T* HWY_RESTRICT lanes, const size_t num_lanes,
              size_t start) {
  constexpr size_t N1 = st.LanesPerKey();
  const FixedTag<T, N1> d;

  while (start < num_lanes) {
    const size_t left = 2 * start + N1;
    const size_t right = 2 * start + 2 * N1;
    if (left >= num_lanes) break;
    size_t idx_larger = start;
    const auto key_j = st.SetKey(d, lanes + start);
    if (AllTrue(d, st.Compare(d, key_j, st.SetKey(d, lanes + left)))) {
      idx_larger = left;
    }
    if (right < num_lanes &&
        AllTrue(d, st.Compare(d, st.SetKey(d, lanes + idx_larger),
                              st.SetKey(d, lanes + right)))) {
      idx_larger = right;
    }
    if (idx_larger == start) break;
    st.Swap(lanes + start, lanes + idx_larger);
    start = idx_larger;
  }
}

// Heapsort: O(1) space, O(N*logN) worst-case comparisons.
// Based on LLVM sanitizer_common.h, licensed under Apache-2.0.
template <class Traits, typename T>
void HeapSort(Traits st, T* HWY_RESTRICT lanes, const size_t num_lanes) {
  constexpr size_t N1 = st.LanesPerKey();

  if (num_lanes < 2 * N1) return;

  // Build heap.
  for (size_t i = ((num_lanes - N1) / N1 / 2) * N1; i != (~N1 + 1); i -= N1) {
    SiftDown(st, lanes, num_lanes, i);
  }

  for (size_t i = num_lanes - N1; i != 0; i -= N1) {
    // Swap root with last
    st.Swap(lanes + 0, lanes + i);

    // Sift down the new root.
    SiftDown(st, lanes, i, 0);
  }
}

#if VQSORT_ENABLED || HWY_IDE

// ------------------------------ BaseCase

// Sorts `keys` within the range [0, num) via sorting network.
template <class D, class Traits, typename T>
HWY_INLINE void BaseCase(D d, Traits st, T* HWY_RESTRICT keys,
                         T* HWY_RESTRICT keys_end, size_t num,
                         T* HWY_RESTRICT buf) {
  const size_t N = Lanes(d);
  using V = decltype(Zero(d));

  // _Nonzero32 requires num - 1 != 0.
  if (HWY_UNLIKELY(num <= 1)) return;

  // Reshape into a matrix with kMaxRows rows, and columns limited by the
  // 1D `num`, which is upper-bounded by the vector width (see BaseCaseNum).
  const size_t num_pow2 = size_t{1}
                          << (32 - Num0BitsAboveMS1Bit_Nonzero32(
                                       static_cast<uint32_t>(num - 1)));
  HWY_DASSERT(num <= num_pow2 && num_pow2 <= Constants::BaseCaseNum(N));
  const size_t cols =
      HWY_MAX(st.LanesPerKey(), num_pow2 >> Constants::kMaxRowsLog2);
  HWY_DASSERT(cols <= N);

  // We can avoid padding and load/store directly to `keys` after checking the
  // original input array has enough space. Except at the right border, it's OK
  // to sort more than the current sub-array. Even if we sort across a previous
  // partition point, we know that keys will not migrate across it. However, we
  // must use the maximum size of the sorting network, because the StoreU of its
  // last vector would otherwise write invalid data starting at kMaxRows * cols.
  const size_t N_sn = Lanes(CappedTag<T, Constants::kMaxCols>());
  if (HWY_LIKELY(keys + N_sn * Constants::kMaxRows <= keys_end)) {
    SortingNetwork(st, keys, N_sn);
    return;
  }

  // Copy `keys` to `buf`.
  size_t i;
  for (i = 0; i + N <= num; i += N) {
    Store(LoadU(d, keys + i), d, buf + i);
  }
  SafeCopyN(num - i, d, keys + i, buf + i);
  i = num;

  // Fill with padding - last in sort order, not copied to keys.
  const V kPadding = st.LastValue(d);
  // Initialize an extra vector because SortingNetwork loads full vectors,
  // which may exceed cols*kMaxRows.
  for (; i < (cols * Constants::kMaxRows + N); i += N) {
    StoreU(kPadding, d, buf + i);
  }

  SortingNetwork(st, buf, cols);

  for (i = 0; i + N <= num; i += N) {
    StoreU(Load(d, buf + i), d, keys + i);
  }
  SafeCopyN(num - i, d, buf + i, keys + i);
}

// ------------------------------ Partition

// Consumes from `keys` until a multiple of kUnroll*N remains.
// Temporarily stores the right side into `buf`, then moves behind `num`.
// Returns the number of keys consumed from the left side.
template <class D, class Traits, class T>
HWY_INLINE size_t PartitionToMultipleOfUnroll(D d, Traits st,
                                              T* HWY_RESTRICT keys, size_t& num,
                                              const Vec<D> pivot,
                                              T* HWY_RESTRICT buf) {
  constexpr size_t kUnroll = Constants::kPartitionUnroll;
  const size_t N = Lanes(d);
  size_t readL = 0;
  T* HWY_RESTRICT posL = keys;
  size_t bufR = 0;
  // Partition requires both a multiple of kUnroll*N and at least
  // 2*kUnroll*N for the initial loads. If less, consume all here.
  const size_t num_rem =
      (num < 2 * kUnroll * N) ? num : (num & (kUnroll * N - 1));
  size_t i = 0;
  for (; i + N <= num_rem; i += N) {
    const Vec<D> vL = LoadU(d, keys + readL);
    readL += N;

    const auto comp = st.Compare(d, pivot, vL);
    posL += CompressBlendedStore(vL, Not(comp), d, posL);
    bufR += CompressStore(vL, comp, d, buf + bufR);
  }
  // Last iteration: only use valid lanes.
  if (HWY_LIKELY(i != num_rem)) {
    const auto mask = FirstN(d, num_rem - i);
    const Vec<D> vL = LoadU(d, keys + readL);

    const auto comp = st.Compare(d, pivot, vL);
    posL += CompressBlendedStore(vL, AndNot(comp, mask), d, posL);
    bufR += CompressStore(vL, And(comp, mask), d, buf + bufR);
  }

  // MSAN seems not to understand CompressStore. buf[0, bufR) are valid.
  UnpoisonIfMemorySanitizer(buf, bufR * sizeof(T));

  // Everything we loaded was put into buf, or behind the current `posL`, after
  // which there is space for bufR items. First move items from `keys + num` to
  // `posL` to free up space, then copy `buf` into the vacated `keys + num`.
  // A loop with masked loads from `buf` is insufficient - we would also need to
  // mask from `keys + num`. Combining a loop with memcpy for the remainders is
  // slower than just memcpy, so we use that for simplicity.
  num -= bufR;
  memcpy(posL, keys + num, bufR * sizeof(T));
  memcpy(keys + num, buf, bufR * sizeof(T));
  return static_cast<size_t>(posL - keys);  // caller will shrink num by this.
}

template <class V>
V OrXor(const V o, const V x1, const V x2) {
  // TODO(janwas): add op so we can benefit from AVX-512 ternlog?
  return Or(o, Xor(x1, x2));
}

// Note: we could track the OrXor of v and pivot to see if the entire left
// partition is equal, but that happens rarely and thus is a net loss.
template <class D, class Traits, typename T>
HWY_INLINE void StoreLeftRight(D d, Traits st, const Vec<D> v,
                               const Vec<D> pivot, T* HWY_RESTRICT keys,
                               size_t& writeL, size_t& remaining) {
  const size_t N = Lanes(d);

  const auto comp = st.Compare(d, pivot, v);

  remaining -= N;
  if (hwy::HWY_NAMESPACE::CompressIsPartition<T>::value ||
      (HWY_MAX_BYTES == 16 && st.Is128())) {
    // Non-native Compress (e.g. AVX2): we are able to partition a vector using
    // a single Compress+two StoreU instead of two Compress[Blended]Store. The
    // latter are more expensive. Because we store entire vectors, the contents
    // between the updated writeL and writeR are ignored and will be overwritten
    // by subsequent calls. This works because writeL and writeR are at least
    // two vectors apart.
    const auto lr = st.CompressKeys(v, comp);
    const size_t num_left = N - CountTrue(d, comp);
    StoreU(lr, d, keys + writeL);
    // Now write the right-side elements (if any), such that the previous writeR
    // is one past the end of the newly written right elements, then advance.
    StoreU(lr, d, keys + remaining + writeL);
    writeL += num_left;
  } else {
    // Native Compress[Store] (e.g. AVX3), which only keep the left or right
    // side, not both, hence we require two calls.
    const size_t num_left = CompressStore(v, Not(comp), d, keys + writeL);
    writeL += num_left;

    (void)CompressBlendedStore(v, comp, d, keys + remaining + writeL);
  }
}

template <class D, class Traits, typename T>
HWY_INLINE void StoreLeftRight4(D d, Traits st, const Vec<D> v0,
                                const Vec<D> v1, const Vec<D> v2,
                                const Vec<D> v3, const Vec<D> pivot,
                                T* HWY_RESTRICT keys, size_t& writeL,
                                size_t& remaining) {
  StoreLeftRight(d, st, v0, pivot, keys, writeL, remaining);
  StoreLeftRight(d, st, v1, pivot, keys, writeL, remaining);
  StoreLeftRight(d, st, v2, pivot, keys, writeL, remaining);
  StoreLeftRight(d, st, v3, pivot, keys, writeL, remaining);
}

// Moves "<= pivot" keys to the front, and others to the back. pivot is
// broadcasted. Time-critical!
//
// Aligned loads do not seem to be worthwhile (not bottlenecked by load ports).
template <class D, class Traits, typename T>
HWY_INLINE size_t Partition(D d, Traits st, T* HWY_RESTRICT keys, size_t num,
                            const Vec<D> pivot, T* HWY_RESTRICT buf) {
  using V = decltype(Zero(d));
  const size_t N = Lanes(d);

  // StoreLeftRight will CompressBlendedStore ending at `writeR`. Unless all
  // lanes happen to be in the right-side partition, this will overrun `keys`,
  // which triggers asan errors. Avoid by special-casing the last vector.
  HWY_DASSERT(num > 2 * N);  // ensured by HandleSpecialCases
  num -= N;
  size_t last = num;
  const V vlast = LoadU(d, keys + last);

  const size_t consumedL =
      PartitionToMultipleOfUnroll(d, st, keys, num, pivot, buf);
  keys += consumedL;
  last -= consumedL;
  num -= consumedL;
  constexpr size_t kUnroll = Constants::kPartitionUnroll;

  // Partition splits the vector into 3 sections, left to right: Elements
  // smaller or equal to the pivot, unpartitioned elements and elements larger
  // than the pivot. To write elements unconditionally on the loop body without
  // overwriting existing data, we maintain two regions of the loop where all
  // elements have been copied elsewhere (e.g. vector registers.). I call these
  // bufferL and bufferR, for left and right respectively.
  //
  // These regions are tracked by the indices (writeL, writeR, left, right) as
  // presented in the diagram below.
  //
  //              writeL                                  writeR
  //               \/                                       \/
  //  |  <= pivot   | bufferL |   unpartitioned   | bufferR |   > pivot   |
  //                          \/                  \/
  //                         left                 right
  //
  // In the main loop body below we choose a side, load some elements out of the
  // vector and move either `left` or `right`. Next we call into StoreLeftRight
  // to partition the data, and the partitioned elements will be written either
  // to writeR or writeL and the corresponding index will be moved accordingly.
  //
  // Note that writeR is not explicitly tracked as an optimization for platforms
  // with conditional operations. Instead we track writeL and the number of
  // elements left to process (`remaining`). From the diagram above we can see
  // that:
  //    writeR - writeL = remaining => writeR = remaining + writeL
  //
  // Tracking `remaining` is advantageous because each iteration reduces the
  // number of unpartitioned elements by a fixed amount, so we can compute
  // `remaining` without data dependencies.
  //
  size_t writeL = 0;
  size_t remaining = num;

  const T* HWY_RESTRICT readL = keys;
  const T* HWY_RESTRICT readR = keys + num;
  // Cannot load if there were fewer than 2 * kUnroll * N.
  if (HWY_LIKELY(num != 0)) {
    HWY_DASSERT(num >= 2 * kUnroll * N);
    HWY_DASSERT((num & (kUnroll * N - 1)) == 0);

    // Make space for writing in-place by reading from readL/readR.
    const V vL0 = LoadU(d, readL + 0 * N);
    const V vL1 = LoadU(d, readL + 1 * N);
    const V vL2 = LoadU(d, readL + 2 * N);
    const V vL3 = LoadU(d, readL + 3 * N);
    readL += kUnroll * N;
    readR -= kUnroll * N;
    const V vR0 = LoadU(d, readR + 0 * N);
    const V vR1 = LoadU(d, readR + 1 * N);
    const V vR2 = LoadU(d, readR + 2 * N);
    const V vR3 = LoadU(d, readR + 3 * N);

    // readL/readR changed above, so check again before the loop.
    while (readL != readR) {
      V v0, v1, v2, v3;

      // Data-dependent but branching is faster than forcing branch-free.
      const size_t capacityL =
          static_cast<size_t>((readL - keys) - static_cast<ptrdiff_t>(writeL));
      HWY_DASSERT(capacityL <= num);  // >= 0
      // Load data from the end of the vector with less data (front or back).
      // The next paragraphs explain how this works.
      //
      // let block_size = (kUnroll * N)
      // On the loop prelude we load block_size elements from the front of the
      // vector and an additional block_size elements from the back. On each
      // iteration k elements are written to the front of the vector and
      // (block_size - k) to the back.
      //
      // This creates a loop invariant where the capacity on the front
      // (capacityL) and on the back (capacityR) always add to 2 * block_size.
      // In other words:
      //    capacityL + capacityR = 2 * block_size
      //    capacityR = 2 * block_size - capacityL
      //
      // This means that:
      //    capacityL < capacityR <=>
      //    capacityL < 2 * block_size - capacityL <=>
      //    2 * capacityL < 2 * block_size <=>
      //    capacityL < block_size
      //
      // Thus the check on the next line is equivalent to capacityL > capacityR.
      //
      if (kUnroll * N < capacityL) {
        readR -= kUnroll * N;
        v0 = LoadU(d, readR + 0 * N);
        v1 = LoadU(d, readR + 1 * N);
        v2 = LoadU(d, readR + 2 * N);
        v3 = LoadU(d, readR + 3 * N);
        hwy::Prefetch(readR - 3 * kUnroll * N);
      } else {
        v0 = LoadU(d, readL + 0 * N);
        v1 = LoadU(d, readL + 1 * N);
        v2 = LoadU(d, readL + 2 * N);
        v3 = LoadU(d, readL + 3 * N);
        readL += kUnroll * N;
        hwy::Prefetch(readL + 3 * kUnroll * N);
      }

      StoreLeftRight4(d, st, v0, v1, v2, v3, pivot, keys, writeL, remaining);
    }

    // Now finish writing the saved vectors to the middle.
    StoreLeftRight4(d, st, vL0, vL1, vL2, vL3, pivot, keys, writeL, remaining);
    StoreLeftRight4(d, st, vR0, vR1, vR2, vR3, pivot, keys, writeL, remaining);
  }

  // We have partitioned [left, right) such that writeL is the boundary.
  HWY_DASSERT(remaining == 0);
  // Make space for inserting vlast: move up to N of the first right-side keys
  // into the unused space starting at last. If we have fewer, ensure they are
  // the last items in that vector by subtracting from the *load* address,
  // which is safe because we have at least two vectors (checked above).
  const size_t totalR = last - writeL;
  const size_t startR = totalR < N ? writeL + totalR - N : writeL;
  StoreU(LoadU(d, keys + startR), d, keys + last);

  // Partition vlast: write L, then R, into the single-vector gap at writeL.
  const auto comp = st.Compare(d, pivot, vlast);
  writeL += CompressBlendedStore(vlast, Not(comp), d, keys + writeL);
  (void)CompressBlendedStore(vlast, comp, d, keys + writeL);

  return consumedL + writeL;
}

// Returns true and partitions if [keys, keys + num) contains only {valueL,
// valueR}. Otherwise, sets third to the first differing value; keys may have
// been reordered and a regular Partition is still necessary.
// Called from two locations, hence NOINLINE.
template <class D, class Traits, typename T>
HWY_NOINLINE bool MaybePartitionTwoValue(D d, Traits st, T* HWY_RESTRICT keys,
                                         size_t num, const Vec<D> valueL,
                                         const Vec<D> valueR, Vec<D>& third,
                                         T* HWY_RESTRICT buf) {
  const size_t N = Lanes(d);

  size_t i = 0;
  size_t writeL = 0;

  // As long as all lanes are equal to L or R, we can overwrite with valueL.
  // This is faster than first counting, then backtracking to fill L and R.
  for (; i + N <= num; i += N) {
    const Vec<D> v = LoadU(d, keys + i);
    // It is not clear how to apply OrXor here - that can check if *both*
    // comparisons are true, but here we want *either*. Comparing the unsigned
    // min of differences to zero works, but is expensive for u64 prior to AVX3.
    const Mask<D> eqL = st.EqualKeys(d, v, valueL);
    const Mask<D> eqR = st.EqualKeys(d, v, valueR);
    // At least one other value present; will require a regular partition.
    // On AVX-512, Or + AllTrue are folded into a single kortest if we are
    // careful with the FindKnownFirstTrue argument, see below.
    if (HWY_UNLIKELY(!AllTrue(d, Or(eqL, eqR)))) {
      // If we repeat Or(eqL, eqR) here, the compiler will hoist it into the
      // loop, which is a pessimization because this if-true branch is cold.
      // We can defeat this via Not(Xor), which is equivalent because eqL and
      // eqR cannot be true at the same time. Can we elide the additional Not?
      // FindFirstFalse instructions are generally unavailable, but we can
      // fuse Not and Xor/Or into one ExclusiveNeither.
      const size_t lane = FindKnownFirstTrue(d, ExclusiveNeither(eqL, eqR));
      third = st.SetKey(d, keys + i + lane);
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "found 3rd value at vec %zu; writeL %zu\n", i, writeL);
      }
      // 'Undo' what we did by filling the remainder of what we read with R.
      for (; writeL + N <= i; writeL += N) {
        StoreU(valueR, d, keys + writeL);
      }
      BlendedStore(valueR, FirstN(d, i - writeL), d, keys + writeL);
      return false;
    }
    StoreU(valueL, d, keys + writeL);
    writeL += CountTrue(d, eqL);
  }

  // Final vector, masked comparison (no effect if i == num)
  const size_t remaining = num - i;
  SafeCopyN(remaining, d, keys + i, buf);
  const Vec<D> v = Load(d, buf);
  const Mask<D> valid = FirstN(d, remaining);
  const Mask<D> eqL = And(st.EqualKeys(d, v, valueL), valid);
  const Mask<D> eqR = st.EqualKeys(d, v, valueR);
  // Invalid lanes are considered equal.
  const Mask<D> eq = Or(Or(eqL, eqR), Not(valid));
  // At least one other value present; will require a regular partition.
  if (HWY_UNLIKELY(!AllTrue(d, eq))) {
    const size_t lane = FindKnownFirstTrue(d, Not(eq));
    third = st.SetKey(d, keys + i + lane);
    if (VQSORT_PRINT >= 2) {
      fprintf(stderr, "found 3rd value at partial vec %zu; writeL %zu\n", i,
              writeL);
    }
    // 'Undo' what we did by filling the remainder of what we read with R.
    for (; writeL + N <= i; writeL += N) {
      StoreU(valueR, d, keys + writeL);
    }
    BlendedStore(valueR, FirstN(d, i - writeL), d, keys + writeL);
    return false;
  }
  BlendedStore(valueL, valid, d, keys + writeL);
  writeL += CountTrue(d, eqL);

  // Fill right side
  i = writeL;
  for (; i + N <= num; i += N) {
    StoreU(valueR, d, keys + i);
  }
  BlendedStore(valueR, FirstN(d, num - i), d, keys + i);

  if (VQSORT_PRINT >= 2) {
    fprintf(stderr, "Successful MaybePartitionTwoValue\n");
  }
  return true;
}

// Same as above, except that the pivot equals valueR, so scan right to left.
template <class D, class Traits, typename T>
HWY_INLINE bool MaybePartitionTwoValueR(D d, Traits st, T* HWY_RESTRICT keys,
                                        size_t num, const Vec<D> valueL,
                                        const Vec<D> valueR, Vec<D>& third,
                                        T* HWY_RESTRICT buf) {
  const size_t N = Lanes(d);

  HWY_DASSERT(num >= N);
  size_t pos = num - N;  // current read/write position
  size_t countR = 0;     // number of valueR found

  // For whole vectors, in descending address order: as long as all lanes are
  // equal to L or R, overwrite with valueR. This is faster than counting, then
  // filling both L and R. Loop terminates after unsigned wraparound.
  for (; pos < num; pos -= N) {
    const Vec<D> v = LoadU(d, keys + pos);
    // It is not clear how to apply OrXor here - that can check if *both*
    // comparisons are true, but here we want *either*. Comparing the unsigned
    // min of differences to zero works, but is expensive for u64 prior to AVX3.
    const Mask<D> eqL = st.EqualKeys(d, v, valueL);
    const Mask<D> eqR = st.EqualKeys(d, v, valueR);
    // If there is a third value, stop and undo what we've done. On AVX-512,
    // Or + AllTrue are folded into a single kortest, but only if we are
    // careful with the FindKnownFirstTrue argument - see prior comment on that.
    if (HWY_UNLIKELY(!AllTrue(d, Or(eqL, eqR)))) {
      const size_t lane = FindKnownFirstTrue(d, ExclusiveNeither(eqL, eqR));
      third = st.SetKey(d, keys + pos + lane);
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "found 3rd value at vec %zu; countR %zu\n", pos,
                countR);
        MaybePrintVector(d, "third", third, 0, st.LanesPerKey());
      }
      pos += N;  // rewind: we haven't yet committed changes in this iteration.
      // We have filled [pos, num) with R, but only countR of them should have
      // been written. Rewrite [pos, num - countR) to L.
      HWY_DASSERT(countR <= num - pos);
      const size_t endL = num - countR;
      for (; pos + N <= endL; pos += N) {
        StoreU(valueL, d, keys + pos);
      }
      BlendedStore(valueL, FirstN(d, endL - pos), d, keys + pos);
      return false;
    }
    StoreU(valueR, d, keys + pos);
    countR += CountTrue(d, eqR);
  }

  // Final partial (or empty) vector, masked comparison.
  const size_t remaining = pos + N;
  HWY_DASSERT(remaining <= N);
  const Vec<D> v = LoadU(d, keys);  // Safe because num >= N.
  const Mask<D> valid = FirstN(d, remaining);
  const Mask<D> eqL = st.EqualKeys(d, v, valueL);
  const Mask<D> eqR = And(st.EqualKeys(d, v, valueR), valid);
  // Invalid lanes are considered equal.
  const Mask<D> eq = Or(Or(eqL, eqR), Not(valid));
  // At least one other value present; will require a regular partition.
  if (HWY_UNLIKELY(!AllTrue(d, eq))) {
    const size_t lane = FindKnownFirstTrue(d, Not(eq));
    third = st.SetKey(d, keys + lane);
    if (VQSORT_PRINT >= 2) {
      fprintf(stderr, "found 3rd value at partial vec %zu; writeR %zu\n", pos,
              countR);
      MaybePrintVector(d, "third", third, 0, st.LanesPerKey());
    }
    pos += N;  // rewind: we haven't yet committed changes in this iteration.
    // We have filled [pos, num) with R, but only countR of them should have
    // been written. Rewrite [pos, num - countR) to L.
    HWY_DASSERT(countR <= num - pos);
    const size_t endL = num - countR;
    for (; pos + N <= endL; pos += N) {
      StoreU(valueL, d, keys + pos);
    }
    BlendedStore(valueL, FirstN(d, endL - pos), d, keys + pos);
    return false;
  }
  const size_t lastR = CountTrue(d, eqR);
  countR += lastR;

  // First finish writing valueR - [0, N) lanes were not yet written.
  StoreU(valueR, d, keys);  // Safe because num >= N.

  // Fill left side (ascending order for clarity)
  const size_t endL = num - countR;
  size_t i = 0;
  for (; i + N <= endL; i += N) {
    StoreU(valueL, d, keys + i);
  }
  Store(valueL, d, buf);
  SafeCopyN(endL - i, d, buf, keys + i);  // avoids asan overrun

  if (VQSORT_PRINT >= 2) {
    fprintf(stderr,
            "MaybePartitionTwoValueR countR %zu pos %zu i %zu endL %zu\n",
            countR, pos, i, endL);
  }

  return true;
}

// `idx_second` is `first_mismatch` from `AllEqual` and thus the index of the
// second key. This is the first path into `MaybePartitionTwoValue`, called
// when all samples are equal. Returns false if there are at least a third
// value and sets `third`. Otherwise, partitions the array and returns true.
template <class D, class Traits, typename T>
HWY_INLINE bool PartitionIfTwoKeys(D d, Traits st, const Vec<D> pivot,
                                   T* HWY_RESTRICT keys, size_t num,
                                   const size_t idx_second, const Vec<D> second,
                                   Vec<D>& third, T* HWY_RESTRICT buf) {
  // True if second comes before pivot.
  const bool is_pivotR = AllFalse(d, st.Compare(d, pivot, second));
  if (VQSORT_PRINT >= 1) {
    fprintf(stderr, "Samples all equal, diff at %zu, isPivotR %d\n", idx_second,
            is_pivotR);
  }
  HWY_DASSERT(AllFalse(d, st.EqualKeys(d, second, pivot)));

  // If pivot is R, we scan backwards over the entire array. Otherwise,
  // we already scanned up to idx_second and can leave those in place.
  return is_pivotR ? MaybePartitionTwoValueR(d, st, keys, num, second, pivot,
                                             third, buf)
                   : MaybePartitionTwoValue(d, st, keys + idx_second,
                                            num - idx_second, pivot, second,
                                            third, buf);
}

// Second path into `MaybePartitionTwoValue`, called when not all samples are
// equal. `samples` is sorted.
template <class D, class Traits, typename T>
HWY_INLINE bool PartitionIfTwoSamples(D d, Traits st, T* HWY_RESTRICT keys,
                                      size_t num, T* HWY_RESTRICT samples) {
  constexpr size_t kSampleLanes = 3 * 64 / sizeof(T);
  constexpr size_t N1 = st.LanesPerKey();
  const Vec<D> valueL = st.SetKey(d, samples);
  const Vec<D> valueR = st.SetKey(d, samples + kSampleLanes - N1);
  HWY_DASSERT(AllTrue(d, st.Compare(d, valueL, valueR)));
  HWY_DASSERT(AllFalse(d, st.EqualKeys(d, valueL, valueR)));
  const Vec<D> prev = st.PrevValue(d, valueR);
  // If the sample has more than two values, then the keys have at least that
  // many, and thus this special case is inapplicable.
  if (HWY_UNLIKELY(!AllTrue(d, st.EqualKeys(d, valueL, prev)))) {
    return false;
  }

  // Must not overwrite samples because if this returns false, caller wants to
  // read the original samples again.
  T* HWY_RESTRICT buf = samples + kSampleLanes;
  Vec<D> third;  // unused
  return MaybePartitionTwoValue(d, st, keys, num, valueL, valueR, third, buf);
}

// ------------------------------ Pivot sampling

template <class Traits, class V>
HWY_INLINE V MedianOf3(Traits st, V v0, V v1, V v2) {
  const DFromV<V> d;
  // Slightly faster for 128-bit, apparently because not serially dependent.
  if (st.Is128()) {
    // Median = XOR-sum 'minus' the first and last. Calling First twice is
    // slightly faster than Compare + 2 IfThenElse or even IfThenElse + XOR.
    const auto sum = Xor(Xor(v0, v1), v2);
    const auto first = st.First(d, st.First(d, v0, v1), v2);
    const auto last = st.Last(d, st.Last(d, v0, v1), v2);
    return Xor(Xor(sum, first), last);
  }
  st.Sort2(d, v0, v2);
  v1 = st.Last(d, v0, v1);
  v1 = st.First(d, v1, v2);
  return v1;
}

#if VQSORT_SECURE_RNG
using Generator = absl::BitGen;
#else
// Based on https://github.com/numpy/numpy/issues/16313#issuecomment-641897028
#pragma pack(push, 1)
class Generator {
 public:
  Generator(const void* heap, size_t num) {
    Sorter::Fill24Bytes(heap, num, &a_);
    k_ = 1;  // stream index: must be odd
  }

  explicit Generator(uint64_t seed) {
    a_ = b_ = w_ = seed;
    k_ = 1;
  }

  uint64_t operator()() {
    const uint64_t b = b_;
    w_ += k_;
    const uint64_t next = a_ ^ w_;
    a_ = (b + (b << 3)) ^ (b >> 11);
    const uint64_t rot = (b << 24) | (b >> 40);
    b_ = rot + next;
    return next;
  }

 private:
  uint64_t a_;
  uint64_t b_;
  uint64_t w_;
  uint64_t k_;  // increment
};
#pragma pack(pop)

#endif  // !VQSORT_SECURE_RNG

// Returns slightly biased random index of a chunk in [0, num_chunks).
// See https://www.pcg-random.org/posts/bounded-rands.html.
HWY_INLINE size_t RandomChunkIndex(const uint32_t num_chunks, uint32_t bits) {
  const uint64_t chunk_index = (static_cast<uint64_t>(bits) * num_chunks) >> 32;
  HWY_DASSERT(chunk_index < num_chunks);
  return static_cast<size_t>(chunk_index);
}

// Writes samples from `keys[0, num)` into `buf`.
template <class D, class Traits, typename T>
HWY_INLINE void DrawSamples(D d, Traits st, T* HWY_RESTRICT keys, size_t num,
                            T* HWY_RESTRICT buf, Generator& rng) {
  using V = decltype(Zero(d));
  const size_t N = Lanes(d);

  // Power of two
  constexpr size_t kLanesPerChunk = Constants::LanesPerChunk(sizeof(T));

  // Align start of keys to chunks. We always have at least 2 chunks because the
  // base case would have handled anything up to 16 vectors, i.e. >= 4 chunks.
  HWY_DASSERT(num >= 2 * kLanesPerChunk);
  const size_t misalign =
      (reinterpret_cast<uintptr_t>(keys) / sizeof(T)) & (kLanesPerChunk - 1);
  if (misalign != 0) {
    const size_t consume = kLanesPerChunk - misalign;
    keys += consume;
    num -= consume;
  }

  // Generate enough random bits for 9 uint32
  uint64_t* bits64 = reinterpret_cast<uint64_t*>(buf);
  for (size_t i = 0; i < 5; ++i) {
    bits64[i] = rng();
  }
  const uint32_t* bits = reinterpret_cast<const uint32_t*>(buf);

  const size_t num_chunks64 = num / kLanesPerChunk;
  // Clamp to uint32 for RandomChunkIndex
  const uint32_t num_chunks =
      static_cast<uint32_t>(HWY_MIN(num_chunks64, 0xFFFFFFFFull));

  const size_t offset0 = RandomChunkIndex(num_chunks, bits[0]) * kLanesPerChunk;
  const size_t offset1 = RandomChunkIndex(num_chunks, bits[1]) * kLanesPerChunk;
  const size_t offset2 = RandomChunkIndex(num_chunks, bits[2]) * kLanesPerChunk;
  const size_t offset3 = RandomChunkIndex(num_chunks, bits[3]) * kLanesPerChunk;
  const size_t offset4 = RandomChunkIndex(num_chunks, bits[4]) * kLanesPerChunk;
  const size_t offset5 = RandomChunkIndex(num_chunks, bits[5]) * kLanesPerChunk;
  const size_t offset6 = RandomChunkIndex(num_chunks, bits[6]) * kLanesPerChunk;
  const size_t offset7 = RandomChunkIndex(num_chunks, bits[7]) * kLanesPerChunk;
  const size_t offset8 = RandomChunkIndex(num_chunks, bits[8]) * kLanesPerChunk;
  for (size_t i = 0; i < kLanesPerChunk; i += N) {
    const V v0 = Load(d, keys + offset0 + i);
    const V v1 = Load(d, keys + offset1 + i);
    const V v2 = Load(d, keys + offset2 + i);
    const V medians0 = MedianOf3(st, v0, v1, v2);
    Store(medians0, d, buf + i);

    const V v3 = Load(d, keys + offset3 + i);
    const V v4 = Load(d, keys + offset4 + i);
    const V v5 = Load(d, keys + offset5 + i);
    const V medians1 = MedianOf3(st, v3, v4, v5);
    Store(medians1, d, buf + i + kLanesPerChunk);

    const V v6 = Load(d, keys + offset6 + i);
    const V v7 = Load(d, keys + offset7 + i);
    const V v8 = Load(d, keys + offset8 + i);
    const V medians2 = MedianOf3(st, v6, v7, v8);
    Store(medians2, d, buf + i + kLanesPerChunk * 2);
  }
}

// For detecting inputs where (almost) all keys are equal.
template <class D, class Traits>
HWY_INLINE bool UnsortedSampleEqual(D d, Traits st,
                                    const TFromD<D>* HWY_RESTRICT samples) {
  constexpr size_t kSampleLanes = 3 * 64 / sizeof(TFromD<D>);
  const size_t N = Lanes(d);
  using V = Vec<D>;

  const V first = st.SetKey(d, samples);
  // OR of XOR-difference may be faster than comparison.
  V diff = Zero(d);
  size_t i = 0;
  for (; i + N <= kSampleLanes; i += N) {
    const V v = Load(d, samples + i);
    diff = OrXor(diff, first, v);
  }
  // Remainder, if any.
  const V v = Load(d, samples + i);
  const auto valid = FirstN(d, kSampleLanes - i);
  diff = IfThenElse(valid, OrXor(diff, first, v), diff);

  return st.NoKeyDifference(d, diff);
}

template <class D, class Traits, typename T>
HWY_INLINE void SortSamples(D d, Traits st, T* HWY_RESTRICT buf) {
  // buf contains 192 bytes, so 16 128-bit vectors are necessary and sufficient.
  constexpr size_t kSampleLanes = 3 * 64 / sizeof(T);
  const CappedTag<T, 16 / sizeof(T)> d128;
  const size_t N128 = Lanes(d128);
  constexpr size_t kCols = HWY_MIN(16 / sizeof(T), Constants::kMaxCols);
  constexpr size_t kBytes = kCols * Constants::kMaxRows * sizeof(T);
  static_assert(192 <= kBytes, "");
  // Fill with padding - last in sort order.
  const auto kPadding = st.LastValue(d128);
  // Initialize an extra vector because SortingNetwork loads full vectors,
  // which may exceed cols*kMaxRows.
  for (size_t i = kSampleLanes; i <= kBytes / sizeof(T); i += N128) {
    StoreU(kPadding, d128, buf + i);
  }

  SortingNetwork(st, buf, kCols);

  if (VQSORT_PRINT >= 2) {
    const size_t N = Lanes(d);
    fprintf(stderr, "Samples:\n");
    for (size_t i = 0; i < kSampleLanes; i += N) {
      MaybePrintVector(d, "", Load(d, buf + i), 0, N);
    }
  }
}

// ------------------------------ Pivot selection

enum class PivotResult {
  kDone,     // stop without partitioning (all equal, or two-value partition)
  kNormal,   // partition and recurse left and right
  kIsFirst,  // partition but skip left recursion
  kWasLast,  // partition but skip right recursion
};

HWY_INLINE const char* PivotResultString(PivotResult result) {
  switch (result) {
    case PivotResult::kDone:
      return "done";
    case PivotResult::kNormal:
      return "normal";
    case PivotResult::kIsFirst:
      return "first";
    case PivotResult::kWasLast:
      return "last";
  }
  return "unknown";
}

template <class Traits, typename T>
HWY_INLINE size_t PivotRank(Traits st, const T* HWY_RESTRICT samples) {
  constexpr size_t kSampleLanes = 3 * 64 / sizeof(T);
  constexpr size_t N1 = st.LanesPerKey();

  constexpr size_t kRankMid = kSampleLanes / 2;
  static_assert(kRankMid % N1 == 0, "Mid is not an aligned key");

  // Find the previous value not equal to the median.
  size_t rank_prev = kRankMid - N1;
  for (; st.Equal1(samples + rank_prev, samples + kRankMid); rank_prev -= N1) {
    // All previous samples are equal to the median.
    if (rank_prev == 0) return 0;
  }

  size_t rank_next = rank_prev + N1;
  for (; st.Equal1(samples + rank_next, samples + kRankMid); rank_next += N1) {
    // The median is also the largest sample. If it is also the largest key,
    // we'd end up with an empty right partition, so choose the previous key.
    if (rank_next == kSampleLanes - N1) return rank_prev;
  }

  // If we choose the median as pivot, the ratio of keys ending in the left
  // partition will likely be rank_next/kSampleLanes (if the sample is
  // representative). This is because equal-to-pivot values also land in the
  // left - it's infeasible to do an in-place vectorized 3-way partition.
  // Check whether prev would lead to a more balanced partition.
  const size_t excess_if_median = rank_next - kRankMid;
  const size_t excess_if_prev = kRankMid - rank_prev;
  return excess_if_median < excess_if_prev ? kRankMid : rank_prev;
}

// Returns pivot chosen from `samples`. It will never be the largest key
// (thus the right partition will never be empty).
template <class D, class Traits, typename T>
HWY_INLINE Vec<D> ChoosePivotByRank(D d, Traits st,
                                    const T* HWY_RESTRICT samples) {
  const size_t pivot_rank = PivotRank(st, samples);
  const Vec<D> pivot = st.SetKey(d, samples + pivot_rank);
  if (VQSORT_PRINT >= 2) {
    fprintf(stderr, "  Pivot rank %zu = %f\n", pivot_rank,
            static_cast<double>(GetLane(pivot)));
  }
  // Verify pivot is not equal to the last sample.
  constexpr size_t kSampleLanes = 3 * 64 / sizeof(T);
  constexpr size_t N1 = st.LanesPerKey();
  const Vec<D> last = st.SetKey(d, samples + kSampleLanes - N1);
  const bool all_neq = AllTrue(d, st.NotEqualKeys(d, pivot, last));
  (void)all_neq;
  HWY_DASSERT(all_neq);
  return pivot;
}

// Returns true if all keys equal `pivot`, otherwise returns false and sets
// `*first_mismatch' to the index of the first differing key.
template <class D, class Traits, typename T>
HWY_INLINE bool AllEqual(D d, Traits st, const Vec<D> pivot,
                         const T* HWY_RESTRICT keys, size_t num,
                         size_t* HWY_RESTRICT first_mismatch) {
  const size_t N = Lanes(d);
  // Ensures we can use overlapping loads for the tail; see HandleSpecialCases.
  HWY_DASSERT(num >= N);
  const Vec<D> zero = Zero(d);

  // Vector-align keys + i.
  const size_t misalign =
      (reinterpret_cast<uintptr_t>(keys) / sizeof(T)) & (N - 1);
  HWY_DASSERT(misalign % st.LanesPerKey() == 0);
  const size_t consume = N - misalign;
  {
    const Vec<D> v = LoadU(d, keys);
    // Only check masked lanes; consider others to be equal.
    const Mask<D> diff = And(FirstN(d, consume), st.NotEqualKeys(d, v, pivot));
    if (HWY_UNLIKELY(!AllFalse(d, diff))) {
      const size_t lane = FindKnownFirstTrue(d, diff);
      *first_mismatch = lane;
      return false;
    }
  }
  size_t i = consume;
  HWY_DASSERT(((reinterpret_cast<uintptr_t>(keys + i) / sizeof(T)) & (N - 1)) ==
              0);

  // Sticky bits registering any difference between `keys` and the first key.
  // We use vector XOR because it may be cheaper than comparisons, especially
  // for 128-bit. 2x unrolled for more ILP.
  Vec<D> diff0 = zero;
  Vec<D> diff1 = zero;

  // We want to stop once a difference has been found, but without slowing
  // down the loop by comparing during each iteration. The compromise is to
  // compare after a 'group', which consists of kLoops times two vectors.
  constexpr size_t kLoops = 8;
  const size_t lanes_per_group = kLoops * 2 * N;

  for (; i + lanes_per_group <= num; i += lanes_per_group) {
    HWY_DEFAULT_UNROLL
    for (size_t loop = 0; loop < kLoops; ++loop) {
      const Vec<D> v0 = Load(d, keys + i + loop * 2 * N);
      const Vec<D> v1 = Load(d, keys + i + loop * 2 * N + N);
      diff0 = OrXor(diff0, v0, pivot);
      diff1 = OrXor(diff1, v1, pivot);
    }

    // If there was a difference in the entire group:
    if (HWY_UNLIKELY(!st.NoKeyDifference(d, Or(diff0, diff1)))) {
      // .. then loop until the first one, with termination guarantee.
      for (;; i += N) {
        const Vec<D> v = Load(d, keys + i);
        const Mask<D> diff = st.NotEqualKeys(d, v, pivot);
        if (HWY_UNLIKELY(!AllFalse(d, diff))) {
          const size_t lane = FindKnownFirstTrue(d, diff);
          *first_mismatch = i + lane;
          return false;
        }
      }
    }
  }

  // Whole vectors, no unrolling, compare directly
  for (; i + N <= num; i += N) {
    const Vec<D> v = Load(d, keys + i);
    const Mask<D> diff = st.NotEqualKeys(d, v, pivot);
    if (HWY_UNLIKELY(!AllFalse(d, diff))) {
      const size_t lane = FindKnownFirstTrue(d, diff);
      *first_mismatch = i + lane;
      return false;
    }
  }
  // Always re-check the last (unaligned) vector to reduce branching.
  i = num - N;
  const Vec<D> v = LoadU(d, keys + i);
  const Mask<D> diff = st.NotEqualKeys(d, v, pivot);
  if (HWY_UNLIKELY(!AllFalse(d, diff))) {
    const size_t lane = FindKnownFirstTrue(d, diff);
    *first_mismatch = i + lane;
    return false;
  }

  if (VQSORT_PRINT >= 1) {
    fprintf(stderr, "All keys equal\n");
  }
  return true;  // all equal
}

// Called from 'two locations', but only one is active (IsKV is constexpr).
template <class D, class Traits, typename T>
HWY_INLINE bool ExistsAnyBefore(D d, Traits st, const T* HWY_RESTRICT keys,
                                size_t num, const Vec<D> pivot) {
  const size_t N = Lanes(d);
  HWY_DASSERT(num >= N);  // See HandleSpecialCases

  if (VQSORT_PRINT >= 2) {
    fprintf(stderr, "Scanning for before\n");
  }

  size_t i = 0;

  constexpr size_t kLoops = 16;
  const size_t lanes_per_group = kLoops * N;

  Vec<D> first = pivot;

  // Whole group, unrolled
  for (; i + lanes_per_group <= num; i += lanes_per_group) {
    HWY_DEFAULT_UNROLL
    for (size_t loop = 0; loop < kLoops; ++loop) {
      const Vec<D> curr = LoadU(d, keys + i + loop * N);
      first = st.First(d, first, curr);
    }

    if (HWY_UNLIKELY(!AllFalse(d, st.Compare(d, first, pivot)))) {
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "Stopped scanning at end of group %zu\n",
                i + lanes_per_group);
      }
      return true;
    }
  }
  // Whole vectors, no unrolling
  for (; i + N <= num; i += N) {
    const Vec<D> curr = LoadU(d, keys + i);
    if (HWY_UNLIKELY(!AllFalse(d, st.Compare(d, curr, pivot)))) {
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "Stopped scanning at %zu\n", i);
      }
      return true;
    }
  }
  // If there are remainders, re-check the last whole vector.
  if (HWY_LIKELY(i != num)) {
    const Vec<D> curr = LoadU(d, keys + num - N);
    if (HWY_UNLIKELY(!AllFalse(d, st.Compare(d, curr, pivot)))) {
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "Stopped scanning at last %zu\n", num - N);
      }
      return true;
    }
  }

  return false;  // pivot is the first
}

// Called from 'two locations', but only one is active (IsKV is constexpr).
template <class D, class Traits, typename T>
HWY_INLINE bool ExistsAnyAfter(D d, Traits st, const T* HWY_RESTRICT keys,
                               size_t num, const Vec<D> pivot) {
  const size_t N = Lanes(d);
  HWY_DASSERT(num >= N);  // See HandleSpecialCases

  if (VQSORT_PRINT >= 2) {
    fprintf(stderr, "Scanning for after\n");
  }

  size_t i = 0;

  constexpr size_t kLoops = 16;
  const size_t lanes_per_group = kLoops * N;

  Vec<D> last = pivot;

  // Whole group, unrolled
  for (; i + lanes_per_group <= num; i += lanes_per_group) {
    HWY_DEFAULT_UNROLL
    for (size_t loop = 0; loop < kLoops; ++loop) {
      const Vec<D> curr = LoadU(d, keys + i + loop * N);
      last = st.Last(d, last, curr);
    }

    if (HWY_UNLIKELY(!AllFalse(d, st.Compare(d, pivot, last)))) {
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "Stopped scanning at end of group %zu\n",
                i + lanes_per_group);
      }
      return true;
    }
  }
  // Whole vectors, no unrolling
  for (; i + N <= num; i += N) {
    const Vec<D> curr = LoadU(d, keys + i);
    if (HWY_UNLIKELY(!AllFalse(d, st.Compare(d, pivot, curr)))) {
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "Stopped scanning at %zu\n", i);
      }
      return true;
    }
  }
  // If there are remainders, re-check the last whole vector.
  if (HWY_LIKELY(i != num)) {
    const Vec<D> curr = LoadU(d, keys + num - N);
    if (HWY_UNLIKELY(!AllFalse(d, st.Compare(d, pivot, curr)))) {
      if (VQSORT_PRINT >= 2) {
        fprintf(stderr, "Stopped scanning at last %zu\n", num - N);
      }
      return true;
    }
  }

  return false;  // pivot is the last
}

// Returns pivot chosen from `keys[0, num)`. It will never be the largest key
// (thus the right partition will never be empty).
template <class D, class Traits, typename T>
HWY_INLINE Vec<D> ChoosePivotForEqualSamples(D d, Traits st,
                                             T* HWY_RESTRICT keys, size_t num,
                                             T* HWY_RESTRICT samples,
                                             Vec<D> second, Vec<D> third,
                                             PivotResult& result) {
  const Vec<D> pivot = st.SetKey(d, samples);  // the single unique sample

  // Early out for mostly-0 arrays, where pivot is often FirstValue.
  if (HWY_UNLIKELY(AllTrue(d, st.EqualKeys(d, pivot, st.FirstValue(d))))) {
    result = PivotResult::kIsFirst;
    return pivot;
  }
  if (HWY_UNLIKELY(AllTrue(d, st.EqualKeys(d, pivot, st.LastValue(d))))) {
    result = PivotResult::kWasLast;
    return st.PrevValue(d, pivot);
  }

  // If key-value, we didn't run PartitionIfTwo* and thus `third` is unknown and
  // cannot be used.
  if (st.IsKV()) {
    // If true, pivot is either middle or last.
    const bool before = !AllFalse(d, st.Compare(d, second, pivot));
    if (HWY_UNLIKELY(before)) {
      // Not last, so middle.
      if (HWY_UNLIKELY(ExistsAnyAfter(d, st, keys, num, pivot))) {
        result = PivotResult::kNormal;
        return pivot;
      }

      // We didn't find anything after pivot, so it is the last. Because keys
      // equal to the pivot go to the left partition, the right partition would
      // be empty and Partition will not have changed anything. Instead use the
      // previous value in sort order, which is not necessarily an actual key.
      result = PivotResult::kWasLast;
      return st.PrevValue(d, pivot);
    }

    // Otherwise, pivot is first or middle. Rule out it being first:
    if (HWY_UNLIKELY(ExistsAnyBefore(d, st, keys, num, pivot))) {
      result = PivotResult::kNormal;
      return pivot;
    }
    // It is first: fall through to shared code below.
  } else {
    // Check if pivot is between two known values. If so, it is not the first
    // nor the last and we can avoid scanning.
    st.Sort2(d, second, third);
    HWY_DASSERT(AllTrue(d, st.Compare(d, second, third)));
    const bool before = !AllFalse(d, st.Compare(d, second, pivot));
    const bool after = !AllFalse(d, st.Compare(d, pivot, third));
    // Only reached if there are three keys, which means pivot is either first,
    // last, or in between. Thus there is another key that comes before or
    // after.
    HWY_DASSERT(before || after);
    if (HWY_UNLIKELY(before)) {
      // Neither first nor last.
      if (HWY_UNLIKELY(after || ExistsAnyAfter(d, st, keys, num, pivot))) {
        result = PivotResult::kNormal;
        return pivot;
      }

      // We didn't find anything after pivot, so it is the last. Because keys
      // equal to the pivot go to the left partition, the right partition would
      // be empty and Partition will not have changed anything. Instead use the
      // previous value in sort order, which is not necessarily an actual key.
      result = PivotResult::kWasLast;
      return st.PrevValue(d, pivot);
    }

    // Has after, and we found one before: in the middle.
    if (HWY_UNLIKELY(ExistsAnyBefore(d, st, keys, num, pivot))) {
      result = PivotResult::kNormal;
      return pivot;
    }
  }

  // Pivot is first. We could consider a special partition mode that only
  // reads from and writes to the right side, and later fills in the left
  // side, which we know is equal to the pivot. However, that leads to more
  // cache misses if the array is large, and doesn't save much, hence is a
  // net loss.
  result = PivotResult::kIsFirst;
  return pivot;
}

// ------------------------------ Quicksort recursion

template <class D, class Traits, typename T>
HWY_NOINLINE void PrintMinMax(D d, Traits st, const T* HWY_RESTRICT keys,
                              size_t num, T* HWY_RESTRICT buf) {
  if (VQSORT_PRINT >= 2) {
    const size_t N = Lanes(d);
    if (num < N) return;

    Vec<D> first = st.LastValue(d);
    Vec<D> last = st.FirstValue(d);

    size_t i = 0;
    for (; i + N <= num; i += N) {
      const Vec<D> v = LoadU(d, keys + i);
      first = st.First(d, v, first);
      last = st.Last(d, v, last);
    }
    if (HWY_LIKELY(i != num)) {
      HWY_DASSERT(num >= N);  // See HandleSpecialCases
      const Vec<D> v = LoadU(d, keys + num - N);
      first = st.First(d, v, first);
      last = st.Last(d, v, last);
    }

    first = st.FirstOfLanes(d, first, buf);
    last = st.LastOfLanes(d, last, buf);
    MaybePrintVector(d, "first", first, 0, st.LanesPerKey());
    MaybePrintVector(d, "last", last, 0, st.LanesPerKey());
  }
}

// keys_end is the end of the entire user input, not just the current subarray
// [keys, keys + num).
template <class D, class Traits, typename T>
HWY_NOINLINE void Recurse(D d, Traits st, T* HWY_RESTRICT keys,
                          T* HWY_RESTRICT keys_end, const size_t num,
                          T* HWY_RESTRICT buf, Generator& rng,
                          const size_t remaining_levels) {
  HWY_DASSERT(num != 0);

  if (HWY_UNLIKELY(num <= Constants::BaseCaseNum(Lanes(d)))) {
    BaseCase(d, st, keys, keys_end, num, buf);
    return;
  }

  // Move after BaseCase so we skip printing for small subarrays.
  if (VQSORT_PRINT >= 1) {
    fprintf(stderr, "\n\n=== Recurse depth=%zu len=%zu\n", remaining_levels,
            num);
    PrintMinMax(d, st, keys, num, buf);
  }

  DrawSamples(d, st, keys, num, buf, rng);

  Vec<D> pivot;
  PivotResult result = PivotResult::kNormal;
  if (HWY_UNLIKELY(UnsortedSampleEqual(d, st, buf))) {
    pivot = st.SetKey(d, buf);
    size_t idx_second = 0;
    if (HWY_UNLIKELY(AllEqual(d, st, pivot, keys, num, &idx_second))) {
      return;
    }
    HWY_DASSERT(idx_second % st.LanesPerKey() == 0);
    // Must capture the value before PartitionIfTwoKeys may overwrite it.
    const Vec<D> second = st.SetKey(d, keys + idx_second);
    MaybePrintVector(d, "pivot", pivot, 0, st.LanesPerKey());
    MaybePrintVector(d, "second", second, 0, st.LanesPerKey());

    Vec<D> third;
    // Not supported for key-value types because two 'keys' may be equivalent
    // but not interchangeable (their values may differ).
    if (HWY_UNLIKELY(!st.IsKV() &&
                     PartitionIfTwoKeys(d, st, pivot, keys, num, idx_second,
                                        second, third, buf))) {
      return;  // Done, skip recursion because each side has all-equal keys.
    }

    // We can no longer start scanning from idx_second because
    // PartitionIfTwoKeys may have reordered keys.
    pivot = ChoosePivotForEqualSamples(d, st, keys, num, buf, second, third,
                                       result);
    // If kNormal, `pivot` is very common but not the first/last. It is
    // tempting to do a 3-way partition (to avoid moving the =pivot keys a
    // second time), but that is a net loss due to the extra comparisons.
  } else {
    SortSamples(d, st, buf);

    // Not supported for key-value types because two 'keys' may be equivalent
    // but not interchangeable (their values may differ).
    if (HWY_UNLIKELY(!st.IsKV() &&
                     PartitionIfTwoSamples(d, st, keys, num, buf))) {
      return;
    }

    pivot = ChoosePivotByRank(d, st, buf);
  }

  // Too many recursions. This is unlikely to happen because we select pivots
  // from large (though still O(1)) samples.
  if (HWY_UNLIKELY(remaining_levels == 0)) {
    if (VQSORT_PRINT >= 1) {
      fprintf(stderr, "HeapSort reached, size=%zu\n", num);
    }
    HeapSort(st, keys, num);  // Slow but N*logN.
    return;
  }

  const size_t bound = Partition(d, st, keys, num, pivot, buf);
  if (VQSORT_PRINT >= 2) {
    fprintf(stderr, "bound %zu num %zu result %s\n", bound, num,
            PivotResultString(result));
  }
  // The left partition is not empty because the pivot is one of the keys
  // (unless kWasLast, in which case the pivot is PrevValue, but we still
  // have at least one value <= pivot because AllEqual ruled out the case of
  // only one unique value, and there is exactly one value after pivot).
  HWY_DASSERT(bound != 0);
  // ChoosePivot* ensure pivot != last, so the right partition is never empty.
  HWY_DASSERT(bound != num);

  if (HWY_LIKELY(result != PivotResult::kIsFirst)) {
    Recurse(d, st, keys, keys_end, bound, buf, rng, remaining_levels - 1);
  }
  if (HWY_LIKELY(result != PivotResult::kWasLast)) {
    Recurse(d, st, keys + bound, keys_end, num - bound, buf, rng,
            remaining_levels - 1);
  }
}

// Returns true if sorting is finished.
template <class D, class Traits, typename T>
HWY_INLINE bool HandleSpecialCases(D d, Traits st, T* HWY_RESTRICT keys,
                                   size_t num) {
  const size_t N = Lanes(d);
  const size_t base_case_num = Constants::BaseCaseNum(N);

  // 128-bit keys require vectors with at least two u64 lanes, which is always
  // the case unless `d` requests partial vectors (e.g. fraction = 1/2) AND the
  // hardware vector width is less than 128bit / fraction.
  const bool partial_128 = !IsFull(d) && N < 2 && st.Is128();
  // Partition assumes its input is at least two vectors. If vectors are huge,
  // base_case_num may actually be smaller. If so, which is only possible on
  // RVV, pass a capped or partial d (LMUL < 1). Use HWY_MAX_BYTES instead of
  // HWY_LANES to account for the largest possible LMUL.
  constexpr bool kPotentiallyHuge =
      HWY_MAX_BYTES / sizeof(T) > Constants::kMaxRows * Constants::kMaxCols;
  const bool huge_vec = kPotentiallyHuge && (2 * N > base_case_num);
  if (partial_128 || huge_vec) {
    if (VQSORT_PRINT >= 1) {
      fprintf(stderr, "WARNING: using slow HeapSort: partial %d huge %d\n",
              partial_128, huge_vec);
    }
    HeapSort(st, keys, num);
    return true;
  }

  // Small arrays are already handled by Recurse.

  // We could also check for already sorted/reverse/equal, but that's probably
  // counterproductive if vqsort is used as a base case.

  return false;  // not finished sorting
}

#endif  // VQSORT_ENABLED
}  // namespace detail

// Sorts `keys[0..num-1]` according to the order defined by `st.Compare`.
// In-place i.e. O(1) additional storage. Worst-case N*logN comparisons.
// Non-stable (order of equal keys may change), except for the common case where
// the upper bits of T are the key, and the lower bits are a sequential or at
// least unique ID.
// There is no upper limit on `num`, but note that pivots may be chosen by
// sampling only from the first 256 GiB.
//
// `d` is typically SortTag<T> (chooses between full and partial vectors).
// `st` is SharedTraits<Traits*<Order*>>. This abstraction layer bridges
//   differences in sort order and single-lane vs 128-bit keys.
template <class D, class Traits, typename T>
void Sort(D d, Traits st, T* HWY_RESTRICT keys, size_t num,
          T* HWY_RESTRICT buf) {
  if (VQSORT_PRINT >= 1) {
    fprintf(stderr, "=============== Sort num %zu\n", num);
  }

#if VQSORT_ENABLED || HWY_IDE
#if !HWY_HAVE_SCALABLE
  // On targets with fixed-size vectors, avoid _using_ the allocated memory.
  // We avoid (potentially expensive for small input sizes) allocations on
  // platforms where no targets are scalable. For 512-bit vectors, this fits on
  // the stack (several KiB).
  HWY_ALIGN T storage[SortConstants::BufNum<T>(HWY_LANES(T))] = {};
  static_assert(sizeof(storage) <= 8192, "Unexpectedly large, check size");
  buf = storage;
#endif  // !HWY_HAVE_SCALABLE

  if (detail::HandleSpecialCases(d, st, keys, num)) return;

#if HWY_MAX_BYTES > 64
  // sorting_networks-inl and traits assume no more than 512 bit vectors.
  if (HWY_UNLIKELY(Lanes(d) > 64 / sizeof(T))) {
    return Sort(CappedTag<T, 64 / sizeof(T)>(), st, keys, num, buf);
  }
#endif  // HWY_MAX_BYTES > 64

  detail::Generator rng(keys, num);

  // Introspection: switch to worst-case N*logN heapsort after this many.
  const size_t max_levels = 2 * hwy::CeilLog2(num) + 4;
  detail::Recurse(d, st, keys, keys + num, num, buf, rng, max_levels);
#else
  (void)d;
  (void)buf;
  if (VQSORT_PRINT >= 1) {
    fprintf(stderr, "WARNING: using slow HeapSort because vqsort disabled\n");
  }
  return detail::HeapSort(st, keys, num);
#endif  // VQSORT_ENABLED
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_SORT_VQSORT_TOGGLE
