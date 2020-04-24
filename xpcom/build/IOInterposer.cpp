/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <vector>

#include "IOInterposer.h"

#include "IOInterposerPrivate.h"
#include "MainThreadIOLogger.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#include "nscore.h"  // for NS_FREE_PERMANENT_DATA
#if !defined(XP_WIN)
#  include "NSPRInterposer.h"
#endif  // !defined(XP_WIN)
#include "nsXULAppAPI.h"
#include "PoisonIOInterposer.h"
#include "prenv.h"

namespace {

/** Find if a vector contains a specific element */
template <class T>
bool VectorContains(const std::vector<T>& aVector, const T& aElement) {
  return std::find(aVector.begin(), aVector.end(), aElement) != aVector.end();
}

/** Remove element from a vector */
template <class T>
void VectorRemove(std::vector<T>& aVector, const T& aElement) {
  typename std::vector<T>::iterator newEnd =
      std::remove(aVector.begin(), aVector.end(), aElement);
  aVector.erase(newEnd, aVector.end());
}

/** Lists of Observers */
struct ObserverLists {
 private:
  ~ObserverLists() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ObserverLists)

  ObserverLists() = default;

  ObserverLists(ObserverLists const& aOther)
      : mCreateObservers(aOther.mCreateObservers),
        mReadObservers(aOther.mReadObservers),
        mWriteObservers(aOther.mWriteObservers),
        mFSyncObservers(aOther.mFSyncObservers),
        mStatObservers(aOther.mStatObservers),
        mCloseObservers(aOther.mCloseObservers),
        mStageObservers(aOther.mStageObservers) {}
  // Lists of observers for I/O events.
  // These are implemented as vectors since they are allowed to survive gecko,
  // without reporting leaks. This is necessary for the IOInterposer to be used
  // for late-write checks.
  std::vector<mozilla::IOInterposeObserver*> mCreateObservers;
  std::vector<mozilla::IOInterposeObserver*> mReadObservers;
  std::vector<mozilla::IOInterposeObserver*> mWriteObservers;
  std::vector<mozilla::IOInterposeObserver*> mFSyncObservers;
  std::vector<mozilla::IOInterposeObserver*> mStatObservers;
  std::vector<mozilla::IOInterposeObserver*> mCloseObservers;
  std::vector<mozilla::IOInterposeObserver*> mStageObservers;
};

class PerThreadData {
 public:
  explicit PerThreadData(bool aIsMainThread = false)
      : mIsMainThread(aIsMainThread),
        mIsHandlingObservation(false),
        mCurrentGeneration(0) {
    MOZ_COUNT_CTOR(PerThreadData);
  }

  MOZ_COUNTED_DTOR(PerThreadData)

  void CallObservers(mozilla::IOInterposeObserver::Observation& aObservation) {
    // Prevent recursive reporting.
    if (mIsHandlingObservation) {
      return;
    }

    mIsHandlingObservation = true;
    // Decide which list of observers to inform
    std::vector<mozilla::IOInterposeObserver*>* observers = nullptr;
    switch (aObservation.ObservedOperation()) {
      case mozilla::IOInterposeObserver::OpCreateOrOpen:
        observers = &mObserverLists->mCreateObservers;
        break;
      case mozilla::IOInterposeObserver::OpRead:
        observers = &mObserverLists->mReadObservers;
        break;
      case mozilla::IOInterposeObserver::OpWrite:
        observers = &mObserverLists->mWriteObservers;
        break;
      case mozilla::IOInterposeObserver::OpFSync:
        observers = &mObserverLists->mFSyncObservers;
        break;
      case mozilla::IOInterposeObserver::OpStat:
        observers = &mObserverLists->mStatObservers;
        break;
      case mozilla::IOInterposeObserver::OpClose:
        observers = &mObserverLists->mCloseObservers;
        break;
      case mozilla::IOInterposeObserver::OpNextStage:
        observers = &mObserverLists->mStageObservers;
        break;
      default: {
        // Invalid IO operation, see documentation comment for
        // IOInterposer::Report()
        MOZ_ASSERT(false);
        // Just ignore it in non-debug builds.
        return;
      }
    }
    MOZ_ASSERT(observers);

    // Inform observers
    for (auto i = observers->begin(), e = observers->end(); i != e; ++i) {
      (*i)->Observe(aObservation);
    }
    mIsHandlingObservation = false;
  }

