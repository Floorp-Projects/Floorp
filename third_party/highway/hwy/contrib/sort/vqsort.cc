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

#include "hwy/contrib/sort/vqsort.h"

#include <string.h>  // memset

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep

// After foreach_target
#include "hwy/contrib/sort/shared-inl.h"

// Architectures for which we know HWY_HAVE_SCALABLE == 0. This opts into an
// optimization that replaces dynamic allocation with stack storage.
#ifndef VQSORT_STACK
#if HWY_ARCH_X86 || HWY_ARCH_WASM
#define VQSORT_STACK 1
#else
#define VQSORT_STACK 0
#endif
#endif  // VQSORT_STACK

#if !VQSORT_STACK
#include "hwy/aligned_allocator.h"
#endif

// Check if we have sys/random.h. First skip some systems on which the check
// itself (features.h) might be problematic.
#if defined(ANDROID) || defined(__ANDROID__) || HWY_ARCH_RVV
#define VQSORT_GETRANDOM 0
#endif

#if !defined(VQSORT_GETRANDOM) && HWY_OS_LINUX
#include <features.h>

// ---- which libc
#if defined(__UCLIBC__)
#define VQSORT_GETRANDOM 1  // added Mar 2015, before uclibc-ng 1.0

#elif defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 25)
#define VQSORT_GETRANDOM 1
#else
#define VQSORT_GETRANDOM 0
#endif

#else
// Assume MUSL, which has getrandom since 2018. There is no macro to test, see
// https://www.openwall.com/lists/musl/2013/03/29/13.
#define VQSORT_GETRANDOM 1

#endif  // ---- which libc
#endif  // linux

#if !defined(VQSORT_GETRANDOM)
#define VQSORT_GETRANDOM 0
#endif

// Seed source for SFC generator: 1=getrandom, 2=CryptGenRandom
// (not all Android support the getrandom wrapper)
#ifndef VQSORT_SECURE_SEED

#if VQSORT_GETRANDOM
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
#pragma comment(lib, "advapi32.lib")
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

}  // namespace

Sorter::Sorter() {
#if VQSORT_STACK
  ptr_ = nullptr;  // Sort will use stack storage instead
#else
  // Determine the largest buffer size required for any type by trying them all.
  // (The capping of N in BaseCaseNum means that smaller N but larger sizeof_t
  // may require a larger buffer.)
  const size_t vector_size = HWY_DYNAMIC_DISPATCH(VectorSize)();
  const size_t max_bytes =
      HWY_MAX(HWY_MAX(SortConstants::BufBytes<uint16_t>(vector_size),
                      SortConstants::BufBytes<uint32_t>(vector_size)),
              SortConstants::BufBytes<uint64_t>(vector_size));
  ptr_ = hwy::AllocateAlignedBytes(max_bytes, nullptr, nullptr);

  // Prevent msan errors by initializing.
  memset(ptr_, 0, max_bytes);
#endif
}

void Sorter::Delete() {
#if !VQSORT_STACK
  FreeAlignedBytes(ptr_, nullptr, nullptr);
  ptr_ = nullptr;
#endif
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
