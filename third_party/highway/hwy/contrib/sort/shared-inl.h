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
#if HWY_COMPILER_MSVC || HWY_IS_DEBUG_BUILD
  static constexpr size_t kMaxCols = 8;  // avoid build timeout/stack overflow
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

  static constexpr HWY_INLINE size_t BaseCaseNum(size_t N) {
    return kMaxRows * HWY_MIN(N, kMaxCols);
  }

  // Unrolling is important (pipelining and amortizing branch mispredictions);
  // 2x is sufficient to reach full memory bandwidth on SKX in Partition, but
  // somewhat slower for sorting than 4x.
  //
  // To change, must also update left + 3 * N etc. in the loop.
  static constexpr size_t kPartitionUnroll = 4;

  static constexpr HWY_INLINE size_t PartitionBufNum(size_t N) {
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
  static constexpr HWY_INLINE size_t LanesPerChunk(size_t sizeof_t, size_t N) {
    return HWY_MAX(64 / sizeof_t, N);
  }

  static constexpr HWY_INLINE size_t PivotBufNum(size_t sizeof_t, size_t N) {
    // 3 chunks of medians, 1 chunk of median medians plus two padding vectors.
    return (3 + 1) * LanesPerChunk(sizeof_t, N) + 2 * N;
  }

  template <typename T>
  static constexpr HWY_INLINE size_t BufNum(size_t N) {
    // One extra for padding plus another for full-vector loads.
    return HWY_MAX(BaseCaseNum(N) + 2 * N,
                   HWY_MAX(PartitionBufNum(N), PivotBufNum(sizeof(T), N)));
  }

  template <typename T>
  static constexpr HWY_INLINE size_t BufBytes(size_t vector_size) {
    return sizeof(T) * BufNum<T>(vector_size / sizeof(T));
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

// vqsort isn't available on HWY_SCALAR, and builds time out on MSVC opt and
// Arm v7 debug.
#undef VQSORT_ENABLED
#if (HWY_TARGET == HWY_SCALAR) ||                 \
    (HWY_COMPILER_MSVC && !HWY_IS_DEBUG_BUILD) || \
    (HWY_ARCH_ARM_V7 && HWY_IS_DEBUG_BUILD)
#define VQSORT_ENABLED 0
#else
#define VQSORT_ENABLED 1
#endif

namespace hwy {
namespace HWY_NAMESPACE {

// Default tag / vector width selector.
#if HWY_TARGET == HWY_RVV
// Use LMUL = 1/2; for SEW=64 this ends up emulated via vsetvl.
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
