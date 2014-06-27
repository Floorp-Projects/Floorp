/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BlockingResourceBase.h"

#ifdef DEBUG
#include "nsAutoPtr.h"

#include "mozilla/CondVar.h"
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
const char* const BlockingResourceBase::kResourceTypeName[] =
{
  // needs to be kept in sync with BlockingResourceType
  "Mutex", "ReentrantMonitor", "CondVar"
};

#ifdef DEBUG

PRCallOnceType BlockingResourceBase::sCallOnce;
unsigned BlockingResourceBase::sResourceAcqnChainFrontTPI = (unsigned)-1;
BlockingResourceBase::DDT* BlockingResourceBase::sDeadlockDetector;

bool
BlockingResourceBase::DeadlockDetectorEntry::Print(
    const DDT::ResourceAcquisition& aFirstSeen,
    nsACString& aOut,
    bool aPrintFirstSeenCx) const
{
  CallStack lastAcquisition = mAcquisitionContext; // RACY, but benign
  bool maybeCurrentlyAcquired = (CallStack::kNone != lastAcquisition);
  CallStack printAcquisition =
    (aPrintFirstSeenCx || !maybeCurrentlyAcquired) ?
      aFirstSeen.mCallContext : lastAcquisition;

  fprintf(stderr, "--- %s : %s",
          kResourceTypeName[mType], mName);
  aOut += BlockingResourceBase::kResourceTypeName[mType];
  aOut += " : ";
  aOut += mName;

  if (maybeCurrentlyAcquired) {
    fputs(" (currently acquired)\n", stderr);
    aOut += " (currently acquired)\n";
  }

  fputs(" calling context\n", stderr);
  printAcquisition.Print(stderr);

  return maybeCurrentlyAcquired;
}


BlockingResourceBase::BlockingResourceBase(
    const char* aName,
    BlockingResourceBase::BlockingResourceType aType)
{
  // PR_CallOnce guaranatees that InitStatics is called in a
  // thread-safe way
  if (PR_SUCCESS != PR_CallOnce(&sCallOnce, InitStatics)) {
    NS_RUNTIMEABORT("can't initialize blocking resource static members");
  }

  mDDEntry = new BlockingResourceBase::DeadlockDetectorEntry(aName, aType);
  mChainPrev = 0;
  sDeadlockDetector->Add(mDDEntry);
}


BlockingResourceBase::~BlockingResourceBase()
{
  // we don't check for really obviously bad things like freeing
  // Mutexes while they're still locked.  it is assumed that the
  // base class, or its underlying primitive, will check for such
  // stupid mistakes.
  mChainPrev = 0;             // racy only for stupidly buggy client code
  mDDEntry = 0;               // owned by deadlock detector
}


