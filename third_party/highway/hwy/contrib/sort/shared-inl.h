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

// Definitions shared between vqsort-inl and sorting_networks-inl.

// Normal include guard for target-independent parts
#ifndef HIGHWAY_HWY_CONTRIB_SORT_SHARED_INL_H_
#define HIGHWAY_HWY_CONTRIB_SORT_SHARED_INL_H_

#include "hwy/base.h"

namespace hwy {

// Internal constants - these are to avoid magic numbers/literals and cannot be
// changed without also changing the associated code.
struct SortConstants {
// SortingNetwork reshapes its input into a matrix. This is the maximum number
// of *keys* per vector.
#if HWY_COMPILER_MSVC
  static constexpr size_t kMaxCols = 8;  // avoids build timeout
#else
  static constexpr size_t kMaxCols = 16;  // enough for u32 in 512-bit vector
#endif

  // 16 rows is a compromise between using the 32 AVX-512/SVE/RVV registers,
  // fitting within 16 AVX2 registers with only a few spills, keeping BaseCase
  // code size reasonable (7 KiB for AVX-512 and 16 cols), and minimizing the
  // extra logN factor for larger networks (for which only loose upper bounds
  // on size are known).
  static constexpr size_t kMaxRowsLog2 = 4;
  static constexpr size_t kMaxRows = size_t{1} << kMaxRowsLog2;

  static HWY_INLINE size_t BaseCaseNum(size_t N) {
    return kMaxRows * HWY_MIN(N, kMaxCols);
  }

  // Unrolling is important (pipelining and amortizing branch mispredictions);
  // 2x is sufficient to reach full memory bandwidth on SKX in Partition, but
  // somewhat slower for sorting than 4x.
  //
  // To change, must also update left + 3 * N etc. in the loop.
  static constexpr size_t kPartitionUnroll = 4;

  static HWY_INLINE size_t PartitionBufNum(size_t N) {
    // The main loop reads kPartitionUnroll vectors, and first loads from
    // both left and right beforehand, so it requires min = 2 *
    // kPartitionUnroll vectors. To handle smaller amounts (only guaranteed
    // >= BaseCaseNum), we partition the right side into a buffer. We need
    // another vector at the end so CompressStore does not overwrite anything.
    return (2 * kPartitionUnroll + 1) * N;
  }

  // Chunk := group of keys loaded for sampling a pivot. Matches the typical
  // cache line size of 64 bytes to get maximum benefit per L2 miss. If vectors
  // are larger, use entire vectors to ensure we do not overrun the array.
  static HWY_INLINE size_t LanesPerChunk(size_t sizeof_t, size_t N) {
    return HWY_MAX(64 / sizeof_t, N);
  }
};

}  // namespace hwy

#endif  // HIGHWAY_HWY_CONTRIB_SORT_SHARED_INL_H_

// Per-target
#if defined(HIGHWAY_HWY_CONTRIB_SORT_SHARED_TOGGLE) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_SHARED_TOGGLE
#undef HIGHWAY_HWY_CONTRIB_SORT_SHARED_TOGGLE
#else
#define HIGHWAY_HWY_CONTRIB_SORT_SHARED_TOGGLE
#endif

#include "hwy/highway.h"

namespace hwy {
namespace HWY_NAMESPACE {

// Default tag / vector width selector.
// TODO(janwas): enable once LMUL < 1 is supported.
#if HWY_TARGET == HWY_RVV && 0
template <typename T>
using SortTag = ScalableTag<T, -1>;
#else
template <typename T>
using SortTag = ScalableTag<T>;
#endif

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy

#endif  // HIGHWAY_HWY_CONTRIB_SORT_SHARED_TOGGLE
