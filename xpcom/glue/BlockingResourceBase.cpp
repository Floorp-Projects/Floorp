/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BlockingResourceBase.h"

#ifdef DEBUG
#include "prthread.h"

#include "nsAutoPtr.h"

#ifndef MOZ_CALLSTACK_DISABLED
#include "CodeAddressService.h"
#include "nsHashKeys.h"
#include "nsStackWalk.h"
#include "nsTHashtable.h"
#endif

#include "mozilla/CondVar.h"
#include "mozilla/DeadlockDetector.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Mutex.h"

#ifdef MOZILLA_INTERNAL_API
#include "GeckoProfiler.h"
#endif //MOZILLA_INTERNAL_API

#endif // ifdef DEBUG

namespace mozilla {
//
// BlockingResourceBase implementation
//

// static members
const char* const BlockingResourceBase::kResourceTypeName[] = {
  // needs to be kept in sync with BlockingResourceType
  "Mutex", "ReentrantMonitor", "CondVar"
};

#ifdef DEBUG

PRCallOnceType BlockingResourceBase::sCallOnce;
unsigned BlockingResourceBase::sResourceAcqnChainFrontTPI = (unsigned)-1;
BlockingResourceBase::DDT* BlockingResourceBase::sDeadlockDetector;


void
BlockingResourceBase::StackWalkCallback(void* aPc, void* aSp, void* aClosure)
{
#ifndef MOZ_CALLSTACK_DISABLED
  AcquisitionState* state = (AcquisitionState*)aClosure;
  state->AppendElement(aPc);
#endif
}

void
BlockingResourceBase::GetStackTrace(AcquisitionState& aState)
{
#ifndef MOZ_CALLSTACK_DISABLED
  // Skip this function and the calling function.
  const uint32_t kSkipFrames = 2;

  aState.Clear();

  // NB: Ignore the return value, there's nothing useful we can do if this
  //     this fails.
  NS_StackWalk(StackWalkCallback, kSkipFrames,
               24, &aState, 0, nullptr);
#endif
}

/**
 * PrintCycle
 * Append to |aOut| detailed information about the circular
 * dependency in |aCycle|.  Returns true if it *appears* that this
 * cycle may represent an imminent deadlock, but this is merely a
 * heuristic; the value returned may be a false positive or false
 * negative.
 *
 * *NOT* thread safe.  Calls |Print()|.
 *
 * FIXME bug 456272 hack alert: because we can't write call
 * contexts into strings, all info is written to stderr, but only
 * some info is written into |aOut|
 */
bool
PrintCycle(const BlockingResourceBase::DDT::ResourceAcquisitionArray* aCycle,
           nsACString& aOut)
{
  NS_ASSERTION(aCycle->Length() > 1, "need > 1 element for cycle!");

  bool maybeImminent = true;

  fputs("=== Cyclical dependency starts at\n", stderr);
  aOut += "Cyclical dependency starts at\n";

  const BlockingResourceBase::DDT::ResourceAcquisitionArray::elem_type res =
    aCycle->ElementAt(0);
  maybeImminent &= res->Print(aOut);

  BlockingResourceBase::DDT::ResourceAcquisitionArray::index_type i;
  BlockingResourceBase::DDT::ResourceAcquisitionArray::size_type len =
    aCycle->Length();
  const BlockingResourceBase::DDT::ResourceAcquisitionArray::elem_type* it =
    1 + aCycle->Elements();
  for (i = 1; i < len - 1; ++i, ++it) {
    fputs("\n--- Next dependency:\n", stderr);
    aOut += "\nNext dependency:\n";

    maybeImminent &= (*it)->Print(aOut);
  }

  fputs("\n=== Cycle completed at\n", stderr);
  aOut += "Cycle completed at\n";
  (*it)->Print(aOut);

  return maybeImminent;
}

#ifndef MOZ_CALLSTACK_DISABLED
class CodeAddressServiceWriter MOZ_FINAL
{
public:
  explicit CodeAddressServiceWriter(nsACString& aOut) : mOut(aOut) {}

