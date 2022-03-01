// Copyright 2022 Google LLC
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

// Interface to vectorized quicksort with dynamic dispatch.

#ifndef HIGHWAY_HWY_CONTRIB_SORT_VQSORT_H_
#define HIGHWAY_HWY_CONTRIB_SORT_VQSORT_H_

#include "hwy/base.h"

namespace hwy {

// Aligned 128-bit type. Cannot use __int128 because clang doesn't yet align it:
// https://reviews.llvm.org/D86310
#pragma pack(push, 1)
struct alignas(16) uint128_t {
  uint64_t lo;  // little-endian layout
  uint64_t hi;
};
#pragma pack(pop)

// Tag arguments that determine the sort order.
struct SortAscending {
  constexpr bool IsAscending() const { return true; }
};
struct SortDescending {
  constexpr bool IsAscending() const { return false; }
};

// Allocates O(1) space. Type-erased RAII wrapper over hwy/aligned_allocator.h.
// This allows amortizing the allocation over multiple sorts.
class HWY_CONTRIB_DLLEXPORT Sorter {
 public:
  Sorter();
  ~Sorter() { Delete(); }

  // Move-only
  Sorter(const Sorter&) = delete;
  Sorter& operator=(const Sorter&) = delete;
  Sorter(Sorter&& other) {
    Delete();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
  }
  Sorter& operator=(Sorter&& other) {
    Delete();
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  // Sorts keys[0, n). Dispatches to the best available instruction set,
  // and does not allocate memory.
  void operator()(uint16_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(uint16_t* HWY_RESTRICT keys, size_t n, SortDescending) const;
  void operator()(uint32_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(uint32_t* HWY_RESTRICT keys, size_t n, SortDescending) const;
  void operator()(uint64_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(uint64_t* HWY_RESTRICT keys, size_t n, SortDescending) const;

  void operator()(int16_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(int16_t* HWY_RESTRICT keys, size_t n, SortDescending) const;
  void operator()(int32_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(int32_t* HWY_RESTRICT keys, size_t n, SortDescending) const;
  void operator()(int64_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(int64_t* HWY_RESTRICT keys, size_t n, SortDescending) const;

  void operator()(float* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(float* HWY_RESTRICT keys, size_t n, SortDescending) const;
  void operator()(double* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(double* HWY_RESTRICT keys, size_t n, SortDescending) const;

  void operator()(uint128_t* HWY_RESTRICT keys, size_t n, SortAscending) const;
  void operator()(uint128_t* HWY_RESTRICT keys, size_t n, SortDescending) const;

  // For internal use only
  static void Fill24Bytes(const void* seed_heap, size_t seed_num, void* bytes);
  static bool HaveFloat64();

 private:
  void Delete();

  template <typename T>
  T* Get() const {
    return static_cast<T*>(ptr_);
  }

  void* ptr_ = nullptr;
};

}  // namespace hwy

#endif  // HIGHWAY_HWY_CONTRIB_SORT_VQSORT_H_
