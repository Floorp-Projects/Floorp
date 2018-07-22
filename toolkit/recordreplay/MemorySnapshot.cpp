/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemorySnapshot.h"

#include "ipc/ChildIPC.h"
#include "js/ReplayHooks.h"
#include "mozilla/Maybe.h"
#include "DirtyMemoryHandler.h"
#include "InfallibleVector.h"
#include "ProcessRecordReplay.h"
#include "ProcessRewind.h"
#include "SpinLock.h"
#include "SplayTree.h"
#include "Thread.h"

#include <algorithm>
#include <mach/mach.h>
#include <mach/mach_vm.h>

// Define to enable the countdown debugging thread. See StartCountdown().
//#define WANT_COUNTDOWN_THREAD 1

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Memory Snapshots Overview.
//
// Checkpoints are periodically saved, storing in memory enough information
// for the process to restore the contents of all tracked memory as it
// rewinds to earlier checkpoitns. There are two components to a saved
// checkpoint:
//
// - Stack contents for each thread are completely saved on disk at each saved
//   checkpoint. This is handled by ThreadSnapshot.cpp
//
// - Heap and static memory contents (tracked memory) are saved in memory as
//   the contents of pages modified before either the the next saved checkpoint
//   or the current execution point (if this is the last saved checkpoint).
//   This is handled here.
//
// Heap memory is only tracked when allocated with TrackedMemoryKind.
//
// Snapshots of heap/static memory is modeled on the copy-on-write semantics
// used by fork. Instead of actually forking, we use write-protected memory and
// a fault handler to perform the copy-on-write, which both gives more control
// of the snapshot process and allows snapshots to be taken on platforms
// without fork (i.e. Windows). The following example shows how snapshots are
// generated:
//
// #1 Save Checkpoint A. The initial snapshot tabulates all allocated tracked
//    memory in the process, and write-protects all of it.
//
// #2 Write pages P0 and P1. Writing to the pages trips the fault handler. The
//    handler creates copies of the initial contents of P0 and P1 (P0a and P1a)
//    and unprotects the pages.
//
// #3 Save Checkpoint B. P0a and P1a, along with any other pages modified
//    between A and B, become associated with checkpoint A. All modified pages
//    are reprotected.
//
// #4 Write pages P1 and P2. Again, writing to the pages trips the fault
//    handler and copies P1b and P2b are created and the pages are unprotected.
//
// #5 Save Checkpoint C. P1b and P2b become associated with snapshot B, and the
//    modified pages are reprotected.
//
// If we were to then rewind from C to A, we would read and restore P1b/P2b,
// followed by P0a/P1a. All data associated with checkpoints A and later is
// discarded (we can only rewind; we cannot jump forward in time).
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Snapshot Threads Overview.
//
// After step #3 above, the main thread has created a diff snapshot with the
// copies of the original contents of pages modified between two saved
// checkpoints. These page copies are initially all in memory. It is the
// responsibility of the snapshot threads to do the following:
//
// 1. When rewinding to the last saved checkpoint, snapshot threads are used to
//    restore the original contents of pages using their in-memory copies.
//
// There are a fixed number of snapshot threads that are spawned when the
// first checkpoint is saved. Threads are each responsible for distinct sets of
// heap memory pages (see AddDirtyPageToWorklist), avoiding synchronization
// issues between different snapshot threads.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Memory Snapshot Structures
///////////////////////////////////////////////////////////////////////////////

// A region of allocated memory which should be tracked by MemoryInfo.
struct AllocatedMemoryRegion {
  uint8_t* mBase;
  size_t mSize;
  bool mExecutable;

  AllocatedMemoryRegion()
    : mBase(nullptr), mSize(0), mExecutable(false)
  {}

  AllocatedMemoryRegion(uint8_t* aBase, size_t aSize, bool aExecutable)
    : mBase(aBase), mSize(aSize), mExecutable(aExecutable)
  {}

  // For sorting regions by base address.
  struct AddressSort {
    typedef void* Lookup;
    static void* getLookup(const AllocatedMemoryRegion& aRegion) {
      return aRegion.mBase;
    }
    static ssize_t compare(void* aAddress, const AllocatedMemoryRegion& aRegion) {
      return (uint8_t*) aAddress - aRegion.mBase;
    }
  };

  // For sorting regions by size, from largest to smallest.
  struct SizeReverseSort {
    typedef size_t Lookup;
    static size_t getLookup(const AllocatedMemoryRegion& aRegion) {
      return aRegion.mSize;
    }
    static ssize_t compare(size_t aSize, const AllocatedMemoryRegion& aRegion) {
      return aRegion.mSize - aSize;
    }
  };
};

// Information about a page which was modified between two saved checkpoints.
struct DirtyPage {
  // Base address of the page.
  uint8_t* mBase;

  // Copy of the page at the first checkpoint. Written by the dirty memory
  // handler via HandleDirtyMemoryFault if this is in the active page set,
  // otherwise accessed by snapshot threads.
  uint8_t* mOriginal;

  bool mExecutable;

  DirtyPage(uint8_t* aBase, uint8_t* aOriginal, bool aExecutable)
    : mBase(aBase), mOriginal(aOriginal), mExecutable(aExecutable)
  {}

  struct AddressSort {
    typedef uint8_t* Lookup;
    static uint8_t* getLookup(const DirtyPage& aPage) {
      return aPage.mBase;
    }
    static ssize_t compare(uint8_t* aBase, const DirtyPage& aPage) {
      return aBase - aPage.mBase;
    }
  };
};

// A set of dirty pages that can be searched quickly.
typedef SplayTree<DirtyPage, DirtyPage::AddressSort,
                  AllocPolicy<UntrackedMemoryKind::SortedDirtyPageSet>, 4> SortedDirtyPageSet;

// A set of dirty pages associated with some checkpoint.
struct DirtyPageSet {
  // Checkpoint associated with this set.
  CheckpointId mCheckpoint;

  // All dirty pages in the set. Pages may be added or destroyed by the main
  // thread when all other threads are idle, by the dirty memory handler when
  // it is active and this is the active page set, and by the snapshot thread
  // which owns this set.
  InfallibleVector<DirtyPage, 256, AllocPolicy<UntrackedMemoryKind::DirtyPageSet>> mPages;

