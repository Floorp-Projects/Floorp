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

// Per-target include guard

#if defined(HIGHWAY_HWY_CONTRIB_SORT_SORT_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_SORT_INL_H_
#undef HIGHWAY_HWY_CONTRIB_SORT_SORT_INL_H_
#else
#define HIGHWAY_HWY_CONTRIB_SORT_SORT_INL_H_
#endif

#include <inttypes.h>

#include "hwy/aligned_allocator.h"
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

enum class SortOrder { kAscending, kDescending };

#if HWY_TARGET != HWY_SCALAR && HWY_ARCH_X86

#define HWY_SORT_VERIFY 1

constexpr inline SortOrder Reverse(SortOrder order) {
  return (order == SortOrder::kAscending) ? SortOrder::kDescending
                                          : SortOrder::kAscending;
}

namespace verify {

template <typename T>
bool Compare(T a, T b, SortOrder kOrder) {
  if (kOrder == SortOrder::kAscending) return a <= b;
  return a >= b;
}

#if HWY_SORT_VERIFY

template <class D>
class Runs {
  using T = TFromD<D>;

 public:
  Runs(D d, size_t num_regs, size_t run_length = 0, bool alternating = false) {
    const size_t N = Lanes(d);

    buf_ = AllocateAligned<T>(N);
    consecutive_ = AllocateAligned<T>(num_regs * N);

    num_regs_ = num_regs;
    if (run_length) {
      run_length_ = run_length;
      num_runs_ = num_regs * N / run_length;
      is_vector_ = true;
      alternating_ = alternating;
    } else {
      run_length_ = num_regs * 4;
      num_runs_ = N / 4;
      is_vector_ = false;
      alternating_ = false;
    }
  }

  void ScatterQuartets(D d, const size_t idx_reg, Vec<D> v) {
    HWY_ASSERT(idx_reg < num_regs_);
    const size_t N = Lanes(d);
    for (size_t i = 0; i < N; i += 4) {
      Store(v, d, buf_.get());
      const size_t idx_q = (i / 4) * num_regs_ + idx_reg;
      CopyBytes<16>(buf_.get() + i, consecutive_.get() + idx_q * 4);
    }
  }

  void StoreVector(D d, const size_t idx_reg, Vec<D> v) {
    HWY_ASSERT(idx_reg < num_regs_);
    Store(v, d, &consecutive_[idx_reg * Lanes(d)]);
  }

  bool IsBitonic() const {
    HWY_ASSERT(!alternating_);
    for (size_t ir = 0; ir < num_runs_; ++ir) {
      const T* p = &consecutive_[ir * run_length_];
      bool is_asc = true;
      bool is_desc = true;
      bool is_zero = true;

      for (size_t i = 0; i < run_length_ / 2 - 1; ++i) {
        is_asc &= (p[i] <= p[i + 1]);
        is_desc &= (p[i] >= p[i + 1]);
      }
      for (size_t i = 0; i < run_length_; ++i) {
        is_zero &= (p[i] == 0);
      }

      bool is_asc2 = true;
      bool is_desc2 = true;
      for (size_t i = run_length_ / 2; i < run_length_ - 1; ++i) {
        is_asc2 &= (p[i] <= p[i + 1]);
        is_desc2 &= (p[i] >= p[i + 1]);
      }

      if (is_zero) continue;
      if (is_asc && is_desc2) continue;
      if (is_desc && is_asc2) continue;
      return false;
    }
    return true;
  }

  void CheckBitonic(int line, int caller) const {
    if (IsBitonic()) return;
    for (size_t ir = 0; ir < num_runs_; ++ir) {
      const T* p = &consecutive_[ir * run_length_];
      printf("run %" PRIu64 " (len %" PRIu64 ")\n", static_cast<uint64_t>(ir),
             static_cast<uint64_t>(run_length_));
      for (size_t i = 0; i < run_length_; ++i) {
        printf("%.0f\n", static_cast<float>(p[i]));
      }
    }
    printf("caller %d\n", caller);
    hwy::Abort("", line, "not bitonic");
  }

  void CheckSorted(SortOrder kOrder, int line, int caller) const {
    for (size_t ir = 0; ir < num_runs_; ++ir) {
      const SortOrder order =
          (alternating_ && (ir & 1)) ? Reverse(kOrder) : kOrder;
      const T* p = &consecutive_[ir * run_length_];

      for (size_t i = 0; i < run_length_ - 1; ++i) {
        if (!Compare(p[i], p[i + 1], order)) {
          printf("ir%" PRIu64 " run_length=%" PRIu64
                 " alt=%d original order=%d this order=%d\n",
                 static_cast<uint64_t>(ir), static_cast<uint64_t>(run_length_),
                 alternating_, static_cast<int>(kOrder),
                 static_cast<int>(order));
          for (size_t i = 0; i < run_length_; ++i) {
            printf(" %.0f\n", static_cast<float>(p[i]));
          }
          printf("caller %d\n", caller);
          hwy::Abort("", line, "not sorted");
        }
      }
    }
  }