  inline uint32_t GetCurrentGeneration() const { return mCurrentGeneration; }

  inline bool IsMainThread() const { return mIsMainThread; }

  inline void SetObserverLists(uint32_t aNewGeneration,
                               RefPtr<ObserverLists>& aNewLists) {
    mCurrentGeneration = aNewGeneration;
    mObserverLists = aNewLists;
  }

  inline void ClearObserverLists() {
    if (mObserverLists) {
      mCurrentGeneration = 0;
      mObserverLists = nullptr;
    }
  }

 private:
  bool mIsMainThread;
  bool mIsHandlingObservation;
  uint32_t mCurrentGeneration;
  RefPtr<ObserverLists> mObserverLists;
};

class MasterList {
 public:
  MasterList()
      : mObservedOperations(mozilla::IOInterposeObserver::OpNone),
        mIsEnabled(true) {
    MOZ_COUNT_CTOR(MasterList);
  }

  MOZ_COUNTED_DTOR(MasterList)

  inline void Disable() { mIsEnabled = false; }
  inline void Enable() { mIsEnabled = true; }

  void Register(mozilla::IOInterposeObserver::Operation aOp,
                mozilla::IOInterposeObserver* aObserver) {
    mozilla::IOInterposer::AutoLock lock(mLock);

    ObserverLists* newLists = nullptr;
    if (mObserverLists) {
      newLists = new ObserverLists(*mObserverLists);
    } else {
      newLists = new ObserverLists();
    }
    // You can register to observe multiple types of observations
    // but you'll never be registered twice for the same observations.
    if (aOp & mozilla::IOInterposeObserver::OpCreateOrOpen &&
        !VectorContains(newLists->mCreateObservers, aObserver)) {
      newLists->mCreateObservers.push_back(aObserver);
    }
    if (aOp & mozilla::IOInterposeObserver::OpRead &&
        !VectorContains(newLists->mReadObservers, aObserver)) {
      newLists->mReadObservers.push_back(aObserver);
    }
    if (aOp & mozilla::IOInterposeObserver::OpWrite &&
        !VectorContains(newLists->mWriteObservers, aObserver)) {
      newLists->mWriteObservers.push_back(aObserver);
    }
    if (aOp & mozilla::IOInterposeObserver::OpFSync &&
        !VectorContains(newLists->mFSyncObservers, aObserver)) {
      newLists->mFSyncObservers.push_back(aObserver);
    }
    if (aOp & mozilla::IOInterposeObserver::OpStat &&
        !VectorContains(newLists->mStatObservers, aObserver)) {
      newLists->mStatObservers.push_back(aObserver);
    }
    if (aOp & mozilla::IOInterposeObserver::OpClose &&
        !VectorContains(newLists->mCloseObservers, aObserver)) {
      newLists->mCloseObservers.push_back(aObserver);
    }
    if (aOp & mozilla::IOInterposeObserver::OpNextStage &&
        !VectorContains(newLists->mStageObservers, aObserver)) {
      newLists->mStageObservers.push_back(aObserver);
    }
    mObserverLists = newLists;
    mObservedOperations =
        (mozilla::IOInterposeObserver::Operation)(mObservedOperations | aOp);

    mCurrentGeneration++;
  }

