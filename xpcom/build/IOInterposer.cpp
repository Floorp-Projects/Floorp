/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <vector>

#include "IOInterposer.h"

#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#if defined(MOZILLA_INTERNAL_API)
// We need to undefine MOZILLA_INTERNAL_API for RefPtr.h because IOInterposer
// does not clean up its data before shutdown.
#undef MOZILLA_INTERNAL_API
#include "mozilla/RefPtr.h"
#define MOZILLA_INTERNAL_API
#else
#include "mozilla/RefPtr.h"
#endif // defined(MOZILLA_INTERNAL_API)
#include "mozilla/StaticPtr.h"
#include "mozilla/ThreadLocal.h"
#if !defined(XP_WIN)
#include "NSPRInterposer.h"
#endif // !defined(XP_WIN)
#include "nsXULAppAPI.h"
#include "PoisonIOInterposer.h"

using namespace mozilla;

namespace {

/** Find if a vector contains a specific element */
template<class T>
bool VectorContains(const std::vector<T>& vector, const T& element)
{
  return std::find(vector.begin(), vector.end(), element) != vector.end();
}

/** Remove element from a vector */
template<class T>
void VectorRemove(std::vector<T>& vector, const T& element)
{
  typename std::vector<T>::iterator newEnd = std::remove(vector.begin(),
                                                         vector.end(), element);
  vector.erase(newEnd, vector.end());
}

/** Lists of Observers */
struct ObserverLists : public AtomicRefCounted<ObserverLists>
{
  ObserverLists()
  {
  }

  ObserverLists(ObserverLists const & aOther)
    : mCreateObservers(aOther.mCreateObservers)
    , mReadObservers(aOther.mReadObservers)
    , mWriteObservers(aOther.mWriteObservers)
    , mFSyncObservers(aOther.mFSyncObservers)
    , mStatObservers(aOther.mStatObservers)
    , mCloseObservers(aOther.mCloseObservers)
  {
  }
  // Lists of observers for read, write and fsync events respectively
  // These are implemented as vectors since they are allowed to survive gecko,
  // without reporting leaks. This is necessary for the IOInterposer to be used
  // for late-write checks.
  std::vector<IOInterposeObserver*>  mCreateObservers;
  std::vector<IOInterposeObserver*>  mReadObservers;
  std::vector<IOInterposeObserver*>  mWriteObservers;
  std::vector<IOInterposeObserver*>  mFSyncObservers;
  std::vector<IOInterposeObserver*>  mStatObservers;
  std::vector<IOInterposeObserver*>  mCloseObservers;
};

/**
 * A quick and dirty RAII class to automatically lock a PRLock
 */
class AutoPRLock
{
  PRLock* mLock;
public:
  AutoPRLock(PRLock* aLock)
   : mLock(aLock)
  {
    PR_Lock(aLock);
  }
  ~AutoPRLock()
  {
    PR_Unlock(mLock);
  }
};

class PerThreadData
{
public:
  PerThreadData(bool aIsMainThread = false)
    : mIsMainThread(aIsMainThread)
    , mIsHandlingObservation(false)
    , mCurrentGeneration(0)
  {
  }

  void
  CallObservers(IOInterposeObserver::Observation& aObservation)
  {
    // Prevent recursive reporting.
    if (mIsHandlingObservation) {
      return;
    }

    mIsHandlingObservation = true;
    // Decide which list of observers to inform
    std::vector<IOInterposeObserver*>* observers = nullptr;
    switch (aObservation.ObservedOperation()) {
      case IOInterposeObserver::OpCreateOrOpen:
        {
          observers = &mObserverLists->mCreateObservers;
        }
        break;
      case IOInterposeObserver::OpRead:
        {
          observers = &mObserverLists->mReadObservers;
        }
        break;
      case IOInterposeObserver::OpWrite:
        {
          observers = &mObserverLists->mWriteObservers;
        }
        break;
      case IOInterposeObserver::OpFSync:
        {
          observers = &mObserverLists->mFSyncObservers;
        }
        break;
      case IOInterposeObserver::OpStat:
        {
          observers = &mObserverLists->mStatObservers;
        }
        break;
      case IOInterposeObserver::OpClose:
        {
          observers = &mObserverLists->mCloseObservers;
        }
        break;
      default:
        {
          // Invalid IO operation, see documentation comment for
          // IOInterposer::Report()
          MOZ_ASSERT(false);
          // Just ignore it in non-debug builds.
          return;
        }
    }
    MOZ_ASSERT(observers);

    // Inform observers
    for (std::vector<IOInterposeObserver*>::iterator i = observers->begin(),
         e = observers->end(); i != e; ++i)
    {
      (*i)->Observe(aObservation);
    }
    mIsHandlingObservation = false;
  }

