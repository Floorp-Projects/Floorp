/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozmemory.h"
#include "replace_malloc_bridge.h"
#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
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

uint8_t* GetPHCAllocation(size_t aSize, size_t aAlignment = 1) {
  // A crude but effective way to get a PHC allocation.
  for (int i = 0; i < 2000000; i++) {
    void* p = (aAlignment == 1) ? moz_xmalloc(aSize)
                                : moz_xmemalign(aAlignment, aSize);
    if (ReplaceMalloc::IsPHCAllocation(p, nullptr)) {
      return (uint8_t*)p;
    }
    free(p);
  }
  return nullptr;
}

static const size_t kPageSize = 4096;

TEST(PHC, TestPHCAllocations)
{
  // First, check that allocations of various sizes all get put at the end of
  // their page as expected. Also, check their sizes are as expected.

#define ASSERT_POS(n1, n2)                                      \
  p = (uint8_t*)moz_xrealloc(p, (n1));                          \
  ASSERT_EQ((reinterpret_cast<uintptr_t>(p) & (kPageSize - 1)), \
            kPageSize - (n2));                                  \
  ASSERT_EQ(moz_malloc_usable_size(p), (n2));

  uint8_t* p = GetPHCAllocation(1);
  if (!p) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  // On Win64 the smallest possible allocation is 16 bytes. On other platforms
  // it is 8 bytes.
#if defined(XP_WIN) && defined(HAVE_64BIT_BUILD)
  ASSERT_POS(8U, 16U);
#else
  ASSERT_POS(8U, 8U);
#endif
  ASSERT_POS(16U, 16U);
  ASSERT_POS(32U, 32U);
  ASSERT_POS(64U, 64U);
  ASSERT_POS(128U, 128U);
  ASSERT_POS(256U, 256U);
  ASSERT_POS(512U, 512U);
  ASSERT_POS(1024U, 1024U);
  ASSERT_POS(2048U, 2048U);
  ASSERT_POS(4096U, 4096U);

  free(p);

#undef ASSERT_POS

  // Second, do similar checking with allocations of various alignments. Also
  // check that their sizes (which are different to allocations with normal
  // alignment) are the same as the sizes of equivalent non-PHC allocations.

#define ASSERT_ALIGN(a1, a2)                                    \
  p = (uint8_t*)GetPHCAllocation(8, (a1));                      \
  ASSERT_EQ((reinterpret_cast<uintptr_t>(p) & (kPageSize - 1)), \
            kPageSize - (a2));                                  \
  ASSERT_EQ(moz_malloc_usable_size(p), (a2));                   \
  free(p);                                                      \
  p = (uint8_t*)moz_xmemalign((a1), 8);                         \
  ASSERT_EQ(moz_malloc_usable_size(p), (a2));                   \
  free(p);

  // On Win64 the smallest possible allocation is 16 bytes. On other platforms
  // it is 8 bytes.
#if defined(XP_WIN) && defined(HAVE_64BIT_BUILD)
  ASSERT_ALIGN(8U, 16U);
#else
  ASSERT_ALIGN(8U, 8U);
#endif
  ASSERT_ALIGN(16U, 16U);
  ASSERT_ALIGN(32U, 32U);
  ASSERT_ALIGN(64U, 64U);
  ASSERT_ALIGN(128U, 128U);
  ASSERT_ALIGN(256U, 256U);
  ASSERT_ALIGN(512U, 512U);
  ASSERT_ALIGN(1024U, 1024U);
  ASSERT_ALIGN(2048U, 2048U);
  ASSERT_ALIGN(4096U, 4096U);

#undef ASSERT_ALIGN
}