  void Unregister(mozilla::IOInterposeObserver::Operation aOp,
                  mozilla::IOInterposeObserver* aObserver) {
    mozilla::IOInterposer::AutoLock lock(mLock);

    ObserverLists* newLists = nullptr;
    if (mObserverLists) {
      newLists = new ObserverLists(*mObserverLists);
    } else {
      newLists = new ObserverLists();
    }

    if (aOp & mozilla::IOInterposeObserver::OpCreateOrOpen) {
      VectorRemove(newLists->mCreateObservers, aObserver);
      if (newLists->mCreateObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations &
            ~mozilla::IOInterposeObserver::OpCreateOrOpen);
      }
    }
    if (aOp & mozilla::IOInterposeObserver::OpRead) {
      VectorRemove(newLists->mReadObservers, aObserver);
      if (newLists->mReadObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations & ~mozilla::IOInterposeObserver::OpRead);
      }
    }
    if (aOp & mozilla::IOInterposeObserver::OpWrite) {
      VectorRemove(newLists->mWriteObservers, aObserver);
      if (newLists->mWriteObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations & ~mozilla::IOInterposeObserver::OpWrite);
      }
    }
    if (aOp & mozilla::IOInterposeObserver::OpFSync) {
      VectorRemove(newLists->mFSyncObservers, aObserver);
      if (newLists->mFSyncObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations & ~mozilla::IOInterposeObserver::OpFSync);
      }
    }
    if (aOp & mozilla::IOInterposeObserver::OpStat) {
      VectorRemove(newLists->mStatObservers, aObserver);
      if (newLists->mStatObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations & ~mozilla::IOInterposeObserver::OpStat);
      }
    }
    if (aOp & mozilla::IOInterposeObserver::OpClose) {
      VectorRemove(newLists->mCloseObservers, aObserver);
      if (newLists->mCloseObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations & ~mozilla::IOInterposeObserver::OpClose);
      }
    }
    if (aOp & mozilla::IOInterposeObserver::OpNextStage) {
      VectorRemove(newLists->mStageObservers, aObserver);
      if (newLists->mStageObservers.empty()) {
        mObservedOperations = (mozilla::IOInterposeObserver::Operation)(
            mObservedOperations & ~mozilla::IOInterposeObserver::OpNextStage);
      }
    }
    mObserverLists = newLists;
    mCurrentGeneration++;
  }

  void Update(PerThreadData& aPtd) {
    if (mCurrentGeneration == aPtd.GetCurrentGeneration()) {
      return;
    }
    // If the generation counts don't match then we need to update the current
    // thread's observer list with the new master list.
    mozilla::IOInterposer::AutoLock lock(mLock);
    aPtd.SetObserverLists(mCurrentGeneration, mObserverLists);
  }

  inline bool IsObservedOperation(mozilla::IOInterposeObserver::Operation aOp) {
    // This does not occur inside of a lock, so this makes no guarantees that
    // the observers are in sync with this. That is acceptable; it is not a
    // problem if we occasionally report more or less IO than is actually
    // occurring.
    return mIsEnabled && !!(mObservedOperations & aOp);
  }

 private:
  RefPtr<ObserverLists> mObserverLists;
  // Note, we cannot use mozilla::Mutex here as the ObserverLists may be leaked
  // (We want to monitor IO during shutdown). Furthermore, as we may have to
  // unregister observers during shutdown an OffTheBooksMutex is not an option
  // either, as its base calls into sDeadlockDetector which may be nullptr
  // during shutdown.
  mozilla::IOInterposer::Mutex mLock;
  // Flags tracking which operations are being observed
  mozilla::Atomic<mozilla::IOInterposeObserver::Operation,
                  mozilla::MemoryOrdering::Relaxed>
      mObservedOperations;
  // Used for quickly disabling everything by IOInterposer::Disable()
  mozilla::Atomic<bool> mIsEnabled;
  // Used to inform threads that the master observer list has changed
  mozilla::Atomic<uint32_t> mCurrentGeneration;
};

// Special observation used by IOInterposer::EnteringNextStage()
class NextStageObservation : public mozilla::IOInterposeObserver::Observation {
 public:
  NextStageObservation()
      : mozilla::IOInterposeObserver::Observation(
            mozilla::IOInterposeObserver::OpNextStage, "IOInterposer", false) {
    mStart = mozilla::TimeStamp::Now();
    mEnd = mStart;
  }
};

// List of observers registered
static mozilla::StaticAutoPtr<MasterList> sMasterList;
static MOZ_THREAD_LOCAL(PerThreadData*) sThreadLocalData;
static bool sThreadLocalDataInitialized;

}  // anonymous namespace