  inline uint32_t
  GetCurrentGeneration() const
  {
    return mCurrentGeneration;
  }

  inline bool
  IsMainThread() const
  {
    return mIsMainThread;
  }

  inline void
  SetObserverLists(uint32_t aNewGeneration, RefPtr<ObserverLists>& aNewLists)
  {
    mCurrentGeneration = aNewGeneration;
    mObserverLists = aNewLists;
  }

private:
  bool                  mIsMainThread;
  bool                  mIsHandlingObservation;
  uint32_t              mCurrentGeneration;
  RefPtr<ObserverLists> mObserverLists;
};

class MasterList
{
public:
  MasterList()
    : mLock(PR_NewLock())
    , mObservedOperations(IOInterposeObserver::OpNone)
    , mIsEnabled(true)
  {
  }

  ~MasterList()
  {
    PR_DestroyLock(mLock);
    mLock = nullptr;
  }

  inline void
  Disable()
  {
    mIsEnabled = false;
  }

  void
  Register(IOInterposeObserver::Operation aOp, IOInterposeObserver* aObserver)
  {
    AutoPRLock lock(mLock);

    ObserverLists* newLists = nullptr;
    if (mObserverLists) {
      newLists = new ObserverLists(*mObserverLists);
    } else {
      newLists = new ObserverLists();
    }
    // You can register to observe multiple types of observations
    // but you'll never be registered twice for the same observations.
    if (aOp & IOInterposeObserver::OpCreateOrOpen &&
        !VectorContains(newLists->mCreateObservers, aObserver)) {
      newLists->mCreateObservers.push_back(aObserver);
    }
    if (aOp & IOInterposeObserver::OpRead &&
        !VectorContains(newLists->mReadObservers, aObserver)) {
      newLists->mReadObservers.push_back(aObserver);
    }
    if (aOp & IOInterposeObserver::OpWrite &&
        !VectorContains(newLists->mWriteObservers, aObserver)) {
      newLists->mWriteObservers.push_back(aObserver);
    }
    if (aOp & IOInterposeObserver::OpFSync &&
        !VectorContains(newLists->mFSyncObservers, aObserver)) {
      newLists->mFSyncObservers.push_back(aObserver);
    }
    if (aOp & IOInterposeObserver::OpStat &&
        !VectorContains(newLists->mStatObservers, aObserver)) {
      newLists->mStatObservers.push_back(aObserver);
    }
    if (aOp & IOInterposeObserver::OpClose &&
        !VectorContains(newLists->mCloseObservers, aObserver)) {
      newLists->mCloseObservers.push_back(aObserver);
    }
    mObserverLists = newLists;
    mObservedOperations = (IOInterposeObserver::Operation)
                            (mObservedOperations | aOp);

    mCurrentGeneration++;
  }

