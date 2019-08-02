/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozmemory.h"
#include "replace_malloc_bridge.h"
#include "mozilla/Assertions.h"
#include "../../PHC.h"

using namespace mozilla;

bool PHCInfoEq(phc::AddrInfo& aInfo, phc::AddrInfo::Kind aKind, void* aBaseAddr,
               size_t aUsableSize, bool aHasAllocStack, bool aHasFreeStack) {
  return aInfo.mKind == aKind && aInfo.mBaseAddr == aBaseAddr &&
         aInfo.mUsableSize == aUsableSize &&
         // Proper stack traces will have at least 3 elements.
         (aHasAllocStack ? (aInfo.mAllocStack->mLength > 2)
                         : (aInfo.mAllocStack.isNothing())) &&
         (aHasFreeStack ? (aInfo.mFreeStack->mLength > 2)
                        : (aInfo.mFreeStack.isNothing()));
}

bool JeInfoEq(jemalloc_ptr_info_t& aInfo, PtrInfoTag aTag, void* aAddr,
              size_t aSize, arena_id_t arenaId) {
  return aInfo.tag == aTag && aInfo.addr == aAddr && aInfo.size == aSize
#ifdef MOZ_DEBUG
         && aInfo.arenaId == arenaId
#endif
      ;
}

char* GetPHCAllocation(size_t aSize) {
  // A crude but effective way to get a PHC allocation.
  for (int i = 0; i < 2000000; i++) {
    char* p = (char*)malloc(aSize);
    if (ReplaceMalloc::IsPHCAllocation(p, nullptr)) {
      return p;
    }
    free(p);
  }
  return nullptr;
}

TEST(PHC, TestPHCBasics)
{
  int stackVar;
  phc::AddrInfo phcInfo;
  jemalloc_ptr_info_t jeInfo;

  // Test a default AddrInfo.
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::Unknown, nullptr, 0ul,
                        false, false));

  // Test some non-PHC allocation addresses.
  ASSERT_FALSE(ReplaceMalloc::IsPHCAllocation(nullptr, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::Unknown, nullptr, 0,
                        false, false));
  ASSERT_FALSE(ReplaceMalloc::IsPHCAllocation(&stackVar, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::Unknown, nullptr, 0,
                        false, false));

  char* p = GetPHCAllocation(32);
  if (!p) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  // Test an in-use PHC allocation, via its base address.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  ASSERT_EQ(moz_malloc_usable_size(p), 32ul);
  jemalloc_ptr_info(p, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagLiveAlloc, p, 32, 0));

  // Test an in-use PHC allocation, via an address in its middle.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 10, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  ASSERT_EQ(moz_malloc_usable_size(p), 32ul);
  jemalloc_ptr_info(p + 10, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagLiveAlloc, p, 32, 0));

  // Test an in-use PHC allocation, via an address past its end. The results
  // for phcInfo should be the same, but be different for jeInfo.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 64, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  jemalloc_ptr_info(p + 64, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  free(p);

  // Test a freed PHC allocation, via its base address.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagFreedAlloc, p, 32, 0));

  // Test a freed PHC allocation, via an address in its middle.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 10, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 10, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagFreedAlloc, p, 32, 0));

  // Test a freed PHC allocation, via an address past its end.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 10, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 64, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // There are no tests for `mKind == NeverAllocatedPage` because it's not
  // possible to reliably get ahold of such a page.

  // There are not tests for `mKind == GuardPage` because it's currently not
  // implemented.
}

TEST(PHC, TestPHCDisabling)
{
  char* p = GetPHCAllocation(32);
  char* q = GetPHCAllocation(32);
  if (!p || !q) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  ASSERT_TRUE(ReplaceMalloc::IsPHCEnabledOnCurrentThread());
  ReplaceMalloc::DisablePHCOnCurrentThread();
  ASSERT_FALSE(ReplaceMalloc::IsPHCEnabledOnCurrentThread());

  // Test realloc() on a PHC allocation while PHC is disabled on the thread.
  char* p2 = (char*)realloc(p, 128);
  // The small realloc is in-place.
  ASSERT_TRUE(p2 == p);
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p2, nullptr));
  char* p3 = (char*)realloc(p2, 8192);
  // The big realloc is not in-place, and the result is not a PHC allocation.
  ASSERT_TRUE(p3 != p2);
  ASSERT_FALSE(ReplaceMalloc::IsPHCAllocation(p3, nullptr));
  free(p3);

  // Test free() on a PHC allocation while PHC is disabled on the thread.
  free(q);

  // These must not be PHC allocations.
  char* r = GetPHCAllocation(32);  // This will fail.
  ASSERT_FALSE(!!r);

  ReplaceMalloc::ReenablePHCOnCurrentThread();
  ASSERT_TRUE(ReplaceMalloc::IsPHCEnabledOnCurrentThread());
}