TEST(PHC, TestPHCInfo)
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

  uint8_t* p = GetPHCAllocation(32);
  if (!p) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  // Test an in-use PHC allocation: first byte within it.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  ASSERT_EQ(moz_malloc_usable_size(p), 32ul);
  jemalloc_ptr_info(p, &jeInfo);

  // Test an in-use PHC allocation: last byte within it.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 31, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  ASSERT_TRUE(JeInfoEq(jeInfo, TagLiveAlloc, p, 32, 0));
  jemalloc_ptr_info(p + 31, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagLiveAlloc, p, 32, 0));

  // Test an in-use PHC allocation: last byte before it.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p - 1, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  jemalloc_ptr_info(p - 1, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: first byte on its allocation page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 32 - kPageSize, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, p, 32ul, true, false));
  jemalloc_ptr_info(p + 32 - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: first byte in the following guard page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 32, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, false));
  jemalloc_ptr_info(p + 32, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: last byte in the lower half of the
  // following guard page.
  ASSERT_TRUE(
      ReplaceMalloc::IsPHCAllocation(p + 32 + (kPageSize / 2 - 1), &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, false));
  jemalloc_ptr_info(p + 32 + (kPageSize / 2 - 1), &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: last byte in the preceding guard page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 31 - kPageSize, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, false));
  jemalloc_ptr_info(p + 31 - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: first byte in the upper half of the
  // preceding guard page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(
      p + 31 - kPageSize - (kPageSize / 2 - 1), &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, false));
  jemalloc_ptr_info(p + 31 - kPageSize - (kPageSize / 2 - 1), &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  free(p);

  // Test a freed PHC allocation: first byte within it.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagFreedAlloc, p, 32, 0));

  // Test a freed PHC allocation: last byte within it.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 31, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 31, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagFreedAlloc, p, 32, 0));

  // Test a freed PHC allocation: last byte before it.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p - 1, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p - 1, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: first byte on its allocation page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 32 - kPageSize, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 32 - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: first byte in the following guard page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 32, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 32, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: last byte in the lower half of the following
  // guard page.
  ASSERT_TRUE(
      ReplaceMalloc::IsPHCAllocation(p + 32 + (kPageSize / 2 - 1), &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 32 + (kPageSize / 2 - 1), &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: last byte in the preceding guard page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p + 31 - kPageSize, &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 31 - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: first byte in the upper half of the preceding
  // guard page.
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(
      p + 31 - kPageSize - (kPageSize / 2 - 1), &phcInfo));
  ASSERT_TRUE(
      PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, p, 32ul, true, true));
  jemalloc_ptr_info(p + 31 - kPageSize - (kPageSize / 2 - 1), &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // There are no tests for `mKind == NeverAllocatedPage` because it's not
  // possible to reliably get ahold of such a page.
}

TEST(PHC, TestPHCDisabling)
{
  uint8_t* p = GetPHCAllocation(32);
  uint8_t* q = GetPHCAllocation(32);
  if (!p || !q) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  ASSERT_TRUE(ReplaceMalloc::IsPHCEnabledOnCurrentThread());
  ReplaceMalloc::DisablePHCOnCurrentThread();
  ASSERT_FALSE(ReplaceMalloc::IsPHCEnabledOnCurrentThread());

  // Test realloc() on a PHC allocation while PHC is disabled on the thread.
  uint8_t* p2 = (uint8_t*)realloc(p, 128);
  // The small realloc is fulfilled within the same page, but it does move.
  ASSERT_TRUE(p2 == p - 96);
  ASSERT_TRUE(ReplaceMalloc::IsPHCAllocation(p2, nullptr));
  uint8_t* p3 = (uint8_t*)realloc(p2, 8192);
  // The big realloc is not in-place, and the result is not a PHC allocation.
  ASSERT_TRUE(p3 != p2);
  ASSERT_FALSE(ReplaceMalloc::IsPHCAllocation(p3, nullptr));
  free(p3);

  // Test free() on a PHC allocation while PHC is disabled on the thread.
  free(q);

  // These must not be PHC allocations.
  uint8_t* r = GetPHCAllocation(32);  // This will fail.
  ASSERT_FALSE(!!r);

  ReplaceMalloc::ReenablePHCOnCurrentThread();
  ASSERT_TRUE(ReplaceMalloc::IsPHCEnabledOnCurrentThread());
}