  void
  Unregister(IOInterposeObserver::Operation aOp, IOInterposeObserver* aObserver)
  {
    AutoPRLock lock(mLock);

    ObserverLists* newLists = nullptr;
    if (mObserverLists) {
      newLists = new ObserverLists(*mObserverLists);
    } else {
      newLists = new ObserverLists();
    }

    if (aOp & IOInterposeObserver::OpCreateOrOpen) {
      VectorRemove(newLists->mCreateObservers, aObserver);
      if (newLists->mCreateObservers.empty()) {
        mObservedOperations = (IOInterposeObserver::Operation)
                         (mObservedOperations &
                          ~IOInterposeObserver::OpCreateOrOpen);
      }
    }
    if (aOp & IOInterposeObserver::OpRead) {
      VectorRemove(newLists->mReadObservers, aObserver);
      if (newLists->mReadObservers.empty()) {
        mObservedOperations = (IOInterposeObserver::Operation)
                         (mObservedOperations & ~IOInterposeObserver::OpRead);
      }
    }
    if (aOp & IOInterposeObserver::OpWrite) {
      VectorRemove(newLists->mWriteObservers, aObserver);
      if (newLists->mWriteObservers.empty()) {
        mObservedOperations = (IOInterposeObserver::Operation)
                         (mObservedOperations & ~IOInterposeObserver::OpWrite);
      }
    }
    if (aOp & IOInterposeObserver::OpFSync) {
      VectorRemove(newLists->mFSyncObservers, aObserver);
      if (newLists->mFSyncObservers.empty()) {
        mObservedOperations = (IOInterposeObserver::Operation)
                         (mObservedOperations & ~IOInterposeObserver::OpFSync);
      }
    }
    if (aOp & IOInterposeObserver::OpStat) {
      VectorRemove(newLists->mStatObservers, aObserver);
      if (newLists->mStatObservers.empty()) {
        mObservedOperations = (IOInterposeObserver::Operation)
                         (mObservedOperations & ~IOInterposeObserver::OpStat);
      }
    }
    if (aOp & IOInterposeObserver::OpClose) {
      VectorRemove(newLists->mCloseObservers, aObserver);
      if (newLists->mCloseObservers.empty()) {
        mObservedOperations = (IOInterposeObserver::Operation)
                         (mObservedOperations & ~IOInterposeObserver::OpClose);
      }
    }
    mObserverLists = newLists;
    mCurrentGeneration++;
  }
 
  void
  Update(PerThreadData &aPtd)
  {
    if (mCurrentGeneration == aPtd.GetCurrentGeneration()) {
      return;
    }
    // If the generation counts don't match then we need to update the current
    // thread's observer list with the new master list.
    AutoPRLock lock(mLock);
    aPtd.SetObserverLists(mCurrentGeneration, mObserverLists);
  }

  inline bool
  IsObservedOperation(IOInterposeObserver::Operation aOp)
  {
    // The quick reader may observe that no locks are being employed here,
    // hence the result of the operations is truly undefined. However, most
    // computers will usually return either true or false, which is good enough.
    // It is not a problem if we occasionally report more or less IO than is
    // actually occurring.
    return mIsEnabled && !!(mObservedOperations & aOp);
  }

private:
  RefPtr<ObserverLists>             mObserverLists;
  // Note, we cannot use mozilla::Mutex here as the ObserverLists may be leaked
  // (We want to monitor IO during shutdown). Furthermore, as we may have to
  // unregister observers during shutdown an OffTheBooksMutex is not an option
  // either, as its base calls into sDeadlockDetector which may be nullptr
  // during shutdown.
  PRLock*                           mLock;
  // Flags tracking which operations are being observed
  IOInterposeObserver::Operation    mObservedOperations;
  // Used for quickly disabling everything by IOInterposer::Disable()
  Atomic<bool>                      mIsEnabled;
  // Used to inform threads that the master observer list has changed
  Atomic<uint32_t>                  mCurrentGeneration;
};

// List of observers registered
static StaticAutoPtr<MasterList> sMasterList;
static ThreadLocal<PerThreadData*> sThreadLocalData;
} // anonymous namespace

IOInterposeObserver::Observation::Observation(Operation aOperation,
                                              const char* aReference,
                                              bool aShouldReport)
  : mOperation(aOperation)
  , mReference(aReference)
  , mShouldReport(IOInterposer::IsObservedOperation(aOperation) &&
                  aShouldReport)
{
  if (mShouldReport) {
    mStart = TimeStamp::Now();
  }
}

