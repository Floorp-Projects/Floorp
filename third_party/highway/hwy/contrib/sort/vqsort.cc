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

#include "hwy/contrib/sort/vqsort.h"

#include <string.h>  // memset

#include "hwy/aligned_allocator.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort.cc"
#include "hwy/foreach_target.h"

// After foreach_target
#include "hwy/contrib/sort/shared-inl.h"

// Seed source for SFC generator: 1=getrandom, 2=CryptGenRandom
// (not all Android support the getrandom wrapper)
#ifndef VQSORT_SECURE_SEED

#if (defined(linux) || defined(__linux__)) && \
    !(defined(ANDROID) || defined(__ANDROID__) || HWY_ARCH_RVV)
#define VQSORT_SECURE_SEED 1
#elif defined(_WIN32) || defined(_WIN64)
#define VQSORT_SECURE_SEED 2
#else
#define VQSORT_SECURE_SEED 0
#endif

#endif  // VQSORT_SECURE_SEED

#if !VQSORT_SECURE_RNG

#include <time.h>
#if VQSORT_SECURE_SEED == 1
#include <sys/random.h>
#elif VQSORT_SECURE_SEED == 2
#include <windows.h>
#pragma comment(lib, "Advapi32.lib")
// Must come after windows.h.
#include <wincrypt.h>
#endif  // VQSORT_SECURE_SEED

#endif  // !VQSORT_SECURE_RNG

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

size_t VectorSize() { return Lanes(ScalableTag<uint8_t, 3>()); }
bool HaveFloat64() { return HWY_HAVE_FLOAT64; }

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(VectorSize);
HWY_EXPORT(HaveFloat64);

HWY_INLINE size_t PivotBufNum(size_t sizeof_t, size_t N) {
  // 3 chunks of medians, 1 chunk of median medians plus two padding vectors.
  const size_t lpc = SortConstants::LanesPerChunk(sizeof_t, N);
  return (3 + 1) * lpc + 2 * N;
}

}  // namespace

Sorter::Sorter() {
  // Determine the largest buffer size required for any type by trying them all.
  // (The capping of N in BaseCaseNum means that smaller N but larger sizeof_t
  // may require a larger buffer.)
  const size_t vector_size = HWY_DYNAMIC_DISPATCH(VectorSize)();
  size_t max_bytes = 0;
  for (size_t sizeof_t :
       {sizeof(uint16_t), sizeof(uint32_t), sizeof(uint64_t)}) {
    const size_t N = vector_size / sizeof_t;
    // One extra for padding plus another for full-vector loads.
    const size_t base_case = SortConstants::BaseCaseNum(N) + 2 * N;
    const size_t partition_num = SortConstants::PartitionBufNum(N);
    const size_t buf_lanes =
        HWY_MAX(base_case, HWY_MAX(partition_num, PivotBufNum(sizeof_t, N)));
    max_bytes = HWY_MAX(max_bytes, buf_lanes * sizeof_t);
  }

  ptr_ = hwy::AllocateAlignedBytes(max_bytes, nullptr, nullptr);

  // Prevent msan errors by initializing.
  memset(ptr_, 0, max_bytes);
}

void Sorter::Delete() {
  FreeAlignedBytes(ptr_, nullptr, nullptr);
  ptr_ = nullptr;
}

#if !VQSORT_SECURE_RNG

void Sorter::Fill24Bytes(const void* seed_heap, size_t seed_num, void* bytes) {
#if VQSORT_SECURE_SEED == 1
  // May block if urandom is not yet initialized.
  const ssize_t ret = getrandom(bytes, 24, /*flags=*/0);
  if (ret == 24) return;
#elif VQSORT_SECURE_SEED == 2
  HCRYPTPROV hProvider{};
  if (CryptAcquireContextA(&hProvider, nullptr, nullptr, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
    const BOOL ok =
        CryptGenRandom(hProvider, 24, reinterpret_cast<BYTE*>(bytes));
    CryptReleaseContext(hProvider, 0);
    if (ok) return;
  }
#endif

  // VQSORT_SECURE_SEED == 0, or one of the above failed. Get some entropy from
  // stack/heap/code addresses and the clock() timer.
  uint64_t* words = reinterpret_cast<uint64_t*>(bytes);
  uint64_t** seed_stack = &words;
  void (*seed_code)(const void*, size_t, void*) = &Fill24Bytes;
  const uintptr_t bits_stack = reinterpret_cast<uintptr_t>(seed_stack);
  const uintptr_t bits_heap = reinterpret_cast<uintptr_t>(seed_heap);
  const uintptr_t bits_code = reinterpret_cast<uintptr_t>(seed_code);
  const uint64_t bits_time = static_cast<uint64_t>(clock());
  words[0] = bits_stack ^ bits_time ^ seed_num;
  words[1] = bits_heap ^ bits_time ^ seed_num;
  words[2] = bits_code ^ bits_time ^ seed_num;
}

#endif  // !VQSORT_SECURE_RNG

bool Sorter::HaveFloat64() { return HWY_DYNAMIC_DISPATCH(HaveFloat64)(); }

}  // namespace hwy
#endif  // HWY_ONCE