  void Write(const char* aFmt, ...) const
  {
    va_list ap;
    va_start(ap, aFmt);

    const size_t kMaxLength = 4096;
    char buffer[kMaxLength];

    vsnprintf(buffer, kMaxLength, aFmt, ap);
    mOut += buffer;
    fprintf(stderr, "%s", buffer);

    va_end(ap);
  }

private:
  nsACString& mOut;
};

struct CodeAddressServiceLock MOZ_FINAL
{
  static void Unlock() { }
  static void Lock() { }
  static bool IsLocked() { return true; }
};

struct CodeAddressServiceStringAlloc MOZ_FINAL
{
  static char* copy(const char* aString) { return ::strdup(aString); }
  static void free(char* aString) { ::free(aString); }
};

class CodeAddressServiceStringTable MOZ_FINAL
{
public:
  CodeAddressServiceStringTable() : mSet(32) {}

  const char* Intern(const char* aString)
  {
    nsCharPtrHashKey* e = mSet.PutEntry(aString);
    return e->GetKey();
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mSet.SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  typedef nsTHashtable<nsCharPtrHashKey> StringSet;
  StringSet mSet;
};

typedef CodeAddressService<CodeAddressServiceStringTable,
                           CodeAddressServiceStringAlloc,
                           CodeAddressServiceWriter,
                           CodeAddressServiceLock> WalkTheStackCodeAddressService;
#endif

bool
BlockingResourceBase::Print(nsACString& aOut) const
{
  fprintf(stderr, "--- %s : %s",
          kResourceTypeName[mType], mName);
  aOut += BlockingResourceBase::kResourceTypeName[mType];
  aOut += " : ";
  aOut += mName;

  bool acquired = IsAcquired();

  if (acquired) {
    fputs(" (currently acquired)\n", stderr);
    aOut += " (currently acquired)\n";
  }

  fputs(" calling context\n", stderr);
#ifdef MOZ_CALLSTACK_DISABLED
  fputs("  [stack trace unavailable]\n", stderr);
#else
  const AcquisitionState& state = acquired ? mAcquired : mFirstSeen;

  WalkTheStackCodeAddressService addressService;
  CodeAddressServiceWriter writer(aOut);
  for (uint32_t i = 0; i < state.Length(); i++) {
    addressService.WriteLocation(writer, state[i]);
  }

#endif

  return acquired;
}


BlockingResourceBase::BlockingResourceBase(
    const char* aName,
    BlockingResourceBase::BlockingResourceType aType)
  : mName(aName)
  , mType(aType)
#ifdef MOZ_CALLSTACK_DISABLED
  , mAcquired(false)
#else
  , mAcquired()
#endif
{
  NS_ABORT_IF_FALSE(mName, "Name must be nonnull");
  // PR_CallOnce guaranatees that InitStatics is called in a
  // thread-safe way
  if (PR_SUCCESS != PR_CallOnce(&sCallOnce, InitStatics)) {
    NS_RUNTIMEABORT("can't initialize blocking resource static members");
  }

  mChainPrev = 0;
  sDeadlockDetector->Add(this);
}


BlockingResourceBase::~BlockingResourceBase()
{
  // we don't check for really obviously bad things like freeing
  // Mutexes while they're still locked.  it is assumed that the
  // base class, or its underlying primitive, will check for such
  // stupid mistakes.
  mChainPrev = 0;             // racy only for stupidly buggy client code
  sDeadlockDetector->Remove(this);
}


size_t
BlockingResourceBase::SizeOfDeadlockDetector(MallocSizeOf aMallocSizeOf)
{
  return sDeadlockDetector ?
      sDeadlockDetector->SizeOfIncludingThis(aMallocSizeOf) : 0;
}


PRStatus
BlockingResourceBase::InitStatics()
{
  PR_NewThreadPrivateIndex(&sResourceAcqnChainFrontTPI, 0);
  sDeadlockDetector = new DDT();
  if (!sDeadlockDetector) {
    NS_RUNTIMEABORT("can't allocate deadlock detector");
  }
  return PR_SUCCESS;
}


void
BlockingResourceBase::Shutdown()
{
  delete sDeadlockDetector;
  sDeadlockDetector = 0;
}


void
BlockingResourceBase::CheckAcquire()
{
  if (mType == eCondVar) {
    NS_NOTYETIMPLEMENTED(
      "FIXME bug 456272: annots. to allow CheckAcquire()ing condvars");
    return;
  }

  BlockingResourceBase* chainFront = ResourceChainFront();
  nsAutoPtr<DDT::ResourceAcquisitionArray> cycle(
    sDeadlockDetector->CheckAcquisition(
      chainFront ? chainFront : 0, this));
  if (!cycle) {
    return;
  }

#ifndef MOZ_CALLSTACK_DISABLED
  // Update the current stack before printing.
  GetStackTrace(mAcquired);
#endif

  fputs("###!!! ERROR: Potential deadlock detected:\n", stderr);
  nsAutoCString out("Potential deadlock detected:\n");
  bool maybeImminent = PrintCycle(cycle, out);

  if (maybeImminent) {
    fputs("\n###!!! Deadlock may happen NOW!\n\n", stderr);
    out.AppendLiteral("\n###!!! Deadlock may happen NOW!\n\n");
  } else {
    fputs("\nDeadlock may happen for some other execution\n\n",
          stderr);
    out.AppendLiteral("\nDeadlock may happen for some other execution\n\n");
  }

  // XXX can customize behavior on whether we /think/ deadlock is
  // XXX about to happen.  for example:
  // XXX   if (maybeImminent)
  //           NS_RUNTIMEABORT(out.get());
  NS_ERROR(out.get());
}


void
BlockingResourceBase::Acquire()
{
  if (mType == eCondVar) {
    NS_NOTYETIMPLEMENTED(
      "FIXME bug 456272: annots. to allow Acquire()ing condvars");
    return;
  }
  NS_ASSERTION(!IsAcquired(),
               "reacquiring already acquired resource");

  ResourceChainAppend(ResourceChainFront());

#ifdef MOZ_CALLSTACK_DISABLED
  mAcquired = true;
#else
  // Take a stack snapshot.
  GetStackTrace(mAcquired);
  if (mFirstSeen.IsEmpty()) {
    mFirstSeen = mAcquired;
  }
#endif
}


void
BlockingResourceBase::Release()
{
  if (mType == eCondVar) {
    NS_NOTYETIMPLEMENTED(
      "FIXME bug 456272: annots. to allow Release()ing condvars");
    return;
  }

  BlockingResourceBase* chainFront = ResourceChainFront();
  NS_ASSERTION(chainFront && IsAcquired(),
               "Release()ing something that hasn't been Acquire()ed");

  if (chainFront == this) {
    ResourceChainRemove();
  } else {
    // not an error, but makes code hard to reason about.
    NS_WARNING("Resource acquired at calling context\n");
    NS_WARNING("  [stack trace unavailable]\n");
    NS_WARNING("\nis being released in non-LIFO order; why?");

    // remove this resource from wherever it lives in the chain
    // we walk backwards in order of acquisition:
    //  (1)  ...node<-prev<-curr...
    //              /     /
    //  (2)  ...prev<-curr...
    BlockingResourceBase* curr = chainFront;
    BlockingResourceBase* prev = nullptr;
    while (curr && (prev = curr->mChainPrev) && (prev != this)) {
      curr = prev;
    }
    if (prev == this) {
      curr->mChainPrev = prev->mChainPrev;
    }
  }

  ClearAcquisitionState();
}


//
// Debug implementation of (OffTheBooks)Mutex
void
OffTheBooksMutex::Lock()
{
  CheckAcquire();
  PR_Lock(mLock);
  Acquire();       // protected by mLock
}

void
OffTheBooksMutex::Unlock()
{
  Release();                  // protected by mLock
  PRStatus status = PR_Unlock(mLock);
  NS_ASSERTION(PR_SUCCESS == status, "bad Mutex::Unlock()");
}


//
// Debug implementation of ReentrantMonitor
void
ReentrantMonitor::Enter()
{
  BlockingResourceBase* chainFront = ResourceChainFront();

  // the code below implements monitor reentrancy semantics

  if (this == chainFront) {
    // immediately re-entered the monitor: acceptable
    PR_EnterMonitor(mReentrantMonitor);
    ++mEntryCount;
    return;
  }

  // this is sort of a hack around not recording the thread that
  // owns this monitor
  if (chainFront) {
    for (BlockingResourceBase* br = ResourceChainPrev(chainFront);
         br;
         br = ResourceChainPrev(br)) {
      if (br == this) {
        NS_WARNING(
          "Re-entering ReentrantMonitor after acquiring other resources.\n"
          "At calling context\n"
          "  [stack trace unavailable]\n");

        // show the caller why this is potentially bad
        CheckAcquire();

        PR_EnterMonitor(mReentrantMonitor);
        ++mEntryCount;
        return;
      }
    }
  }

  CheckAcquire();
  PR_EnterMonitor(mReentrantMonitor);
  NS_ASSERTION(mEntryCount == 0, "ReentrantMonitor isn't free!");
  Acquire();       // protected by mReentrantMonitor
  mEntryCount = 1;
}

void
ReentrantMonitor::Exit()
{
  if (--mEntryCount == 0) {
    Release();              // protected by mReentrantMonitor
  }
  PRStatus status = PR_ExitMonitor(mReentrantMonitor);
  NS_ASSERTION(PR_SUCCESS == status, "bad ReentrantMonitor::Exit()");
}

nsresult
ReentrantMonitor::Wait(PRIntervalTime aInterval)
{
  AssertCurrentThreadIn();

  // save monitor state and reset it to empty
  int32_t savedEntryCount = mEntryCount;
  AcquisitionState savedAcquisitionState = GetAcquisitionState();
  BlockingResourceBase* savedChainPrev = mChainPrev;
  mEntryCount = 0;
  ClearAcquisitionState();
  mChainPrev = 0;

  nsresult rv;
#ifdef MOZILLA_INTERNAL_API
  {
    GeckoProfilerSleepRAII profiler_sleep;
#endif //MOZILLA_INTERNAL_API

    // give up the monitor until we're back from Wait()
    rv = PR_Wait(mReentrantMonitor, aInterval) == PR_SUCCESS ? NS_OK :
                                                               NS_ERROR_FAILURE;

#ifdef MOZILLA_INTERNAL_API
  }
#endif //MOZILLA_INTERNAL_API

  // restore saved state
  mEntryCount = savedEntryCount;
  SetAcquisitionState(savedAcquisitionState);
  mChainPrev = savedChainPrev;

  return rv;
}


//
// Debug implementation of CondVar
nsresult
CondVar::Wait(PRIntervalTime aInterval)
{
  AssertCurrentThreadOwnsMutex();

  // save mutex state and reset to empty
  AcquisitionState savedAcquisitionState = mLock->GetAcquisitionState();
  BlockingResourceBase* savedChainPrev = mLock->mChainPrev;
  mLock->ClearAcquisitionState();
  mLock->mChainPrev = 0;

  // give up mutex until we're back from Wait()
  nsresult rv =
    PR_WaitCondVar(mCvar, aInterval) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;

  // restore saved state
  mLock->SetAcquisitionState(savedAcquisitionState);
  mLock->mChainPrev = savedChainPrev;

  return rv;
}

#endif // ifdef DEBUG


} // namespace mozilla