IOInterposeObserver::Observation::Observation(Operation aOperation,
                                              const TimeStamp& aStart,
                                              const TimeStamp& aEnd,
                                              const char* aReference)
  : mOperation(aOperation)
  , mStart(aStart)
  , mEnd(aEnd)
  , mReference(aReference)
  , mShouldReport(false)
{
}

void
IOInterposeObserver::Observation::Report()
{
  if (mShouldReport) {
    mEnd = TimeStamp::Now();
    IOInterposer::Report(*this);
  }
}

/* static */ bool
IOInterposer::Init()
{
  // Don't initialize twice...
  if (sMasterList) {
    return true;
  }
  if (!sThreadLocalData.init()) {
    return false;
  }
#if defined(XP_WIN)
  bool isMainThread = XRE_GetWindowsEnvironment() !=
                        WindowsEnvironmentType_Metro;
#else
  bool isMainThread = true;
#endif
  RegisterCurrentThread(isMainThread);
  sMasterList = new MasterList();

  // Now we initialize the various interposers depending on platform
  InitPoisonIOInterposer();
  // We don't hook NSPR on Windows because PoisonIOInterposer captures a
  // superset of the former's events.
#if !defined(XP_WIN)
  InitNSPRIOInterposing();
#endif
  return true;
}

/* static */ bool
IOInterposeObserver::IsMainThread()
{
  if (!sThreadLocalData.initialized()) {
    return false;
  }
  PerThreadData *ptd = sThreadLocalData.get();
  if (!ptd) {
    return false;
  }
  return ptd->IsMainThread();
}

/* static */ void
IOInterposer::Clear()
{
  sMasterList = nullptr;
}

/* static */ void
IOInterposer::Disable()
{
  if (!sMasterList) {
    return;
  }
  sMasterList->Disable();
}

/* static */ void
IOInterposer::Report(IOInterposeObserver::Observation& aObservation)
{
  MOZ_ASSERT(sMasterList);
  if (!sMasterList) {
    return;
  }

  PerThreadData* ptd = sThreadLocalData.get();
  if (!ptd) {
    // In this case the current thread is not registered with IOInterposer.
    // Alternatively we could take the slow path and just lock everything if
    // we're not registered. That could potentially perform poorly, though.
    return;
  }
  sMasterList->Update(*ptd);

  // Don't try to report if there's nobody listening.
  if (!IOInterposer::IsObservedOperation(aObservation.ObservedOperation())) {
    return;
  }

  ptd->CallObservers(aObservation);
}

/* static */ bool
IOInterposer::IsObservedOperation(IOInterposeObserver::Operation aOp)
{
  return sMasterList && sMasterList->IsObservedOperation(aOp);
}

/* static */ void
IOInterposer::Register(IOInterposeObserver::Operation aOp,
                       IOInterposeObserver* aObserver)
{
  MOZ_ASSERT(aObserver);
  if (!sMasterList || !aObserver) {
    return;
  }

  sMasterList->Register(aOp, aObserver);
}

/* static */ void
IOInterposer::Unregister(IOInterposeObserver::Operation aOp,
                         IOInterposeObserver* aObserver)
{
  if (!sMasterList) {
    return;
  }

  sMasterList->Unregister(aOp, aObserver);
}

/* static */ void
IOInterposer::RegisterCurrentThread(bool aIsMainThread)
{
  if (!sThreadLocalData.initialized()) {
    return;
  }
  MOZ_ASSERT(!sThreadLocalData.get());
  PerThreadData* curThreadData = new PerThreadData(aIsMainThread);
  sThreadLocalData.set(curThreadData);
}

/* static */ void
IOInterposer::UnregisterCurrentThread()
{
  if (!sThreadLocalData.initialized()) {
    return;
  }
  PerThreadData* curThreadData = sThreadLocalData.get();
  MOZ_ASSERT(curThreadData);
  sThreadLocalData.set(nullptr);
  delete curThreadData;
}