  explicit DirtyPageSet(const CheckpointId& aCheckpoint)
    : mCheckpoint(aCheckpoint)
  {}
};

// Worklist used by each snapshot thread.
struct SnapshotThreadWorklist {
  // Index into gMemoryInfo->mSnapshotWorklists of the thread.
  size_t mThreadIndex;

  // Record/replay ID of the thread.
  size_t mThreadId;

  // Sets of pages in the thread's worklist. Each set is for a different diff,
  // with the oldest checkpoints first.
  InfallibleVector<DirtyPageSet, 256, AllocPolicy<UntrackedMemoryKind::Generic>> mSets;
};

// Structure used to coordinate activity between the main thread and all
// snapshot threads. The workflow with this structure is as follows:
//
// 1. The main thread calls ActivateBegin(), marking the condition as active
//    and notifying each snapshot thread. The main thread blocks in this call.
//
// 2. Each snapshot thread, maybe after waking up, checks the condition, does
//    any processing it needs to (knowing the main thread is blocked) and
//    then calls WaitUntilNoLongerActive(), blocking in the call.
//
// 3. Once all snapshot threads are blocked in WaitUntilNoLongerActive(), the
//    main thread is unblocked from ActivateBegin(). It can then do whatever
//    processing it needs to (knowing all snapshot threads are blocked) and
//    then calls ActivateEnd(), blocking in the call.
//
// 4. Snapshot threads are now unblocked from WaitUntilNoLongerActive(). The
//    main thread does not unblock from ActivateEnd() until all snapshot
//    threads have left WaitUntilNoLongerActive().
//
// The intent with this class is to ensure that the main thread knows exactly
// when the snapshot threads are operating and that there is no potential for
// races between them.
class SnapshotThreadCondition {
  Atomic<bool, SequentiallyConsistent, Behavior::DontPreserve> mActive;
  Atomic<int32_t, SequentiallyConsistent, Behavior::DontPreserve> mCount;

public:
  void ActivateBegin();
  void ActivateEnd();
  bool IsActive();
  void WaitUntilNoLongerActive();
};

static const size_t NumSnapshotThreads = 8;

// A set of free regions in the process. There are two of these, for the
// free regions in tracked and untracked memory.
class FreeRegionSet {
  // Kind of memory being managed. This also describes the memory used by the
  // set itself.
  AllocatedMemoryKind mKind;

  // Lock protecting contents of the structure.
  SpinLock mLock;

  // To avoid reentrancy issues when growing the set, a chunk of pages for
  // the splay tree is preallocated for use the next time the tree needs to
  // expand its size.
  static const size_t ChunkPages = 4;
  void* mNextChunk;

  // Ensure there is a chunk available for the splay tree.
  void MaybeRefillNextChunk(AutoSpinLock& aLockHeld);

  // Get the next chunk from the free region set for this memory kind.
  void* TakeNextChunk();

  struct MyAllocPolicy {
    FreeRegionSet& mSet;

    template <typename T>
    void free_(T* aPtr, size_t aSize) { MOZ_CRASH(); }

    template <typename T>
    T* pod_malloc(size_t aNumElems) {
      MOZ_RELEASE_ASSERT(sizeof(T) * aNumElems <= ChunkPages * PageSize);
      return (T*) mSet.TakeNextChunk();
    }

    explicit MyAllocPolicy(FreeRegionSet& aSet)
      : mSet(aSet)
    {}
  };

  // All memory in gMemoryInfo->mTrackedRegions that is not in use at the current
  // point in execution.
  typedef SplayTree<AllocatedMemoryRegion,
                    AllocatedMemoryRegion::SizeReverseSort,
                    MyAllocPolicy, ChunkPages> Tree;
  Tree mRegions;

  void InsertLockHeld(void* aAddress, size_t aSize, AutoSpinLock& aLockHeld);
  void* ExtractLockHeld(size_t aSize, AutoSpinLock& aLockHeld);

public:
  explicit FreeRegionSet(AllocatedMemoryKind aKind)
    : mKind(aKind), mRegions(MyAllocPolicy(*this))
  {}

  // Get the single region set for a given memory kind.
  static FreeRegionSet& Get(AllocatedMemoryKind aKind);

  // Add a free region to the set.
  void Insert(void* aAddress, size_t aSize);

  // Remove a free region of the specified size. If aAddress is specified then
  // this address will be prioritized, but a different pointer may be returned.
  // The resulting memory will be zeroed.
  void* Extract(void* aAddress, size_t aSize);

  // Return whether a memory range intersects this set at all.
  bool Intersects(void* aAddress, size_t aSize);
};

// Information about the current memory state. The contents of this structure
// are in untracked memory.
struct MemoryInfo {
  // Whether new dirty pages or allocated regions are allowed.
  bool mMemoryChangesAllowed;

  // Untracked memory regions allocated before the first checkpoint. This is only
  // accessed on the main thread, and is not a vector because of reentrancy
  // issues.
  static const size_t MaxInitialUntrackedRegions = 256;
  AllocatedMemoryRegion mInitialUntrackedRegions[MaxInitialUntrackedRegions];
  SpinLock mInitialUntrackedRegionsLock;

  // All tracked memory in the process. This may be updated by any thread while
  // holding mTrackedRegionsLock.
  SplayTree<AllocatedMemoryRegion, AllocatedMemoryRegion::AddressSort,
            AllocPolicy<UntrackedMemoryKind::TrackedRegions>, 4>
    mTrackedRegions;
  InfallibleVector<AllocatedMemoryRegion, 512, AllocPolicy<UntrackedMemoryKind::TrackedRegions>>
    mTrackedRegionsByAllocationOrder;
  SpinLock mTrackedRegionsLock;

  // Pages from |trackedRegions| modified since the last saved checkpoint.
  // Accessed by any thread (usually the dirty memory handler) when memory
  // changes are allowed, and by the main thread when memory changes are not
  // allowed.
  SortedDirtyPageSet mActiveDirty;
  SpinLock mActiveDirtyLock;

  // All untracked memory which is available for new allocations.
  FreeRegionSet mFreeUntrackedRegions;

  // Worklists for each snapshot thread.
  SnapshotThreadWorklist mSnapshotWorklists[NumSnapshotThreads];