namespace mozilla {

IOInterposeObserver::Observation::Observation(Operation aOperation,
                                              const char* aReference,
                                              bool aShouldReport)
    : mOperation(aOperation),
      mReference(aReference),
      mShouldReport(IOInterposer::IsObservedOperation(aOperation) &&
                    aShouldReport) {
  if (mShouldReport) {
    mStart = TimeStamp::Now();
  }
}

IOInterposeObserver::Observation::Observation(Operation aOperation,
                                              const TimeStamp& aStart,
                                              const TimeStamp& aEnd,
                                              const char* aReference)
    : mOperation(aOperation),
      mStart(aStart),
      mEnd(aEnd),
      mReference(aReference),
      mShouldReport(false) {}

const char* IOInterposeObserver::Observation::ObservedOperationString() const {
  switch (mOperation) {
    case OpCreateOrOpen:
      return "create/open";
    case OpRead:
      return "read";
    case OpWrite:
      return "write";
    case OpFSync:
      return "fsync";
    case OpStat:
      return "stat";
    case OpClose:
      return "close";
    case OpNextStage:
      return "NextStage";
    default:
      return "unknown";
  }
}

void IOInterposeObserver::Observation::Report() {
  if (mShouldReport) {
    mEnd = TimeStamp::Now();
    IOInterposer::Report(*this);
  }
}

bool IOInterposer::Init() {
  // Don't initialize twice...
  if (sMasterList) {
    return true;
  }
  if (!sThreadLocalData.init()) {
    return false;
  }
  sThreadLocalDataInitialized = true;
  bool isMainThread = true;
  RegisterCurrentThread(isMainThread);
  sMasterList = new MasterList();

  MainThreadIOLogger::Init();

  // Now we initialize the various interposers depending on platform

  // Under certain conditions it may be unsafe to initialize PoisonIOInterposer,
  // such as when a background thread is already running. We set this variable
  // elsewhere when such a condition applies.
  if (!PR_GetEnv("MOZ_DISABLE_POISON_IO_INTERPOSER")) {
    InitPoisonIOInterposer();
  }

  // We don't hook NSPR on Windows because PoisonIOInterposer captures a
  // superset of the former's events.
#if !defined(XP_WIN)
  InitNSPRIOInterposing();
#endif
  return true;
}

bool IOInterposeObserver::IsMainThread() {
  if (!sThreadLocalDataInitialized) {
    return false;
  }
  PerThreadData* ptd = sThreadLocalData.get();
  if (!ptd) {
    return false;
  }
  return ptd->IsMainThread();
}

void IOInterposer::Clear() {
  /* Clear() is a no-op on release builds so that we may continue to trap I/O
     until process termination. In leak-checking builds, we need to shut down
     IOInterposer so that all references are properly released. */
#ifdef NS_FREE_PERMANENT_DATA
  UnregisterCurrentThread();
  sMasterList = nullptr;
#endif
}

void IOInterposer::Disable() {
  if (!sMasterList) {
    return;
  }
  sMasterList->Disable();
}

void IOInterposer::Enable() {
  if (!sMasterList) {
    return;
  }
  sMasterList->Enable();
}

void IOInterposer::Report(IOInterposeObserver::Observation& aObservation) {
  PerThreadData* ptd = sThreadLocalData.get();
  if (!ptd) {
    // In this case the current thread is not registered with IOInterposer.
    // Alternatively we could take the slow path and just lock everything if
    // we're not registered. That could potentially perform poorly, though.
    return;
  }

  if (!sMasterList) {
    // If there is no longer a master list then we should clear the local one.
    ptd->ClearObserverLists();
    return;
  }

  sMasterList->Update(*ptd);

  // Don't try to report if there's nobody listening.
  if (!IOInterposer::IsObservedOperation(aObservation.ObservedOperation())) {
    return;
  }

  ptd->CallObservers(aObservation);
}

bool IOInterposer::IsObservedOperation(IOInterposeObserver::Operation aOp) {
  return sMasterList && sMasterList->IsObservedOperation(aOp);
}

void IOInterposer::Register(IOInterposeObserver::Operation aOp,
                            IOInterposeObserver* aObserver) {
  MOZ_ASSERT(aObserver);
  if (!sMasterList || !aObserver) {
    return;
  }

  sMasterList->Register(aOp, aObserver);
}

void IOInterposer::Unregister(IOInterposeObserver::Operation aOp,
                              IOInterposeObserver* aObserver) {
  if (!sMasterList) {
    return;
  }

  sMasterList->Unregister(aOp, aObserver);
}

void IOInterposer::RegisterCurrentThread(bool aIsMainThread) {
  if (!sThreadLocalDataInitialized) {
    return;
  }
  MOZ_ASSERT(!sThreadLocalData.get());
  PerThreadData* curThreadData = new PerThreadData(aIsMainThread);
  sThreadLocalData.set(curThreadData);
}

void IOInterposer::UnregisterCurrentThread() {
  if (!sThreadLocalDataInitialized) {
    return;
  }
  PerThreadData* curThreadData = sThreadLocalData.get();
  MOZ_ASSERT(curThreadData);
  sThreadLocalData.set(nullptr);
  delete curThreadData;
}

void IOInterposer::EnteringNextStage() {
  if (!sMasterList) {
    return;
  }
  NextStageObservation observation;
  Report(observation);
}

}  // namespace mozilla