 private:
  AlignedFreeUniquePtr<T[]> buf_;
  AlignedFreeUniquePtr<T[]> consecutive_;
  size_t num_regs_;
  size_t run_length_;
  size_t num_runs_;
  bool is_vector_;
  bool alternating_;
};

template <class D>
Runs<D> StoreDeinterleavedQuartets(D d, Vec<D> v0) {
  Runs<D> runs(d, 1);
  runs.ScatterQuartets(d, 0, v0);
  return runs;
}

template <class D>
Runs<D> StoreDeinterleavedQuartets(D d, Vec<D> v0, Vec<D> v1) {
  Runs<D> runs(d, 2);
  runs.ScatterQuartets(d, 0, v0);
  runs.ScatterQuartets(d, 1, v1);
  return runs;
}

template <class D>
Runs<D> StoreDeinterleavedQuartets(D d, Vec<D> v0, Vec<D> v1, Vec<D> v2,
                                   Vec<D> v3) {
  Runs<D> runs(d, 4);
  runs.ScatterQuartets(d, 0, v0);
  runs.ScatterQuartets(d, 1, v1);
  runs.ScatterQuartets(d, 2, v2);
  runs.ScatterQuartets(d, 3, v3);
  return runs;
}

template <class D>
Runs<D> StoreDeinterleavedQuartets(D d, Vec<D> v0, Vec<D> v1, Vec<D> v2,
                                   Vec<D> v3, Vec<D> v4, Vec<D> v5, Vec<D> v6,
                                   Vec<D> v7) {
  Runs<D> runs(d, 8);
  runs.ScatterQuartets(d, 0, v0);
  runs.ScatterQuartets(d, 1, v1);
  runs.ScatterQuartets(d, 2, v2);
  runs.ScatterQuartets(d, 3, v3);
  runs.ScatterQuartets(d, 4, v4);
  runs.ScatterQuartets(d, 5, v5);
  runs.ScatterQuartets(d, 6, v6);
  runs.ScatterQuartets(d, 7, v7);
  return runs;
}

template <class D>
Runs<D> StoreDeinterleavedQuartets(D d, Vec<D> v0, Vec<D> v1, Vec<D> v2,
                                   Vec<D> v3, Vec<D> v4, Vec<D> v5, Vec<D> v6,
                                   Vec<D> v7, Vec<D> v8, Vec<D> v9, Vec<D> vA,
                                   Vec<D> vB, Vec<D> vC, Vec<D> vD, Vec<D> vE,
                                   Vec<D> vF) {
  Runs<D> runs(d, 16);
  runs.ScatterQuartets(d, 0x0, v0);
  runs.ScatterQuartets(d, 0x1, v1);
  runs.ScatterQuartets(d, 0x2, v2);
  runs.ScatterQuartets(d, 0x3, v3);
  runs.ScatterQuartets(d, 0x4, v4);
  runs.ScatterQuartets(d, 0x5, v5);
  runs.ScatterQuartets(d, 0x6, v6);
  runs.ScatterQuartets(d, 0x7, v7);
  runs.ScatterQuartets(d, 0x8, v8);
  runs.ScatterQuartets(d, 0x9, v9);
  runs.ScatterQuartets(d, 0xA, vA);
  runs.ScatterQuartets(d, 0xB, vB);
  runs.ScatterQuartets(d, 0xC, vC);
  runs.ScatterQuartets(d, 0xD, vD);
  runs.ScatterQuartets(d, 0xE, vE);
  runs.ScatterQuartets(d, 0xF, vF);
  return runs;
}

template <class D>
Runs<D> StoreDeinterleavedQuartets(
    D d, const Vec<D>& v00, const Vec<D>& v01, const Vec<D>& v02,
    const Vec<D>& v03, const Vec<D>& v04, const Vec<D>& v05, const Vec<D>& v06,
    const Vec<D>& v07, const Vec<D>& v08, const Vec<D>& v09, const Vec<D>& v0A,
    const Vec<D>& v0B, const Vec<D>& v0C, const Vec<D>& v0D, const Vec<D>& v0E,
    const Vec<D>& v0F, const Vec<D>& v10, const Vec<D>& v11, const Vec<D>& v12,
    const Vec<D>& v13, const Vec<D>& v14, const Vec<D>& v15, const Vec<D>& v16,
    const Vec<D>& v17, const Vec<D>& v18, const Vec<D>& v19, const Vec<D>& v1A,
    const Vec<D>& v1B, const Vec<D>& v1C, const Vec<D>& v1D, const Vec<D>& v1E,
    const Vec<D>& v1F) {
  Runs<D> runs(d, 32);
  runs.ScatterQuartets(d, 0x00, v00);
  runs.ScatterQuartets(d, 0x01, v01);
  runs.ScatterQuartets(d, 0x02, v02);
  runs.ScatterQuartets(d, 0x03, v03);
  runs.ScatterQuartets(d, 0x04, v04);
  runs.ScatterQuartets(d, 0x05, v05);
  runs.ScatterQuartets(d, 0x06, v06);
  runs.ScatterQuartets(d, 0x07, v07);
  runs.ScatterQuartets(d, 0x08, v08);
  runs.ScatterQuartets(d, 0x09, v09);
  runs.ScatterQuartets(d, 0x0A, v0A);
  runs.ScatterQuartets(d, 0x0B, v0B);
  runs.ScatterQuartets(d, 0x0C, v0C);
  runs.ScatterQuartets(d, 0x0D, v0D);
  runs.ScatterQuartets(d, 0x0E, v0E);
  runs.ScatterQuartets(d, 0x0F, v0F);
  runs.ScatterQuartets(d, 0x10, v10);
  runs.ScatterQuartets(d, 0x11, v11);
  runs.ScatterQuartets(d, 0x12, v12);
  runs.ScatterQuartets(d, 0x13, v13);
  runs.ScatterQuartets(d, 0x14, v14);
  runs.ScatterQuartets(d, 0x15, v15);
  runs.ScatterQuartets(d, 0x16, v16);
  runs.ScatterQuartets(d, 0x17, v17);
  runs.ScatterQuartets(d, 0x18, v18);
  runs.ScatterQuartets(d, 0x19, v19);
  runs.ScatterQuartets(d, 0x1A, v1A);
  runs.ScatterQuartets(d, 0x1B, v1B);
  runs.ScatterQuartets(d, 0x1C, v1C);
  runs.ScatterQuartets(d, 0x1D, v1D);
  runs.ScatterQuartets(d, 0x1E, v1E);
  runs.ScatterQuartets(d, 0x1F, v1F);
  return runs;
}

template <class D>
Runs<D> StoreVectors(D d, Vec<D> v0, size_t run_length, bool alternating) {
  Runs<D> runs(d, 1, run_length, alternating);
  runs.StoreVector(d, 0, v0);
  return runs;
}

template <class D>
Runs<D> StoreVectors(D d, Vec<D> v0, Vec<D> v1) {
  constexpr size_t kRegs = 2;
  Runs<D> runs(d, kRegs, /*run_length=*/kRegs * Lanes(d), /*alternating=*/false);
  runs.StoreVector(d, 0, v0);
  runs.StoreVector(d, 1, v1);
  return runs;
}

template <class D>
Runs<D> StoreVectors(D d, Vec<D> v0, Vec<D> v1, Vec<D> v2, Vec<D> v3) {
  constexpr size_t kRegs = 4;
  Runs<D> runs(d, kRegs, /*run_length=*/kRegs * Lanes(d), /*alternating=*/false);
  runs.StoreVector(d, 0, v0);
  runs.StoreVector(d, 1, v1);
  runs.StoreVector(d, 2, v2);
  runs.StoreVector(d, 3, v3);
  return runs;
}

template <class D>
Runs<D> StoreVectors(D d, Vec<D> v0, Vec<D> v1, Vec<D> v2, Vec<D> v3, Vec<D> v4,
                     Vec<D> v5, Vec<D> v6, Vec<D> v7) {
  constexpr size_t kRegs = 8;
  Runs<D> runs(d, kRegs, /*run_length=*/kRegs * Lanes(d), /*alternating=*/false);
  runs.StoreVector(d, 0, v0);
  runs.StoreVector(d, 1, v1);
  runs.StoreVector(d, 2, v2);
  runs.StoreVector(d, 3, v3);
  runs.StoreVector(d, 4, v4);
  runs.StoreVector(d, 5, v5);
  runs.StoreVector(d, 6, v6);
  runs.StoreVector(d, 7, v7);
  return runs;
}

#endif  // HWY_SORT_VERIFY
}  // namespace verify

