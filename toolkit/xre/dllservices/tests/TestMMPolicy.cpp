/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsDllInterceptor.h"

#include <functional>

mozilla::interceptor::MMPolicyInProcess gPolicy;

void DepleteVirtualAddress(
    uint8_t* aStart, size_t aSize,
    const std::function<void(void*)>& aPostAllocCallback) {
  const DWORD granularity = gPolicy.GetAllocGranularity();
  if (aStart == 0 || aSize < granularity) {
    return;
  }

  uint8_t* alignedStart = reinterpret_cast<uint8_t*>(
      (((reinterpret_cast<uintptr_t>(aStart) - 1) / granularity) + 1) *
      granularity);
  aSize -= (alignedStart - aStart);
  if (auto p = VirtualAlloc(alignedStart, aSize, MEM_RESERVE, PAGE_NOACCESS)) {
    aPostAllocCallback(p);
    return;
  }

  uintptr_t mask = ~(static_cast<uintptr_t>(granularity) - 1);
  size_t halfSize = (aSize >> 1) & mask;
  if (halfSize == 0) {
    return;
  }

  DepleteVirtualAddress(aStart, halfSize, aPostAllocCallback);
  DepleteVirtualAddress(aStart + halfSize, aSize - halfSize,
                        aPostAllocCallback);
}

bool ValidateFreeRegion(LPVOID aRegion, size_t aDesiredLen) {
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(aRegion, &mbi, sizeof(mbi)) != sizeof(mbi)) {
    printf(
        "TEST-FAILED | TestMMPolicy | "
        "VirtualQuery(%p) failed - %08lx\n",
        aRegion, GetLastError());
    return false;
  }

  if (mbi.State != MEM_FREE) {
    printf(
        "TEST-FAILED | TestMMPolicy | "
        "%p is not within a free region\n",
        aRegion);
    return false;
  }

  if (aRegion != mbi.BaseAddress ||
      reinterpret_cast<uintptr_t>(mbi.BaseAddress) %
          gPolicy.GetAllocGranularity()) {
    printf(
        "TEST-FAILED | TestMMPolicy | "
        "%p is not a region's start address\n",
        aRegion);
    return false;
  }

  LPVOID allocated = VirtualAlloc(aRegion, aDesiredLen,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!allocated) {
    printf(
        "TEST-FAILED | TestMMPolicy | "
        "VirtualAlloc(%p) failed - %08lx\n",
        aRegion, GetLastError());
    return false;
  }

  if (!VirtualFree(allocated, 0, MEM_RELEASE)) {
    printf(
        "TEST-FAILED | TestMMPolicy | "
        "VirtualFree(%p) failed - %08lx\n",
        allocated, GetLastError());
    return false;
  }

  return true;
}

bool TestFindRegion() {
  // Skip the near-null addresses
  uint8_t* minAddr = reinterpret_cast<uint8_t*>(
      std::max(gPolicy.GetAllocGranularity(), 0x1000000ul));
  // 64bit address space is too large to deplete.  32bit space is enough.
  uint8_t* maxAddr = reinterpret_cast<uint8_t*>(std::min(
      gPolicy.GetMaxUserModeAddress(), static_cast<uintptr_t>(0xffffffff)));

  // Keep one of the regions we allocate so that we can release it later.
  void* lastResort = nullptr;

  // Reserve all free regions in the range [minAddr, maxAddr]
  for (uint8_t* address = minAddr; address <= maxAddr;) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi)) {
      printf(
          "TEST-FAILED | TestMMPolicy | "
          "VirtualQuery(%p) failed - %08lx\n",
          address, GetLastError());
      break;
    }

    address = reinterpret_cast<uint8_t*>(mbi.BaseAddress);
    if (mbi.State == MEM_FREE) {
      DepleteVirtualAddress(address, mbi.RegionSize,
                            [&lastResort](void* aAllocated) {
                              // Pick the first address we allocate to make sure
                              // FindRegion scans the full range.
                              if (!lastResort) {
                                lastResort = aAllocated;
                              }
                            });
    }

    address += mbi.RegionSize;
  }

  if (!lastResort) {
    printf(
        "TEST-SKIPPED | TestMMPolicy | "
        "No free region in [%p - %p].  Skipping the testcase.\n",
        minAddr, maxAddr);
    return true;
  }

  // Make sure there are no free regions
  PVOID freeRegion =
      gPolicy.FindRegion(GetCurrentProcess(), 1, minAddr, maxAddr);
  if (freeRegion) {
    if (reinterpret_cast<uintptr_t>(freeRegion) %
        gPolicy.GetAllocGranularity()) {
      printf(
          "TEST-FAILED | TestMMPolicy | "
          "MMPolicyBase::FindRegion returned an unaligned address %p.\n",
          freeRegion);
      return false;
    }

    printf(
        "TEST-SKIPPED | TestMMPolicy | "
        "%p was freed after depletion.  Skipping the testcase.\n",
        freeRegion);
    return true;
  }

  // Free one region, and thus we can expect FindRegion finds this region
  if (!VirtualFree(lastResort, 0, MEM_RELEASE)) {
    printf(
        "TEST-FAILED | TestMMPolicy | "
        "VirtualFree(%p) failed - %08lx\n",
        lastResort, GetLastError());
    return false;
  }
  printf("The region starting from %p has been freed.\n", lastResort);

  // Run the function several times because it uses a randon number inside
  // and its result is nondeterministic.
  for (int i = 0; i < 50; ++i) {
    // Because one region was freed, a desire up to one region
    // should be fulfilled.
    const size_t desiredLengths[] = {1, gPolicy.GetAllocGranularity()};

    for (auto desiredLen : desiredLengths) {
      freeRegion =
          gPolicy.FindRegion(GetCurrentProcess(), desiredLen, minAddr, maxAddr);
      if (!freeRegion) {
        printf(
            "TEST-FAILED | TestMMPolicy | "
            "Failed to find a free region.\n");
        return false;
      }

      if (!ValidateFreeRegion(freeRegion, desiredLen)) {
        return false;
      }
    }
  }

  return true;
}

extern "C" int wmain(int argc, wchar_t* argv[]) {
  if (!TestFindRegion()) {
    return 1;
  }

  printf("TEST-PASS | TestMMPolicy | All tests passed.\n");
  return 0;
}