  // Whether snapshot threads should update memory to that when the last saved
  // diff was started.
  SnapshotThreadCondition mSnapshotThreadsShouldRestore;

  // Whether snapshot threads should idle.
  SnapshotThreadCondition mSnapshotThreadsShouldIdle;

  // Counter used by the countdown thread.
  Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> mCountdown;

  // Information for timers.
  double mStartTime;
  uint32_t mTimeHits[(size_t) TimerKind::Count];
  double mTimeTotals[(size_t) TimerKind::Count];

  // Information for memory allocation.
  Atomic<ssize_t, Relaxed, Behavior::DontPreserve> mMemoryBalance[UntrackedMemoryKind::Count];

  // Recent dirty memory faults.
  void* mDirtyMemoryFaults[50];

  // Whether RecordReplayDirective may crash this process.
  bool mIntentionalCrashesAllowed;

  // Whether the CrashSoon directive has been given to this process.
  bool mCrashSoon;

  MemoryInfo()
    : mMemoryChangesAllowed(true)
    , mFreeUntrackedRegions(UntrackedMemoryKind::FreeRegions)
    , mStartTime(CurrentTime())
    , mIntentionalCrashesAllowed(true)
  {
    // The singleton MemoryInfo is allocated with zeroed memory, so other
    // fields do not need explicit initialization.
  }
};

static MemoryInfo* gMemoryInfo = nullptr;

void
SetMemoryChangesAllowed(bool aAllowed)
{
  MOZ_RELEASE_ASSERT(gMemoryInfo->mMemoryChangesAllowed == !aAllowed);
  gMemoryInfo->mMemoryChangesAllowed = aAllowed;
}

static void
EnsureMemoryChangesAllowed()
{
  while (!gMemoryInfo->mMemoryChangesAllowed) {
    ThreadYield();
  }
}

void
StartCountdown(size_t aCount)
{
  gMemoryInfo->mCountdown = aCount;
}

AutoCountdown::AutoCountdown(size_t aCount)
{
  StartCountdown(aCount);
}

AutoCountdown::~AutoCountdown()
{
  StartCountdown(0);
}

#ifdef WANT_COUNTDOWN_THREAD

static void
CountdownThreadMain(void*)
{
  while (true) {
    if (gMemoryInfo->mCountdown && --gMemoryInfo->mCountdown == 0) {
      // When debugging hangs in the child process, we can break here in lldb
      // to inspect what the process is doing.
      child::ReportFatalError("CountdownThread activated");
    }
    ThreadYield();
  }
}

#endif // WANT_COUNTDOWN_THREAD

///////////////////////////////////////////////////////////////////////////////
// Profiling
///////////////////////////////////////////////////////////////////////////////

AutoTimer::AutoTimer(TimerKind aKind)
  : mKind(aKind), mStart(CurrentTime())
{}

AutoTimer::~AutoTimer()
{
  if (gMemoryInfo) {
    gMemoryInfo->mTimeHits[(size_t) mKind]++;
    gMemoryInfo->mTimeTotals[(size_t) mKind] += CurrentTime() - mStart;
  }
}

static const char* gTimerKindNames[] = {
#define DefineTimerKindName(aKind) #aKind,
  ForEachTimerKind(DefineTimerKindName)
#undef DefineTimerKindName
};

void
DumpTimers()
{
  if (!gMemoryInfo) {
    return;
  }
  Print("Times %.2fs\n", (CurrentTime() - gMemoryInfo->mStartTime) / 1000000.0);
  for (size_t i = 0; i < (size_t) TimerKind::Count; i++) {
    uint32_t hits = gMemoryInfo->mTimeHits[i];
    double time = gMemoryInfo->mTimeTotals[i];
    Print("%s: %d hits, %.2fs\n",
          gTimerKindNames[i], (int) hits, time / 1000000.0);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Directives
///////////////////////////////////////////////////////////////////////////////

void
SetAllowIntentionalCrashes(bool aAllowed)
{
  gMemoryInfo->mIntentionalCrashesAllowed = aAllowed;
}

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayDirective(long aDirective)
{
  switch ((Directive) aDirective) {
  case Directive::CrashSoon:
    gMemoryInfo->mCrashSoon = true;
    break;
  case Directive::MaybeCrash:
    if (gMemoryInfo->mIntentionalCrashesAllowed && gMemoryInfo->mCrashSoon) {
      PrintSpew("Intentionally Crashing!\n");
      MOZ_CRASH("RecordReplayDirective intentional crash");
    }
    gMemoryInfo->mCrashSoon = false;
    break;
  case Directive::AlwaysSaveTemporaryCheckpoints:
    JS::replay::hooks.alwaysSaveTemporaryCheckpoints();
    break;
  case Directive::AlwaysMarkMajorCheckpoints:
    child::NotifyAlwaysMarkMajorCheckpoints();
    break;
  default:
    MOZ_CRASH("Unknown directive");
  }
}

} // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Snapshot Thread Conditions
///////////////////////////////////////////////////////////////////////////////

void
SnapshotThreadCondition::ActivateBegin()
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!mActive);
  mActive = true;
  for (size_t i = 0; i < NumSnapshotThreads; i++) {
    Thread::Notify(gMemoryInfo->mSnapshotWorklists[i].mThreadId);
  }
  while (mCount != NumSnapshotThreads) {
    Thread::WaitNoIdle();
  }
}

void
SnapshotThreadCondition::ActivateEnd()
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(mActive);
  mActive = false;
  for (size_t i = 0; i < NumSnapshotThreads; i++) {
    Thread::Notify(gMemoryInfo->mSnapshotWorklists[i].mThreadId);
  }
  while (mCount) {
    Thread::WaitNoIdle();
  }
}

bool
SnapshotThreadCondition::IsActive()
{
  MOZ_RELEASE_ASSERT(!Thread::CurrentIsMainThread());
  return mActive;
}