namespace detail {

// ------------------------------ Vector-length agnostic (quartets)

// For each lane i: replaces a[i] with the first and b[i] with the second
// according to kOrder.
// Corresponds to a conditional swap, which is one "node" of a sorting network.
// Min/Max are cheaper than compare + blend at least for integers.
template <SortOrder kOrder, class V>
HWY_INLINE void SortLanesIn2Vectors(V& a, V& b) {
  V temp = a;
  a = (kOrder == SortOrder::kAscending) ? Min(a, b) : Max(a, b);
  b = (kOrder == SortOrder::kAscending) ? Max(temp, b) : Min(temp, b);
}

// For each lane: sorts the four values in the that lane of the four vectors.
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void SortLanesIn4Vectors(D d, const TFromD<D>* in, V& v0, V& v1,
                                    V& v2, V& v3) {
  const size_t N = Lanes(d);

  // Bitonic and odd-even sorters both have 5 nodes. This one is from
  // http://users.telenet.be/bertdobbelaere/SorterHunter/sorting_networks.html

  // layer 1
  v0 = Load(d, in + 0 * N);
  v2 = Load(d, in + 2 * N);
  SortLanesIn2Vectors<kOrder>(v0, v2);
  v1 = Load(d, in + 1 * N);
  v3 = Load(d, in + 3 * N);
  SortLanesIn2Vectors<kOrder>(v1, v3);

  // layer 2
  SortLanesIn2Vectors<kOrder>(v0, v1);
  SortLanesIn2Vectors<kOrder>(v2, v3);

  // layer 3
  SortLanesIn2Vectors<kOrder>(v1, v2);
}

// Inputs are vectors with columns in sorted order (from SortLanesIn4Vectors).
// Transposes so that output vectors are sorted quartets (128-bit blocks),
// and a quartet in v0 comes before its counterpart in v1, etc.
template <class D, class V = Vec<D>>
HWY_INLINE void Transpose4x4(D d, V& v0, V& v1, V& v2, V& v3) {
  const RepartitionToWide<decltype(d)> dw;

  // Input: first number is reg, second is lane (0 is lowest)
  // 03 02 01 00  |
  // 13 12 11 10  | columns are sorted
  // 23 22 21 20  | (in this order)
  // 33 32 31 30  V
  const V t0 = InterleaveLower(d, v0, v1);  // 11 01 10 00
  const V t1 = InterleaveLower(d, v2, v3);  // 31 21 30 20
  const V t2 = InterleaveUpper(d, v0, v1);  // 13 03 12 02
  const V t3 = InterleaveUpper(d, v2, v3);  // 33 23 32 22

  // 30 20 10 00
  v0 = BitCast(d, InterleaveLower(BitCast(dw, t0), BitCast(dw, t1)));
  // 31 21 11 01
  v1 = BitCast(d, InterleaveUpper(BitCast(dw, t0), BitCast(dw, t1)));
  // 32 22 12 02
  v2 = BitCast(d, InterleaveLower(BitCast(dw, t2), BitCast(dw, t3)));
  // 33 23 13 03 --> sorted in descending order (03=smallest in lane 0).
  v3 = BitCast(d, InterleaveUpper(BitCast(dw, t2), BitCast(dw, t3)));
}

// 12 ops (including 4 swizzle)
// Precondition: v0 and v1 are already sorted according to kOrder.
// Postcondition: concatenate(v0, v1) is sorted and v0 is the lower half.
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void Merge2SortedQuartets(D d, V& v0, V& v1, int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input0 = verify::StoreDeinterleavedQuartets(d, v0);
  const verify::Runs<D> input1 = verify::StoreDeinterleavedQuartets(d, v1);
  input0.CheckSorted(kOrder, __LINE__, caller);
  input1.CheckSorted(kOrder, __LINE__, caller);
#endif

  // See figure 5 from https://www.vldb.org/pvldb/vol8/p1274-inoue.pdf.
  // This requires 8 min/max vs 6 for bitonic merge (see Figure 2 in
  // http://www.vldb.org/pvldb/vol1/1454171.pdf), but is faster overall because
  // it needs less shuffling, and does not need a bitonic input.
  SortLanesIn2Vectors<kOrder>(v0, v1);
  v0 = Shuffle0321(v0);
  SortLanesIn2Vectors<kOrder>(v0, v1);
  v0 = Shuffle0321(v0);
  SortLanesIn2Vectors<kOrder>(v0, v1);
  v0 = Shuffle0321(v0);
  SortLanesIn2Vectors<kOrder>(v0, v1);
  v0 = Shuffle0321(v0);

#if HWY_SORT_VERIFY
  auto output = verify::StoreDeinterleavedQuartets(d, v0, v1);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// ------------------------------ Bitonic merge (quartets)

// For the last layer of bitonic merge. Conditionally swaps even-numbered lanes
// with their odd-numbered neighbor. Works for both quartets and vectors.
template <SortOrder kOrder, class D>
HWY_INLINE void SortAdjacentLanesQV(D d, Vec<D>& q_or_v) {
  (void)d;
  // Optimization for 32-bit integers: swap via Shuffle and 64-bit Min/Max.
  // (not worthwhile on SSE4/AVX2 because they lack 64-bit Min/Max)
#if !HWY_ARCH_X86 || HWY_TARGET <= HWY_AVX3
  if (sizeof(TFromD<D>) == 4 && !IsFloat<TFromD<D>>()) {
    const RepartitionToWide<decltype(d)> dw;
    const auto wide = BitCast(dw, q_or_v);
    const auto swap = BitCast(dw, Shuffle2301(q_or_v));
    if (kOrder == SortOrder::kAscending) {
      q_or_v = BitCast(d, Max(wide, swap));
    } else {
      q_or_v = BitCast(d, Min(wide, swap));
    }
  } else
#endif
  {
    Vec<D> swapped = Shuffle2301(q_or_v);
    SortLanesIn2Vectors<kOrder>(q_or_v, swapped);
    q_or_v = OddEven(swapped, q_or_v);
  }
}

// Lane 0 with 2, 1 with 3 etc. Works for both quartets and vectors.
template <SortOrder kOrder, class D>
HWY_INLINE void SortDistance2LanesQV(D d, Vec<D>& q_or_v) {
  const RepartitionToWide<decltype(d)> dw;
  Vec<D> swapped = Shuffle1032(q_or_v);
  SortLanesIn2Vectors<kOrder>(q_or_v, swapped);
  q_or_v = BitCast(d, OddEven(BitCast(dw, swapped), BitCast(dw, q_or_v)));
}

// For all BitonicMerge*, and each block, the concatenation of those blocks from
// the first half and second half of the input vectors must be sorted in
// opposite orders.

// 14 ops (including 4 swizzle)
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void BitonicMerge2Quartets(D d, V& q0, V& q1, int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input = verify::StoreDeinterleavedQuartets(d, q0, q1);
  if (caller == -1) input.CheckBitonic(__LINE__, __LINE__);
#endif

  // Layer 1: lane stride 4 (2 ops)
  SortLanesIn2Vectors<kOrder>(q0, q1);

  // Layer 2: lane stride 2 (6 ops)
  SortDistance2LanesQV<kOrder>(d, q0);
  SortDistance2LanesQV<kOrder>(d, q1);

  // Layer 3: lane stride 1 (4 ops)
  SortAdjacentLanesQV<kOrder>(d, q0);
  SortAdjacentLanesQV<kOrder>(d, q1);

#if HWY_SORT_VERIFY
  const verify::Runs<D> output = verify::StoreDeinterleavedQuartets(d, q0, q1);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// 32 ops, more efficient than three 4+4 merges (36 ops).
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void BitonicMerge4Quartets(D d, V& q0, V& q1, V& q2, V& q3,
                                      int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input =
      verify::StoreDeinterleavedQuartets(d, q0, q1, q2, q3);
  if (caller == -1) input.CheckBitonic(__LINE__, __LINE__);
#endif

  // Layer 1: lane stride 8
  SortLanesIn2Vectors<kOrder>(q0, q2);
  SortLanesIn2Vectors<kOrder>(q1, q3);

  // Layers 2 to 4
  // Inputs are not fully sorted, so cannot use Merge2SortedQuartets.
  BitonicMerge2Quartets<kOrder>(d, q0, q1, __LINE__);
  BitonicMerge2Quartets<kOrder>(d, q2, q3, __LINE__);

#if HWY_SORT_VERIFY
  const verify::Runs<D> output =
      verify::StoreDeinterleavedQuartets(d, q0, q1, q2, q3);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// 72 ops.
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void BitonicMerge8Quartets(D d, V& q0, V& q1, V& q2, V& q3, V& q4,
                                      V& q5, V& q6, V& q7, int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input =
      verify::StoreDeinterleavedQuartets(d, q0, q1, q2, q3, q4, q5, q6, q7);
  if (caller == -1) input.CheckBitonic(__LINE__, __LINE__);
#endif

  // Layer 1: lane stride 16
  SortLanesIn2Vectors<kOrder>(q0, q4);
  SortLanesIn2Vectors<kOrder>(q1, q5);
  SortLanesIn2Vectors<kOrder>(q2, q6);
  SortLanesIn2Vectors<kOrder>(q3, q7);

  // Layers 2 to 5
  BitonicMerge4Quartets<kOrder>(d, q0, q1, q2, q3, __LINE__);
  BitonicMerge4Quartets<kOrder>(d, q4, q5, q6, q7, __LINE__);

#if HWY_SORT_VERIFY
  const verify::Runs<D> output =
      verify::StoreDeinterleavedQuartets(d, q0, q1, q2, q3, q4, q5, q6, q7);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// ------------------------------ Bitonic merge (vectors)

// Lane 0 with 4, 1 with 5 etc. Only used for vectors with at least 8 lanes.
#if HWY_TARGET <= HWY_AVX3

// TODO(janwas): move to op
template <typename T>
Vec512<T> Shuffle128_2020(Vec512<T> a, Vec512<T> b) {
  return Vec512<T>{_mm512_shuffle_i32x4(a.raw, b.raw, _MM_SHUFFLE(2, 0, 2, 0))};
}

template <typename T>
Vec512<T> Shuffle128_3131(Vec512<T> a, Vec512<T> b) {
  return Vec512<T>{_mm512_shuffle_i32x4(a.raw, b.raw, _MM_SHUFFLE(3, 1, 3, 1))};
}

template <typename T>
Vec512<T> Shuffle128_2301(Vec512<T> a, Vec512<T> b) {
  return Vec512<T>{_mm512_shuffle_i32x4(a.raw, b.raw, _MM_SHUFFLE(2, 3, 0, 1))};
}

template <typename T>
Vec512<T> OddEven128(Vec512<T> odd, Vec512<T> even) {
  return Vec512<T>{_mm512_mask_blend_epi64(__mmask8{0x33u}, odd.raw, even.raw)};
}

template <SortOrder kOrder, class T>
HWY_INLINE void SortDistance4LanesV(Simd<T, 16> d, Vec<decltype(d)>& v) {
  // In: FEDCBA98 76543210
  // Swap 128-bit halves of each 256 bits => BA98FEDC 32107654
  Vec512<T> swapped = Shuffle128_2301(v, v);
  SortLanesIn2Vectors<kOrder>(v, swapped);
  v = OddEven128(swapped, v);
}

#endif

template <SortOrder kOrder, typename T>
HWY_INLINE void SortDistance4LanesV(Simd<T, 8> d, Vec<decltype(d)>& v) {
  Vec<decltype(d)> swapped = ConcatLowerUpper(d, v, v);
  SortLanesIn2Vectors<kOrder>(v, swapped);
  v = ConcatUpperLower(swapped, v);
}

template <SortOrder kOrder, typename T>
HWY_INLINE void SortDistance4LanesV(Simd<T, 4> /* tag */, ...) {}

// Only used for vectors with at least 16 lanes.
template <SortOrder kOrder, class D>
HWY_INLINE void SortDistance8LanesV(D d, Vec<D>& v) {
  Vec<D> swapped = ConcatLowerUpper(d, v, v);
  SortLanesIn2Vectors<kOrder>(v, swapped);
  v = ConcatUpperLower(swapped, v);
}

// 120 ops. Only used if vectors are at least 8 lanes.
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void BitonicMergeTo64(D d, V& v0, V& v1, V& v2, V& v3, V& v4, V& v5,
                                 V& v6, V& v7, int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input =
      verify::StoreVectors(d, v0, v1, v2, v3, v4, v5, v6, v7);
  if (caller == -1) input.CheckBitonic(__LINE__, __LINE__);
#endif

  // Layer 1: lane stride 32
  SortLanesIn2Vectors<kOrder>(v0, v4);
  SortLanesIn2Vectors<kOrder>(v1, v5);
  SortLanesIn2Vectors<kOrder>(v2, v6);
  SortLanesIn2Vectors<kOrder>(v3, v7);

  // Layer 2: lane stride 16
  SortLanesIn2Vectors<kOrder>(v0, v2);
  SortLanesIn2Vectors<kOrder>(v1, v3);
  SortLanesIn2Vectors<kOrder>(v4, v6);
  SortLanesIn2Vectors<kOrder>(v5, v7);

  // Layer 3: lane stride 8
  SortLanesIn2Vectors<kOrder>(v0, v1);
  SortLanesIn2Vectors<kOrder>(v2, v3);
  SortLanesIn2Vectors<kOrder>(v4, v5);
  SortLanesIn2Vectors<kOrder>(v6, v7);

  // Layer 4: lane stride 4
  SortDistance4LanesV<kOrder>(d, v0);
  SortDistance4LanesV<kOrder>(d, v1);
  SortDistance4LanesV<kOrder>(d, v2);
  SortDistance4LanesV<kOrder>(d, v3);
  SortDistance4LanesV<kOrder>(d, v4);
  SortDistance4LanesV<kOrder>(d, v5);
  SortDistance4LanesV<kOrder>(d, v6);
  SortDistance4LanesV<kOrder>(d, v7);

  // Layer 5: lane stride 2
  SortDistance2LanesQV<kOrder>(d, v0);
  SortDistance2LanesQV<kOrder>(d, v1);
  SortDistance2LanesQV<kOrder>(d, v2);
  SortDistance2LanesQV<kOrder>(d, v3);
  SortDistance2LanesQV<kOrder>(d, v4);
  SortDistance2LanesQV<kOrder>(d, v5);
  SortDistance2LanesQV<kOrder>(d, v6);
  SortDistance2LanesQV<kOrder>(d, v7);

  // Layer 6: lane stride 1
  SortAdjacentLanesQV<kOrder>(d, v0);
  SortAdjacentLanesQV<kOrder>(d, v1);
  SortAdjacentLanesQV<kOrder>(d, v2);
  SortAdjacentLanesQV<kOrder>(d, v3);
  SortAdjacentLanesQV<kOrder>(d, v4);
  SortAdjacentLanesQV<kOrder>(d, v5);
  SortAdjacentLanesQV<kOrder>(d, v6);
  SortAdjacentLanesQV<kOrder>(d, v7);

#if HWY_SORT_VERIFY
  const verify::Runs<D> output =
      verify::StoreVectors(d, v0, v1, v2, v3, v4, v5, v6, v7);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// 60 ops. Only used if vectors are at least 16 lanes.
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void BitonicMergeTo64(D d, V& v0, V& v1, V& v2, V& v3, int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input = verify::StoreVectors(d, v0, v1, v2, v3);
  if (caller == -1) input.CheckBitonic(__LINE__, __LINE__);
#endif

  // Layer 1: lane stride 32
  SortLanesIn2Vectors<kOrder>(v0, v2);
  SortLanesIn2Vectors<kOrder>(v1, v3);

  // Layer 2: lane stride 16
  SortLanesIn2Vectors<kOrder>(v0, v1);
  SortLanesIn2Vectors<kOrder>(v2, v3);

  // Layer 3: lane stride 8
  SortDistance8LanesV<kOrder>(d, v0);
  SortDistance8LanesV<kOrder>(d, v1);
  SortDistance8LanesV<kOrder>(d, v2);
  SortDistance8LanesV<kOrder>(d, v3);

  // Layer 4: lane stride 4
  SortDistance4LanesV<kOrder>(d, v0);
  SortDistance4LanesV<kOrder>(d, v1);
  SortDistance4LanesV<kOrder>(d, v2);
  SortDistance4LanesV<kOrder>(d, v3);

  // Layer 5: lane stride 2
  SortDistance2LanesQV<kOrder>(d, v0);
  SortDistance2LanesQV<kOrder>(d, v1);
  SortDistance2LanesQV<kOrder>(d, v2);
  SortDistance2LanesQV<kOrder>(d, v3);

  // Layer 6: lane stride 1
  SortAdjacentLanesQV<kOrder>(d, v0);
  SortAdjacentLanesQV<kOrder>(d, v1);
  SortAdjacentLanesQV<kOrder>(d, v2);
  SortAdjacentLanesQV<kOrder>(d, v3);

#if HWY_SORT_VERIFY
  const verify::Runs<D> output = verify::StoreVectors(d, v0, v1, v2, v3);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// 128 ops. Only used if vectors are at least 16 lanes.
template <SortOrder kOrder, class D, class V = Vec<D>>
HWY_INLINE void BitonicMergeTo128(D d, V& v0, V& v1, V& v2, V& v3, V& v4, V& v5,
                                  V& v6, V& v7, int caller) {
#if HWY_SORT_VERIFY
  const verify::Runs<D> input =
      verify::StoreVectors(d, v0, v1, v2, v3, v4, v5, v6, v7);
  if (caller == -1) input.CheckBitonic(__LINE__, __LINE__);
#endif

  // Layer 1: lane stride 64
  SortLanesIn2Vectors<kOrder>(v0, v4);
  SortLanesIn2Vectors<kOrder>(v1, v5);
  SortLanesIn2Vectors<kOrder>(v2, v6);
  SortLanesIn2Vectors<kOrder>(v3, v7);

  BitonicMergeTo64<kOrder>(d, v0, v1, v2, v3, __LINE__);
  BitonicMergeTo64<kOrder>(d, v4, v5, v6, v7, __LINE__);

#if HWY_SORT_VERIFY
  const verify::Runs<D> output =
      verify::StoreVectors(d, v0, v1, v2, v3, v4, v5, v6, v7);
  output.CheckSorted(kOrder, __LINE__, caller);
#endif
}

// ------------------------------ Vector-length dependent

// Only called when N=4 (single block, so quartets can just be stored).
template <SortOrder kOrder, class D, class V>
HWY_API size_t SingleQuartetPerVector(D d, V& q0, V& q1, V& q2, V& q3, V& q4,
                                      V& q5, V& q6, V& q7, TFromD<D>* inout) {
  Store(q0, d, inout + 0 * 4);
  Store(q1, d, inout + 1 * 4);
  Store(q2, d, inout + 2 * 4);
  Store(q3, d, inout + 3 * 4);
  Store(q4, d, inout + 4 * 4);
  Store(q5, d, inout + 5 * 4);
  Store(q6, d, inout + 6 * 4);
  Store(q7, d, inout + 7 * 4);
  return 8 * 4;
}

// Only called when N=8.
template <SortOrder kOrder, class D, class V>
HWY_API size_t TwoQuartetsPerVector(D d, V& q0, V& q1, V& q2, V& q3, V& q4,
                                    V& q5, V& q6, V& q7, TFromD<D>* inout) {
  V v0 = ConcatLowerLower(d, q1, q0);
  V v1 = ConcatLowerLower(d, q3, q2);
  V v2 = ConcatLowerLower(d, q5, q4);
  V v3 = ConcatLowerLower(d, q7, q6);
  // TODO(janwas): merge into single table
  V v4 = Reverse(d, ConcatUpperUpper(d, q7, q6));
  V v5 = Reverse(d, ConcatUpperUpper(d, q5, q4));
  V v6 = Reverse(d, ConcatUpperUpper(d, q3, q2));
  V v7 = Reverse(d, ConcatUpperUpper(d, q1, q0));
  detail::BitonicMergeTo64<kOrder>(d, v0, v1, v2, v3, v4, v5, v6, v7, -1);

  Store(v0, d, inout + 0 * 8);
  Store(v1, d, inout + 1 * 8);
  Store(v2, d, inout + 2 * 8);
  Store(v3, d, inout + 3 * 8);
  Store(v4, d, inout + 4 * 8);
  Store(v5, d, inout + 5 * 8);
  Store(v6, d, inout + 6 * 8);
  Store(v7, d, inout + 7 * 8);
  return 8 * 8;
}

// Only called when N=16.
template <SortOrder kOrder, typename T, class V>
HWY_API size_t FourQuartetsPerVector(Simd<T, 16> d, V& q0, V& q1, V& q2, V& q3,
                                     V& q4, V& q5, V& q6, V& q7, T* inout) {
  const V q11_01_10_00 = Shuffle128_2020(q0, q1);
  const V q13_03_12_02 = Shuffle128_2020(q2, q3);
  V v0 = Shuffle128_2020(q11_01_10_00, q13_03_12_02);  // 3..0

  const V q15_05_14_04 = Shuffle128_2020(q4, q5);
  const V q17_07_16_06 = Shuffle128_2020(q6, q7);
  V v1 = Shuffle128_2020(q15_05_14_04, q17_07_16_06);  // 7..4

  const V q19_09_18_08 = Shuffle128_3131(q0, q1);
  const V q1b_0b_1a_0a = Shuffle128_3131(q2, q3);
  V v3 = Reverse(d, Shuffle128_2020(q19_09_18_08, q1b_0b_1a_0a));  // b..8

  const V q1d_0d_1c_0c = Shuffle128_3131(q4, q5);
  const V q1f_0f_1e_0e = Shuffle128_3131(q6, q7);
  V v2 = Reverse(d, Shuffle128_2020(q1d_0d_1c_0c, q1f_0f_1e_0e));  // f..c

  detail::BitonicMergeTo64<kOrder>(d, v0, v1, v2, v3, -1);

  // TODO(janwas): merge into single table
  V v4 = Shuffle128_3131(q11_01_10_00, q13_03_12_02);              // 13..10
  V v5 = Shuffle128_3131(q15_05_14_04, q17_07_16_06);              // 17..14
  V v7 = Reverse(d, Shuffle128_3131(q19_09_18_08, q1b_0b_1a_0a));  // 1b..18
  V v6 = Reverse(d, Shuffle128_3131(q1d_0d_1c_0c, q1f_0f_1e_0e));  // 1f..1c

  detail::BitonicMergeTo64<Reverse(kOrder)>(d, v4, v5, v6, v7, -1);

  detail::BitonicMergeTo128<kOrder>(d, v0, v1, v2, v3, v4, v5, v6, v7, -1);

  Store(v0, d, inout + 0 * 16);
  Store(v1, d, inout + 1 * 16);
  Store(v2, d, inout + 2 * 16);
  Store(v3, d, inout + 3 * 16);
  Store(v4, d, inout + 4 * 16);
  Store(v5, d, inout + 5 * 16);
  Store(v6, d, inout + 6 * 16);
  Store(v7, d, inout + 7 * 16);
  return 8 * 16;
}

// Avoid needing #if at the call sites.
template <SortOrder kOrder, typename T>
HWY_API size_t TwoQuartetsPerVector(Simd<T, 4> /* tag */, ...) {
  return 0;
}

template <SortOrder kOrder, typename T>
HWY_API size_t FourQuartetsPerVector(Simd<T, 4> /* tag */, ...) {
  return 0;
}
template <SortOrder kOrder, typename T>
HWY_API size_t FourQuartetsPerVector(Simd<T, 8> /* tag */, ...) {
  return 0;
}

}  // namespace detail

template <class D>
HWY_API size_t SortBatchSize(D d) {
  const size_t N = Lanes(d);
  if (N == 4) return 32;
  if (N == 8) return 64;
  if (N == 16) return 128;
  return 0;
}

template <SortOrder kOrder, class D>
HWY_API size_t SortBatch(D d, TFromD<D>* inout) {
  const size_t N = Lanes(d);

  Vec<D> q0, q1, q2, q3;
  detail::SortLanesIn4Vectors<kOrder>(d, inout, q0, q1, q2, q3);
  detail::Transpose4x4(d, q0, q1, q2, q3);
  detail::Merge2SortedQuartets<kOrder>(d, q0, q1, -1);
  detail::Merge2SortedQuartets<kOrder>(d, q2, q3, -1);

  // Bitonic merges require one input to be in reverse order.
  constexpr SortOrder kReverse = Reverse(kOrder);

  Vec<D> q4, q5, q6, q7;
  detail::SortLanesIn4Vectors<kReverse>(d, inout + 4 * N, q4, q5, q6, q7);
  detail::Transpose4x4(d, q4, q5, q6, q7);
  detail::Merge2SortedQuartets<kReverse>(d, q4, q5, -1);
  detail::Merge2SortedQuartets<kReverse>(d, q6, q7, -1);

  detail::BitonicMerge4Quartets<kOrder>(d, q0, q1, q4, q5, -1);
  detail::BitonicMerge4Quartets<kReverse>(d, q2, q3, q6, q7, -1);

  detail::BitonicMerge8Quartets<kOrder>(d, q0, q1, q4, q5, q2, q3, q6, q7,
                                        __LINE__);

  if (N == 4) {
    return detail::SingleQuartetPerVector<kOrder>(d, q0, q1, q4, q5, q2, q3, q6,
                                                  q7, inout);
  }

  if (N == 8) {
    return detail::TwoQuartetsPerVector<kOrder>(d, q0, q1, q4, q5, q2, q3, q6,
                                                q7, inout);
  }

  return detail::FourQuartetsPerVector<kOrder>(d, q0, q1, q4, q5, q2, q3, q6,
                                               q7, inout);
}

#else

// Avoids unused attribute warning
template <SortOrder kOrder, class D>
HWY_API size_t SortBatch(D /* tag */, TFromD<D>* /* inout */) {
  return 0;
}

#endif  // HWY_TARGET != HWY_SCALAR && HWY_ARCH_X86

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_SORT_SORT_INL_H_