void
BlockingResourceBase::CheckAcquire(const CallStack& aCallContext)
{
  if (mDDEntry->mType == eCondVar) {
    NS_NOTYETIMPLEMENTED(
      "FIXME bug 456272: annots. to allow CheckAcquire()ing condvars");
    return;
  }

  BlockingResourceBase* chainFront = ResourceChainFront();
  nsAutoPtr<DDT::ResourceAcquisitionArray> cycle(
    sDeadlockDetector->CheckAcquisition(
      chainFront ? chainFront->mDDEntry : 0, mDDEntry, aCallContext));
  if (!cycle) {
    return;
  }

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
BlockingResourceBase::Acquire(const CallStack& aCallContext)
{
  if (mDDEntry->mType == eCondVar) {
    NS_NOTYETIMPLEMENTED(
      "FIXME bug 456272: annots. to allow Acquire()ing condvars");
    return;
  }
  NS_ASSERTION(mDDEntry->mAcquisitionContext == CallStack::kNone,
               "reacquiring already acquired resource");

  ResourceChainAppend(ResourceChainFront());
  mDDEntry->mAcquisitionContext = aCallContext;
}


void
BlockingResourceBase::Release()
{
  if (mDDEntry->mType == eCondVar) {
    NS_NOTYETIMPLEMENTED(
      "FIXME bug 456272: annots. to allow Release()ing condvars");
    return;
  }

  BlockingResourceBase* chainFront = ResourceChainFront();
  NS_ASSERTION(chainFront && mDDEntry->mAcquisitionContext != CallStack::kNone,
               "Release()ing something that hasn't been Acquire()ed");

  if (chainFront == this) {
    ResourceChainRemove();
  } else {
    // not an error, but makes code hard to reason about.
    NS_WARNING("Resource acquired at calling context\n");
    mDDEntry->mAcquisitionContext.Print(stderr);
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

  mDDEntry->mAcquisitionContext = CallStack::kNone;
}


bool
BlockingResourceBase::PrintCycle(const DDT::ResourceAcquisitionArray* aCycle,
                                 nsACString& aOut)
{
  NS_ASSERTION(aCycle->Length() > 1, "need > 1 element for cycle!");

  bool maybeImminent = true;

  fputs("=== Cyclical dependency starts at\n", stderr);
  aOut += "Cyclical dependency starts at\n";

  const DDT::ResourceAcquisition res = aCycle->ElementAt(0);
  maybeImminent &= res.mResource->Print(res, aOut);

  DDT::ResourceAcquisitionArray::index_type i;
  DDT::ResourceAcquisitionArray::size_type len = aCycle->Length();
  const DDT::ResourceAcquisition* it = 1 + aCycle->Elements();
  for (i = 1; i < len - 1; ++i, ++it) {
    fputs("\n--- Next dependency:\n", stderr);
    aOut += "\nNext dependency:\n";

    maybeImminent &= it->mResource->Print(*it, aOut);
  }

  fputs("\n=== Cycle completed at\n", stderr);
  aOut += "Cycle completed at\n";
  it->mResource->Print(*it, aOut, true);

  return maybeImminent;
}


//
// Debug implementation of (OffTheBooks)Mutex
void
OffTheBooksMutex::Lock()
{
  CallStack callContext = CallStack();

  CheckAcquire(callContext);
  PR_Lock(mLock);
  Acquire(callContext);       // protected by mLock
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

  CallStack callContext = CallStack();

  // this is sort of a hack around not recording the thread that
  // owns this monitor
  if (chainFront) {
    for (BlockingResourceBase* br = ResourceChainPrev(chainFront);
         br;
         br = ResourceChainPrev(br)) {
      if (br == this) {
        NS_WARNING(
          "Re-entering ReentrantMonitor after acquiring other resources.\n"
          "At calling context\n");
        GetAcquisitionContext().Print(stderr);

        // show the caller why this is potentially bad
        CheckAcquire(callContext);

        PR_EnterMonitor(mReentrantMonitor);
        ++mEntryCount;
        return;
      }
    }
  }

  CheckAcquire(callContext);
  PR_EnterMonitor(mReentrantMonitor);
  NS_ASSERTION(mEntryCount == 0, "ReentrantMonitor isn't free!");
  Acquire(callContext);       // protected by mReentrantMonitor
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
  CallStack savedAcquisitionContext = GetAcquisitionContext();
  BlockingResourceBase* savedChainPrev = mChainPrev;
  mEntryCount = 0;
  SetAcquisitionContext(CallStack::kNone);
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
  SetAcquisitionContext(savedAcquisitionContext);
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
  CallStack savedAcquisitionContext = mLock->GetAcquisitionContext();
  BlockingResourceBase* savedChainPrev = mLock->mChainPrev;
  mLock->SetAcquisitionContext(CallStack::kNone);
  mLock->mChainPrev = 0;

  // give up mutex until we're back from Wait()
  nsresult rv =
    PR_WaitCondVar(mCvar, aInterval) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;

  // restore saved state
  mLock->SetAcquisitionContext(savedAcquisitionContext);
  mLock->mChainPrev = savedChainPrev;

  return rv;
}

#endif // ifdef DEBUG


} // namespace mozilla