void
SnapshotThreadCondition::WaitUntilNoLongerActive()
{
  MOZ_RELEASE_ASSERT(!Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(mActive);
  if (NumSnapshotThreads == ++mCount) {
    Thread::Notify(MainThreadId);
  }
  while (mActive) {
    Thread::WaitNoIdle();
  }
  if (0 == --mCount) {
    Thread::Notify(MainThreadId);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Snapshot Page Allocation
///////////////////////////////////////////////////////////////////////////////

// Get a page in untracked memory that can be used as a copy of a tracked page.
static uint8_t*
AllocatePageCopy()
{
  return (uint8_t*) AllocateMemory(PageSize, UntrackedMemoryKind::PageCopy);
}

// Free a page allocated by AllocatePageCopy.
static void
FreePageCopy(uint8_t* aPage)
{
  DeallocateMemory(aPage, PageSize, UntrackedMemoryKind::PageCopy);
}

///////////////////////////////////////////////////////////////////////////////
// Page Fault Handling
///////////////////////////////////////////////////////////////////////////////

void
MemoryMove(void* aDst, const void* aSrc, size_t aSize)
{
  MOZ_RELEASE_ASSERT((size_t)aDst % sizeof(uint32_t) == 0);
  MOZ_RELEASE_ASSERT((size_t)aSrc % sizeof(uint32_t) == 0);
  MOZ_RELEASE_ASSERT(aSize % sizeof(uint32_t) == 0);
  MOZ_RELEASE_ASSERT((size_t)aDst <= (size_t)aSrc || (size_t)aDst >= (size_t)aSrc + aSize);

  uint32_t* ndst = (uint32_t*)aDst;
  const uint32_t* nsrc = (const uint32_t*)aSrc;
  for (size_t i = 0; i < aSize / sizeof(uint32_t); i++) {
    ndst[i] = nsrc[i];
  }
}

void
MemoryZero(void* aDst, size_t aSize)
{
  MOZ_RELEASE_ASSERT((size_t)aDst % sizeof(uint32_t) == 0);
  MOZ_RELEASE_ASSERT(aSize % sizeof(uint32_t) == 0);

  // Use volatile here to avoid annoying clang optimizations.
  volatile uint32_t* ndst = (uint32_t*)aDst;
  for (size_t i = 0; i < aSize / sizeof(uint32_t); i++) {
    ndst[i] = 0;
  }
}

// Return whether an address is in a tracked region. This excludes memory that
// is in an active new region and is not write protected.
static bool
IsTrackedAddress(void* aAddress, bool* aExecutable)
{
  AutoSpinLock lock(gMemoryInfo->mTrackedRegionsLock);
  Maybe<AllocatedMemoryRegion> region =
    gMemoryInfo->mTrackedRegions.lookupClosestLessOrEqual(aAddress);
  if (region.isSome() && MemoryContains(region.ref().mBase, region.ref().mSize, aAddress)) {
    if (aExecutable) {
      *aExecutable = region.ref().mExecutable;
    }
    return true;
  }
  return false;
}

bool
HandleDirtyMemoryFault(uint8_t* aAddress)
{
  EnsureMemoryChangesAllowed();

  bool different = false;
  for (size_t i = ArrayLength(gMemoryInfo->mDirtyMemoryFaults) - 1; i; i--) {
    gMemoryInfo->mDirtyMemoryFaults[i] = gMemoryInfo->mDirtyMemoryFaults[i - 1];
    if (gMemoryInfo->mDirtyMemoryFaults[i] != aAddress) {
      different = true;
    }
  }
  gMemoryInfo->mDirtyMemoryFaults[0] = aAddress;
  if (!different) {
    Print("WARNING: Repeated accesses to the same dirty address %p\n", aAddress);
  }

  // Round down to the base of the page.
  aAddress = PageBase(aAddress);

  AutoSpinLock lock(gMemoryInfo->mActiveDirtyLock);

  // Check to see if this is already an active dirty page. Once a page has been
  // marked as dirty it will be accessible until the next checkpoint is saved,
  // but it's possible for multiple threads to access the same protected memory
  // before we have a chance to unprotect it, in which case we'll end up here
  // multiple times for the page.
  if (gMemoryInfo->mActiveDirty.maybeLookup(aAddress)) {
    return true;
  }

  // Crash if this address is not in a tracked region.
  bool executable;
  if (!IsTrackedAddress(aAddress, &executable)) {
    return false;
  }

  // Copy the page's original contents into the active dirty set, and unprotect
  // it so that execution can proceed.
  uint8_t* original = AllocatePageCopy();
  MemoryMove(original, aAddress, PageSize);
  gMemoryInfo->mActiveDirty.insert(aAddress, DirtyPage(aAddress, original, executable));
  DirectUnprotectMemory(aAddress, PageSize, executable);
  return true;
}

void
UnrecoverableSnapshotFailure()
{
  AutoSpinLock lock(gMemoryInfo->mTrackedRegionsLock);
  DirectUnprotectMemory(PageBase(&errno), PageSize, false);
  for (auto region : gMemoryInfo->mTrackedRegionsByAllocationOrder) {
    DirectUnprotectMemory(region.mBase, region.mSize, region.mExecutable,
                          /* aIgnoreFailures = */ true);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Initial Memory Region Processing
///////////////////////////////////////////////////////////////////////////////

static void
AddInitialUntrackedMemoryRegion(uint8_t* aBase, size_t aSize)
{
  MOZ_RELEASE_ASSERT(!HasSavedCheckpoint());

  if (gInitializationFailureMessage) {
    return;
  }

  static void* gSkippedRegion;
  if (!gSkippedRegion) {
    // We are allocating gMemoryInfo itself, and will directly call this
    // function again shortly.
    gSkippedRegion = aBase;
    return;
  }
  MOZ_RELEASE_ASSERT(gSkippedRegion == gMemoryInfo);

  AutoSpinLock lock(gMemoryInfo->mInitialUntrackedRegionsLock);

  for (AllocatedMemoryRegion& region : gMemoryInfo->mInitialUntrackedRegions) {
    if (!region.mBase) {
      region.mBase = aBase;
      region.mSize = aSize;
      return;
    }
  }

  // If we end up here then MaxInitialUntrackedRegions should be larger.
  MOZ_CRASH();
}

static void
RemoveInitialUntrackedRegion(uint8_t* aBase, size_t aSize)
{
  MOZ_RELEASE_ASSERT(!HasSavedCheckpoint());
  AutoSpinLock lock(gMemoryInfo->mInitialUntrackedRegionsLock);

  for (AllocatedMemoryRegion& region : gMemoryInfo->mInitialUntrackedRegions) {
    if (region.mBase == aBase) {
      MOZ_RELEASE_ASSERT(region.mSize == aSize);
      region.mBase = nullptr;
      region.mSize = 0;
      return;
    }
  }
  MOZ_CRASH();
}

static void
MarkThreadStacksAsUntracked()
{
  // Thread stacks are excluded from the tracked regions.
  for (size_t i = MainThreadId; i <= MaxThreadId; i++) {
    Thread* thread = Thread::GetById(i);
    AddInitialUntrackedMemoryRegion(thread->StackBase(), thread->StackSize());
  }
}

// Given an address region [*aAddress, *aAddress + *aSize], return true if
// there is any intersection with an excluded region
// [aExclude, aExclude + aExcludeSize], set *aSize to contain the subregion
// starting at aAddress which which is not excluded, and *aRemaining and
// *aRemainingSize to any additional subregion which is not excluded.
static bool
MaybeExtractMemoryRegion(uint8_t* aAddress, size_t* aSize,
                         uint8_t** aRemaining, size_t* aRemainingSize,
                         uint8_t* aExclude, size_t aExcludeSize)
{
  uint8_t* addrLimit = aAddress + *aSize;

  // Expand the excluded region out to the containing page boundaries.
  MOZ_RELEASE_ASSERT((size_t)aExclude % PageSize == 0);
  aExcludeSize = RoundupSizeToPageBoundary(aExcludeSize);

  uint8_t* excludeLimit = aExclude + aExcludeSize;

  if (excludeLimit <= aAddress || addrLimit <= aExclude) {
    // No intersection.
    return false;
  }

  *aSize = std::max<ssize_t>(aExclude - aAddress, 0);
  if (aRemaining) {
    *aRemaining = excludeLimit;
    *aRemainingSize = std::max<ssize_t>(addrLimit - *aRemaining, 0);
  }
  return true;
}

// Set *aSize to describe the number of bytes starting at aAddress that should
// be considered tracked memory. *aRemaining and *aRemainingSize are set to any
// remaining portion of the initial region after the first excluded portion
// that is found.
static void
ExtractTrackedInitialMemoryRegion(uint8_t* aAddress, size_t* aSize,
                                  uint8_t** aRemaining, size_t* aRemainingSize)
{
  // Look for the earliest untracked region which intersects the given region.
  const AllocatedMemoryRegion* earliestIntersect = nullptr;
  for (const AllocatedMemoryRegion& region : gMemoryInfo->mInitialUntrackedRegions) {
    size_t size = *aSize;
    if (MaybeExtractMemoryRegion(aAddress, &size, nullptr, nullptr, region.mBase, region.mSize)) {
      // There was an intersection.
      if (!earliestIntersect || region.mBase < earliestIntersect->mBase) {
        earliestIntersect = &region;
      }
    }
  }

  if (earliestIntersect) {
    if (!MaybeExtractMemoryRegion(aAddress, aSize, aRemaining, aRemainingSize,
                                  earliestIntersect->mBase, earliestIntersect->mSize)) {
      MOZ_CRASH();
    }
  } else {
    // If there is no intersection then the entire region is tracked.
    *aRemaining = aAddress + *aSize;
    *aRemainingSize = 0;
  }
}

static void
AddTrackedRegion(uint8_t* aAddress, size_t aSize, bool aExecutable)
{
  if (aSize) {
    AutoSpinLock lock(gMemoryInfo->mTrackedRegionsLock);
    gMemoryInfo->mTrackedRegions.insert(aAddress,
                                        AllocatedMemoryRegion(aAddress, aSize, aExecutable));
    gMemoryInfo->mTrackedRegionsByAllocationOrder.emplaceBack(aAddress, aSize, aExecutable);
  }
}

// Add any tracked subregions of [aAddress, aAddress + aSize].
void
AddInitialTrackedMemoryRegions(uint8_t* aAddress, size_t aSize, bool aExecutable)
{
  while (aSize) {
    uint8_t* remaining;
    size_t remainingSize;
    ExtractTrackedInitialMemoryRegion(aAddress, &aSize, &remaining, &remainingSize);

    AddTrackedRegion(aAddress, aSize, aExecutable);

    aAddress = remaining;
    aSize = remainingSize;
  }
}

static void UpdateNumTrackedRegionsForSnapshot();

// Handle all initial untracked memory regions in the process.
static void
ProcessAllInitialMemoryRegions()
{
  MOZ_ASSERT(!AreThreadEventsPassedThrough());

  {
    AutoPassThroughThreadEvents pt;
    for (mach_vm_address_t addr = 0;;) {
      mach_vm_size_t nbytes;

      vm_region_basic_info_64 info;
      mach_msg_type_number_t info_count = sizeof(vm_region_basic_info_64);
      mach_port_t some_port;
      kern_return_t rv = mach_vm_region(mach_task_self(), &addr, &nbytes, VM_REGION_BASIC_INFO,
                                        (vm_region_info_t) &info, &info_count, &some_port);
      if (rv == KERN_INVALID_ADDRESS) {
        break;
      }
      MOZ_RELEASE_ASSERT(rv == KERN_SUCCESS);

      if (info.max_protection & VM_PROT_WRITE) {
        MOZ_RELEASE_ASSERT(info.max_protection & VM_PROT_READ);
        AddInitialTrackedMemoryRegions(reinterpret_cast<uint8_t*>(addr), nbytes,
                                       info.max_protection & VM_PROT_EXECUTE);
      }

      addr += nbytes;
    }
  }

  UpdateNumTrackedRegionsForSnapshot();

  // Write protect all tracked memory.
  AutoDisallowMemoryChanges disallow;
  for (const AllocatedMemoryRegion& region : gMemoryInfo->mTrackedRegionsByAllocationOrder) {
    DirectWriteProtectMemory(region.mBase, region.mSize, region.mExecutable);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Free Region Management
///////////////////////////////////////////////////////////////////////////////

// All memory in gMemoryInfo->mTrackedRegions that is not in use at the current
// point in execution.
static FreeRegionSet gFreeRegions(TrackedMemoryKind);

// The size of gMemoryInfo->mTrackedRegionsByAllocationOrder we expect to see
// at the point of the last saved checkpoint.
static size_t gNumTrackedRegions;

static void
UpdateNumTrackedRegionsForSnapshot()
{
  MOZ_ASSERT(Thread::CurrentIsMainThread());
  gNumTrackedRegions = gMemoryInfo->mTrackedRegionsByAllocationOrder.length();
}

void
FixupFreeRegionsAfterRewind()
{
  // All memory that has been allocated since the associated checkpoint was
  // reached is now free, and may be reused for new allocations.
  size_t newTrackedRegions = gMemoryInfo->mTrackedRegionsByAllocationOrder.length();
  for (size_t i = gNumTrackedRegions; i < newTrackedRegions; i++) {
    const AllocatedMemoryRegion& region = gMemoryInfo->mTrackedRegionsByAllocationOrder[i];
    gFreeRegions.Insert(region.mBase, region.mSize);
  }
  gNumTrackedRegions = newTrackedRegions;
}

/* static */ FreeRegionSet&
FreeRegionSet::Get(AllocatedMemoryKind aKind)
{
  return (aKind == TrackedMemoryKind) ? gFreeRegions : gMemoryInfo->mFreeUntrackedRegions;
}

void*
FreeRegionSet::TakeNextChunk()
{
  MOZ_RELEASE_ASSERT(mNextChunk);
  void* res = mNextChunk;
  mNextChunk = nullptr;
  return res;
}

void
FreeRegionSet::InsertLockHeld(void* aAddress, size_t aSize, AutoSpinLock& aLockHeld)
{
  mRegions.insert(aSize, AllocatedMemoryRegion((uint8_t*) aAddress, aSize, true));
}

void
FreeRegionSet::MaybeRefillNextChunk(AutoSpinLock& aLockHeld)
{
  if (mNextChunk) {
    return;
  }

  // Look for a free region we can take the next chunk from.
  size_t size = ChunkPages * PageSize;
  gMemoryInfo->mMemoryBalance[mKind] += size;

  mNextChunk = ExtractLockHeld(size, aLockHeld);

  if (!mNextChunk) {
    // Allocate memory from the system.
    mNextChunk = DirectAllocateMemory(nullptr, size);
    RegisterAllocatedMemory(mNextChunk, size, mKind);
  }
}

void
FreeRegionSet::Insert(void* aAddress, size_t aSize)
{
  MOZ_RELEASE_ASSERT(aAddress && aAddress == PageBase(aAddress));
  MOZ_RELEASE_ASSERT(aSize && aSize == RoundupSizeToPageBoundary(aSize));

  AutoSpinLock lock(mLock);

  MaybeRefillNextChunk(lock);
  InsertLockHeld(aAddress, aSize, lock);
}

void*
FreeRegionSet::ExtractLockHeld(size_t aSize, AutoSpinLock& aLockHeld)
{
  Maybe<AllocatedMemoryRegion> best =
    mRegions.lookupClosestLessOrEqual(aSize, /* aRemove = */ true);
  if (best.isSome()) {
    MOZ_RELEASE_ASSERT(best.ref().mSize >= aSize);
    uint8_t* res = best.ref().mBase;
    if (best.ref().mSize > aSize) {
      InsertLockHeld(res + aSize, best.ref().mSize - aSize, aLockHeld);
    }
    MemoryZero(res, aSize);
    return res;
  }
  return nullptr;
}

void*
FreeRegionSet::Extract(void* aAddress, size_t aSize)
{
  MOZ_RELEASE_ASSERT(aAddress == PageBase(aAddress));
  MOZ_RELEASE_ASSERT(aSize && aSize == RoundupSizeToPageBoundary(aSize));

  AutoSpinLock lock(mLock);

  if (aAddress) {
    MaybeRefillNextChunk(lock);

    // We were given a point at which to try to place the allocation. Look for
    // a free region which contains [aAddress, aAddress + aSize] entirely.
    for (typename Tree::Iter iter = mRegions.begin(); !iter.done(); ++iter) {
      uint8_t* regionBase = iter.ref().mBase;
      uint8_t* regionExtent = regionBase + iter.ref().mSize;
      uint8_t* addrBase = (uint8_t*)aAddress;
      uint8_t* addrExtent = addrBase + aSize;
      if (regionBase <= addrBase && regionExtent >= addrExtent) {
        iter.removeEntry();
        if (regionBase < addrBase) {
          InsertLockHeld(regionBase, addrBase - regionBase, lock);
        }
        if (regionExtent > addrExtent) {
          InsertLockHeld(addrExtent, regionExtent - addrExtent, lock);
        }
        MemoryZero(aAddress, aSize);
        return aAddress;
      }
    }
    // Fall through and look for a free region at another address.
  }

  // No address hint, look for the smallest free region which is larger than
  // the desired allocation size.
  return ExtractLockHeld(aSize, lock);
}

bool
FreeRegionSet::Intersects(void* aAddress, size_t aSize)
{
  AutoSpinLock lock(mLock);
  for (typename Tree::Iter iter = mRegions.begin(); !iter.done(); ++iter) {
    if (MemoryIntersects(iter.ref().mBase, iter.ref().mSize, aAddress, aSize)) {
      return true;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Memory Management
///////////////////////////////////////////////////////////////////////////////

void
RegisterAllocatedMemory(void* aBaseAddress, size_t aSize, AllocatedMemoryKind aKind)
{
  MOZ_RELEASE_ASSERT(aBaseAddress == PageBase(aBaseAddress));
  MOZ_RELEASE_ASSERT(aSize == RoundupSizeToPageBoundary(aSize));

  uint8_t* aAddress = reinterpret_cast<uint8_t*>(aBaseAddress);

  if (aKind != TrackedMemoryKind) {
    if (!HasSavedCheckpoint()) {
      AddInitialUntrackedMemoryRegion(aAddress, aSize);
    }
  } else if (HasSavedCheckpoint()) {
    EnsureMemoryChangesAllowed();
    DirectWriteProtectMemory(aAddress, aSize, true);
    AddTrackedRegion(aAddress, aSize, true);
  }
}

void
CheckFixedMemory(void* aAddress, size_t aSize)
{
  MOZ_RELEASE_ASSERT(aAddress == PageBase(aAddress));
  MOZ_RELEASE_ASSERT(aSize == RoundupSizeToPageBoundary(aSize));

  if (!HasSavedCheckpoint()) {
    return;
  }

  {
    // The memory should already be tracked. Check each page in the allocation
    // because there might be tracked regions adjacent to one another, neither
    // of which entirely contains this memory.
    AutoSpinLock lock(gMemoryInfo->mTrackedRegionsLock);
    for (size_t offset = 0; offset < aSize; offset += PageSize) {
      uint8_t* page = (uint8_t*)aAddress + offset;
      Maybe<AllocatedMemoryRegion> region =
        gMemoryInfo->mTrackedRegions.lookupClosestLessOrEqual(page);
      if (!region.isSome() ||
          !MemoryContains(region.ref().mBase, region.ref().mSize, page, PageSize)) {
        child::ReportFatalError("Fixed memory is not tracked!");
      }
    }
  }

  // The memory should not be free.
  if (gFreeRegions.Intersects(aAddress, aSize)) {
    child::ReportFatalError("Fixed memory is currently free!");
  }
}

void
RestoreWritableFixedMemory(void* aAddress, size_t aSize)
{
  MOZ_RELEASE_ASSERT(aAddress == PageBase(aAddress));
  MOZ_RELEASE_ASSERT(aSize == RoundupSizeToPageBoundary(aSize));

  if (!HasSavedCheckpoint()) {
    return;
  }

  AutoSpinLock lock(gMemoryInfo->mActiveDirtyLock);
  for (size_t offset = 0; offset < aSize; offset += PageSize) {
    uint8_t* page = (uint8_t*)aAddress + offset;
    if (gMemoryInfo->mActiveDirty.maybeLookup(page)) {
      DirectUnprotectMemory(page, PageSize, true);
    }
  }
}

void*
AllocateMemoryTryAddress(void* aAddress, size_t aSize, AllocatedMemoryKind aKind)
{
  MOZ_RELEASE_ASSERT(aAddress == PageBase(aAddress));
  aSize = RoundupSizeToPageBoundary(aSize);

  if (gMemoryInfo) {
    gMemoryInfo->mMemoryBalance[aKind] += aSize;
  }

  if (HasSavedCheckpoint()) {
    if (void* res = FreeRegionSet::Get(aKind).Extract(aAddress, aSize)) {
      return res;
    }
  }

  void* res = DirectAllocateMemory(aAddress, aSize);
  RegisterAllocatedMemory(res, aSize, aKind);
  return res;
}

extern "C" {

MOZ_EXPORT void*
RecordReplayInterface_AllocateMemory(size_t aSize, AllocatedMemoryKind aKind)
{
  if (!IsReplaying()) {
    return DirectAllocateMemory(nullptr, aSize);
  }
  return AllocateMemoryTryAddress(nullptr, aSize, aKind);
}

MOZ_EXPORT void
RecordReplayInterface_DeallocateMemory(void* aAddress, size_t aSize, AllocatedMemoryKind aKind)
{
  // Round the supplied region to the containing page boundaries.
  aSize += (uint8_t*) aAddress - PageBase(aAddress);
  aAddress = PageBase(aAddress);
  aSize = RoundupSizeToPageBoundary(aSize);

  if (!aAddress || !aSize) {
    return;
  }

  if (gMemoryInfo) {
    gMemoryInfo->mMemoryBalance[aKind] -= aSize;
  }

  // Memory is returned to the system before saving the first checkpoint.
  if (!HasSavedCheckpoint()) {
    if (IsReplaying() && aKind != TrackedMemoryKind) {
      RemoveInitialUntrackedRegion((uint8_t*) aAddress, aSize);
    }
    DirectDeallocateMemory(aAddress, aSize);
    return;
  }

  if (aKind == TrackedMemoryKind) {
    // For simplicity, all free regions must be executable, so ignore deallocated
    // memory in regions that are not executable.
    bool executable;
    if (!IsTrackedAddress(aAddress, &executable) || !executable) {
      return;
    }
  }

  // Mark this region as free, but do not unmap it. It will become usable for
  // later allocations, but will not need to be remapped if we end up
  // rewinding to a point where this memory was in use.
  FreeRegionSet::Get(aKind).Insert(aAddress, aSize);
}

} // extern "C"

///////////////////////////////////////////////////////////////////////////////
// Snapshot Threads
///////////////////////////////////////////////////////////////////////////////

// While on a snapshot thread, restore the contents of all pages belonging to
// this thread which were modified since the last recorded diff snapshot.
static void
SnapshotThreadRestoreLastDiffSnapshot(SnapshotThreadWorklist* aWorklist)
{
  CheckpointId checkpoint = GetLastSavedCheckpoint();

  DirtyPageSet& set = aWorklist->mSets.back();
  MOZ_RELEASE_ASSERT(set.mCheckpoint == checkpoint);

  // Copy the original contents of all pages.
  for (size_t index = 0; index < set.mPages.length(); index++) {
    const DirtyPage& page = set.mPages[index];
    MOZ_RELEASE_ASSERT(page.mOriginal);
    DirectUnprotectMemory(page.mBase, PageSize, page.mExecutable);
    MemoryMove(page.mBase, page.mOriginal, PageSize);
    DirectWriteProtectMemory(page.mBase, PageSize, page.mExecutable);
    FreePageCopy(page.mOriginal);
  }

  // Remove the set from the worklist, if necessary.
  if (!aWorklist->mSets.empty()) {
    MOZ_RELEASE_ASSERT(&set == &aWorklist->mSets.back());
    aWorklist->mSets.popBack();
  }
}

// Start routine for a snapshot thread.
void
SnapshotThreadMain(void* aArgument)
{
  size_t threadIndex = (size_t) aArgument;
  SnapshotThreadWorklist* worklist = &gMemoryInfo->mSnapshotWorklists[threadIndex];
  worklist->mThreadIndex = threadIndex;

  while (true) {
    // If the main thread is waiting for us to restore the most recent diff,
    // then do so and notify the main thread we finished.
    if (gMemoryInfo->mSnapshotThreadsShouldRestore.IsActive()) {
      SnapshotThreadRestoreLastDiffSnapshot(worklist);
      gMemoryInfo->mSnapshotThreadsShouldRestore.WaitUntilNoLongerActive();
    }

    // Idle if the main thread wants us to.
    if (gMemoryInfo->mSnapshotThreadsShouldIdle.IsActive()) {
      gMemoryInfo->mSnapshotThreadsShouldIdle.WaitUntilNoLongerActive();
    }

    // Idle until notified by the main thread.
    Thread::WaitNoIdle();
  }
}

// An alternative to memcmp that can be called from any place.
static bool
MemoryEquals(void* aDst, void* aSrc, size_t aSize)
{
  MOZ_ASSERT((size_t)aDst % sizeof(size_t) == 0);
  MOZ_ASSERT((size_t)aSrc % sizeof(size_t) == 0);
  MOZ_ASSERT(aSize % sizeof(size_t) == 0);

  size_t* ndst = (size_t*)aDst;
  size_t* nsrc = (size_t*)aSrc;
  for (size_t i = 0; i < aSize / sizeof(size_t); i++) {
    if (ndst[i] != nsrc[i]) {
      return false;
    }
  }
  return true;
}

// Add a page to the last set in some snapshot thread's worklist. This is
// called on the main thread while the snapshot thread is idle.
static void
AddDirtyPageToWorklist(uint8_t* aAddress, uint8_t* aOriginal, bool aExecutable)
{
  // Distribute pages to snapshot threads using the base address of a page.
  // This guarantees that the same page will be consistently assigned to the
  // same thread as different snapshots are taken.
  MOZ_ASSERT((size_t)aAddress % PageSize == 0);
  if (MemoryEquals(aAddress, aOriginal, PageSize)) {
    FreePageCopy(aOriginal);
  } else {
    size_t pageIndex = ((size_t)aAddress / PageSize) % NumSnapshotThreads;
    SnapshotThreadWorklist* worklist = &gMemoryInfo->mSnapshotWorklists[pageIndex];
    MOZ_RELEASE_ASSERT(!worklist->mSets.empty());
    DirtyPageSet& set = worklist->mSets.back();
    MOZ_RELEASE_ASSERT(set.mCheckpoint == GetLastSavedCheckpoint());
    set.mPages.emplaceBack(aAddress, aOriginal, aExecutable);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Snapshot Interface
///////////////////////////////////////////////////////////////////////////////

void
InitializeMemorySnapshots()
{
  MOZ_RELEASE_ASSERT(gMemoryInfo == nullptr);
  void* memory = AllocateMemory(sizeof(MemoryInfo), UntrackedMemoryKind::Generic);
  gMemoryInfo = new(memory) MemoryInfo();

  // Mark gMemoryInfo as untracked. See AddInitialUntrackedMemoryRegion.
  AddInitialUntrackedMemoryRegion(reinterpret_cast<uint8_t*>(memory), sizeof(MemoryInfo));
}

void
InitializeCountdownThread()
{
#ifdef WANT_COUNTDOWN_THREAD
  Thread::SpawnNonRecordedThread(CountdownThreadMain, nullptr);
#endif
}

void
TakeFirstMemorySnapshot()
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(gMemoryInfo->mTrackedRegions.empty());

  // Spawn all snapshot threads.
  {
    AutoPassThroughThreadEvents pt;

    for (size_t i = 0; i < NumSnapshotThreads; i++) {
      Thread* thread = Thread::SpawnNonRecordedThread(SnapshotThreadMain, (void*) i);
      gMemoryInfo->mSnapshotWorklists[i].mThreadId = thread->Id();
    }
  }

  // All threads should have been created by now.
  MarkThreadStacksAsUntracked();

  // Fill in the tracked regions for the process.
  ProcessAllInitialMemoryRegions();
}

void
TakeDiffMemorySnapshot()
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  UpdateNumTrackedRegionsForSnapshot();

  AutoDisallowMemoryChanges disallow;

  // Stop all snapshot threads while we modify their worklists.
  gMemoryInfo->mSnapshotThreadsShouldIdle.ActivateBegin();

  // Add a DirtyPageSet to each snapshot thread's worklist for this snapshot.
  for (size_t i = 0; i < NumSnapshotThreads; i++) {
    SnapshotThreadWorklist* worklist = &gMemoryInfo->mSnapshotWorklists[i];
    worklist->mSets.emplaceBack(GetLastSavedCheckpoint());
  }

  // Distribute remaining active dirty pages to the snapshot thread worklists.
  for (SortedDirtyPageSet::Iter iter = gMemoryInfo->mActiveDirty.begin(); !iter.done(); ++iter) {
    AddDirtyPageToWorklist(iter.ref().mBase, iter.ref().mOriginal, iter.ref().mExecutable);
    DirectWriteProtectMemory(iter.ref().mBase, PageSize, iter.ref().mExecutable);
  }

  gMemoryInfo->mActiveDirty.clear();

  // Allow snapshot threads to resume execution.
  gMemoryInfo->mSnapshotThreadsShouldIdle.ActivateEnd();
}

void
RestoreMemoryToLastSavedCheckpoint()
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  AutoDisallowMemoryChanges disallow;

  // Restore all dirty regions that have been modified since the last
  // checkpoint was saved/restored.
  for (SortedDirtyPageSet::Iter iter = gMemoryInfo->mActiveDirty.begin(); !iter.done(); ++iter) {
    MemoryMove(iter.ref().mBase, iter.ref().mOriginal, PageSize);
    FreePageCopy(iter.ref().mOriginal);
    DirectWriteProtectMemory(iter.ref().mBase, PageSize, iter.ref().mExecutable);
  }
  gMemoryInfo->mActiveDirty.clear();
}

void
RestoreMemoryToLastSavedDiffCheckpoint()
{
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(gMemoryInfo->mActiveDirty.empty());

  AutoDisallowMemoryChanges disallow;

  // Wait while the snapshot threads restore all pages modified since the diff
  // snapshot was recorded.
  gMemoryInfo->mSnapshotThreadsShouldRestore.ActivateBegin();
  gMemoryInfo->mSnapshotThreadsShouldRestore.ActivateEnd();
}

} // namespace recordreplay
} // namespace mozilla
