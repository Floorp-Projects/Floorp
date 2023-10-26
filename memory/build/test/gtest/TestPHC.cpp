/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozmemory.h"
#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include "PHC.h"

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
    if (mozilla::phc::IsPHCAllocation(p, nullptr)) {
      return (uint8_t*)p;
    }
    free(p);
  }
  return nullptr;
}

#if defined(XP_DARWIN) && defined(__aarch64__)
static const size_t kPageSize = 16384;
#else
static const size_t kPageSize = 4096;
#endif

TEST(PHC, TestPHCAllocations)
{
  mozilla::phc::SetPHCState(phc::PHCState::Enabled);

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
  for (unsigned i = 16; i <= kPageSize; i *= 2) {
    ASSERT_POS(i, i);
  }

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
  for (unsigned i = 16; i <= kPageSize; i *= 2) {
    ASSERT_ALIGN(i, i);
  }

#undef ASSERT_ALIGN
}

static void TestInUseAllocation(uint8_t* aPtr, size_t aSize) {
  phc::AddrInfo phcInfo;
  jemalloc_ptr_info_t jeInfo;

  // Test an in-use PHC allocation: first byte within it.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, aPtr, aSize,
                        true, false));
  ASSERT_EQ(moz_malloc_usable_size(aPtr), aSize);
  jemalloc_ptr_info(aPtr, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagLiveAlloc, aPtr, aSize, 0));

  // Test an in-use PHC allocation: last byte within it.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr + aSize - 1, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, aPtr, aSize,
                        true, false));
  ASSERT_EQ(moz_malloc_usable_size(aPtr + aSize - 1), aSize);
  jemalloc_ptr_info(aPtr + aSize - 1, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagLiveAlloc, aPtr, aSize, 0));

  // Test an in-use PHC allocation: last byte before it.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr - 1, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, aPtr, aSize,
                        true, false));
  ASSERT_EQ(moz_malloc_usable_size(aPtr - 1), 0ul);
  jemalloc_ptr_info(aPtr - 1, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: first byte on its allocation page.
  ASSERT_TRUE(
      mozilla::phc::IsPHCAllocation(aPtr + aSize - kPageSize, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::InUsePage, aPtr, aSize,
                        true, false));
  jemalloc_ptr_info(aPtr + aSize - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: first byte in the following guard page.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr + aSize, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, false));
  jemalloc_ptr_info(aPtr + aSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: last byte in the lower half of the
  // following guard page.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr + aSize + (kPageSize / 2 - 1),
                                            &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, false));
  jemalloc_ptr_info(aPtr + aSize + (kPageSize / 2 - 1), &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: last byte in the preceding guard page.
  ASSERT_TRUE(
      mozilla::phc::IsPHCAllocation(aPtr + aSize - 1 - kPageSize, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, false));
  jemalloc_ptr_info(aPtr + aSize - 1 - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test an in-use PHC allocation: first byte in the upper half of the
  // preceding guard page.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(
      aPtr + aSize - 1 - kPageSize - (kPageSize / 2 - 1), &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, false));
  jemalloc_ptr_info(aPtr + aSize - 1 - kPageSize - (kPageSize / 2 - 1),
                    &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));
}

static void TestFreedAllocation(uint8_t* aPtr, size_t aSize) {
  phc::AddrInfo phcInfo;
  jemalloc_ptr_info_t jeInfo;

  // Test a freed PHC allocation: first byte within it.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagFreedAlloc, aPtr, aSize, 0));

  // Test a freed PHC allocation: last byte within it.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr + aSize - 1, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr + aSize - 1, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagFreedAlloc, aPtr, aSize, 0));

  // Test a freed PHC allocation: last byte before it.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr - 1, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr - 1, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: first byte on its allocation page.
  ASSERT_TRUE(
      mozilla::phc::IsPHCAllocation(aPtr + aSize - kPageSize, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::FreedPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr + aSize - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: first byte in the following guard page.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr + aSize, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr + aSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: last byte in the lower half of the following
  // guard page.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(aPtr + aSize + (kPageSize / 2 - 1),
                                            &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr + aSize + (kPageSize / 2 - 1), &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: last byte in the preceding guard page.
  ASSERT_TRUE(
      mozilla::phc::IsPHCAllocation(aPtr + aSize - 1 - kPageSize, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr + aSize - 1 - kPageSize, &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));

  // Test a freed PHC allocation: first byte in the upper half of the preceding
  // guard page.
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(
      aPtr + aSize - 1 - kPageSize - (kPageSize / 2 - 1), &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::GuardPage, aPtr, aSize,
                        true, true));
  jemalloc_ptr_info(aPtr + aSize - 1 - kPageSize - (kPageSize / 2 - 1),
                    &jeInfo);
  ASSERT_TRUE(JeInfoEq(jeInfo, TagUnknown, nullptr, 0, 0));
}

TEST(PHC, TestPHCInfo)
{
  mozilla::phc::SetPHCState(phc::PHCState::Enabled);

  int stackVar;
  phc::AddrInfo phcInfo;

  // Test a default AddrInfo.
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::Unknown, nullptr, 0ul,
                        false, false));

  // Test some non-PHC allocation addresses.
  ASSERT_FALSE(mozilla::phc::IsPHCAllocation(nullptr, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::Unknown, nullptr, 0,
                        false, false));
  ASSERT_FALSE(mozilla::phc::IsPHCAllocation(&stackVar, &phcInfo));
  ASSERT_TRUE(PHCInfoEq(phcInfo, phc::AddrInfo::Kind::Unknown, nullptr, 0,
                        false, false));

  uint8_t* p = GetPHCAllocation(32);
  if (!p) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  TestInUseAllocation(p, 32);

  free(p);

  TestFreedAllocation(p, 32);

  // There are no tests for `mKind == NeverAllocatedPage` because it's not
  // possible to reliably get ahold of such a page.
}

TEST(PHC, TestPHCDisablingThread)
{
  mozilla::phc::SetPHCState(phc::PHCState::Enabled);

  uint8_t* p = GetPHCAllocation(32);
  uint8_t* q = GetPHCAllocation(32);
  if (!p || !q) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  ASSERT_TRUE(mozilla::phc::IsPHCEnabledOnCurrentThread());
  mozilla::phc::DisablePHCOnCurrentThread();
  ASSERT_FALSE(mozilla::phc::IsPHCEnabledOnCurrentThread());

  // Test realloc() on a PHC allocation while PHC is disabled on the thread.
  uint8_t* p2 = (uint8_t*)realloc(p, 128);
  // The small realloc is fulfilled within the same page, but it does move.
  ASSERT_TRUE(p2 == p - 96);
  ASSERT_TRUE(mozilla::phc::IsPHCAllocation(p2, nullptr));
  uint8_t* p3 = (uint8_t*)realloc(p2, 2 * kPageSize);
  // The big realloc is not in-place, and the result is not a PHC allocation.
  ASSERT_TRUE(p3 != p2);
  ASSERT_FALSE(mozilla::phc::IsPHCAllocation(p3, nullptr));
  free(p3);

  // Test free() on a PHC allocation while PHC is disabled on the thread.
  free(q);

  // These must not be PHC allocations.
  uint8_t* r = GetPHCAllocation(32);  // This will fail.
  ASSERT_FALSE(!!r);

  mozilla::phc::ReenablePHCOnCurrentThread();
  ASSERT_TRUE(mozilla::phc::IsPHCEnabledOnCurrentThread());

  // If it really was reenabled we should be able to get PHC allocations
  // again.
  uint8_t* s = GetPHCAllocation(32);  // This should succeed.
  ASSERT_TRUE(!!s);
  free(s);
}

TEST(PHC, TestPHCDisablingGlobal)
{
  mozilla::phc::SetPHCState(phc::PHCState::Enabled);

  uint8_t* p1 = GetPHCAllocation(32);
  uint8_t* p2 = GetPHCAllocation(32);
  uint8_t* q = GetPHCAllocation(32);
  if (!p1 || !p2 || !q) {
    MOZ_CRASH("failed to get a PHC allocation");
  }

  mozilla::phc::SetPHCState(phc::PHCState::OnlyFree);

  // Test realloc() on a PHC allocation while PHC is disabled on the thread.
  uint8_t* p3 = (uint8_t*)realloc(p1, 128);
  // The small realloc is evicted from PHC because in "OnlyFree" state PHC
  // tries to reduce its memory impact.
  ASSERT_TRUE(p3 != p1);
  ASSERT_FALSE(mozilla::phc::IsPHCAllocation(p3, nullptr));
  free(p3);
  uint8_t* p4 = (uint8_t*)realloc(p2, 2 * kPageSize);
  // The big realloc is not in-place, and the result is not a PHC
  // allocation, regardless of PHC's state.
  ASSERT_TRUE(p4 != p2);
  ASSERT_FALSE(mozilla::phc::IsPHCAllocation(p4, nullptr));
  free(p4);

  // Test free() on a PHC allocation while PHC is disabled on the thread.
  free(q);

  // These must not be PHC allocations.
  uint8_t* r = GetPHCAllocation(32);  // This will fail.
  ASSERT_FALSE(!!r);

  mozilla::phc::SetPHCState(phc::PHCState::Enabled);

  // If it really was reenabled we should be able to get PHC allocations
  // again.
  uint8_t* s = GetPHCAllocation(32);  // This should succeed.
  ASSERT_TRUE(!!s);
  free(s);
}

// This test is disabled for now, see Bug 1845017 and Bug 1845655.
// TEST(PHC, TestPHCExhaustion)
// {
void DisabledPHCExhaustionTest() {
  // PHC hardcodes the amount of allocations to track.
#if defined(XP_DARWIN) && defined(__aarch64__)
  const unsigned NUM_ALLOCATIONS = 1024;
#else
  const unsigned NUM_ALLOCATIONS = 4096;
#endif
  uint8_t* allocations[NUM_ALLOCATIONS];
  const unsigned REQUIRED_ALLOCATIONS = NUM_ALLOCATIONS - 50;

  unsigned last_allocation;
  for (unsigned i = 0; i < NUM_ALLOCATIONS; i++) {
    allocations[i] = GetPHCAllocation(128);
    last_allocation = i;
    if (i < REQUIRED_ALLOCATIONS) {
      // Assert that the first REQUIRED_ALLOCATIONS work.  We require only
      // REQUIRED_ALLOCATIONS rather than NUM_ALLOCATIONS because sometimes
      // some PHC slots are used before the test begins.
      ASSERT_TRUE(allocations[i]);
    } else if (!allocations[i]) {
      // Break the loop if an allocation fails to move to the next phase.
      last_allocation--;
      break;
    }
    TestInUseAllocation(allocations[i], 128);
  }

  // We should now fail to get an allocation.
  ASSERT_FALSE(GetPHCAllocation(128));

  for (unsigned i = 0; i <= last_allocation; i++) {
    free(allocations[i]);
    TestFreedAllocation(allocations[i], 128);
  }

  // And now that we've released those allocations we should be able to get
  // new allocations again.
  uint8_t* r = GetPHCAllocation(128);
  ASSERT_TRUE(!!r);
  free(r);
}
