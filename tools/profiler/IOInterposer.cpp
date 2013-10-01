/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <vector>

#include "IOInterposer.h"

#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"

using namespace mozilla;

namespace {

/** Lists of Observers */
struct ObserverLists {
  ObserverLists()
  {
    mObserverListsLock = PR_NewLock();
    // We don't do MOZ_COUNT_CTOR(ObserverLists) as we will need to leak the
    // IO interposer when doing late-write checks, which uses IO interposing
    // to check for writes while static destructors are invoked.
  }

  // mObserverListsLock guards access to lists of observers
  // Note, we can use mozilla::Mutex here as the ObserverLists may be leaked, as
  // we want to monitor IO during shutdown. Furthermore, as we may have to
  // unregister observers during shutdown an OffTheBooksMutex is not an option
  // either, as it base calls into sDeadlockDetector which may be NULL during
  // shutdown.
  PRLock* mObserverListsLock;

  ~ObserverLists()
  {
    PR_DestroyLock(mObserverListsLock);
    mObserverListsLock = nullptr;
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

// List of observers registered
static StaticAutoPtr<ObserverLists> sObserverLists;

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

} // anonymous namespace

// Flags tracking which operations are being observed
IOInterposeObserver::Operation IOInterposer::sObservedOperations =
                                                  IOInterposeObserver::OpNone;

/* static */ void IOInterposer::Init()
{
  // Don't initialize twice...
  if (sObserverLists) {
    return;
  }
  sObserverLists = new ObserverLists();
  sObservedOperations = IOInterposeObserver::OpNone;
}

/* static */ void IOInterposer::Clear()
{
  // Clear() shouldn't be called if Init() wasn't called,
  MOZ_ASSERT(sObserverLists);
  if (sObserverLists) {
    // We require everybody unregister before clearing. If somebody didn't then
    // this is probably a case where one consumer clears the IO interposer and
    // another consumer still wants events.
    MOZ_ASSERT(sObserverLists->mReadObservers.empty());
    MOZ_ASSERT(sObserverLists->mWriteObservers.empty());
    MOZ_ASSERT(sObserverLists->mFSyncObservers.empty());

    sObserverLists = nullptr;
    sObservedOperations = IOInterposeObserver::OpNone;
  }
}

/* static */ void IOInterposer::Report(
  IOInterposeObserver::Observation& aObservation)
{
  // IOInterposer::Init most be called before this method
  MOZ_ASSERT(sObserverLists);
  if (!sObserverLists) {
    return;
  }

  //TODO: We only need read access here, so we should investigate the
  //      performance overhead involved in using some kind of shared lock.
  //      Work towards this end is tracked in bug #913653
  AutoPRLock listLock(sObserverLists->mObserverListsLock);

  // Don't try to report if there's nobody listening
  if (!IOInterposer::IsObservedOperation(aObservation.ObservedOperation())) {
    return;
  }

  // Decide which list of observers to inform
  std::vector<IOInterposeObserver*>* observers = nullptr;
  switch (aObservation.ObservedOperation()) {
    case IOInterposeObserver::OpCreateOrOpen:
      {
        observers = &sObserverLists->mCreateObservers;
      }
      break;
    case IOInterposeObserver::OpRead:
      {
        observers = &sObserverLists->mReadObservers;
      }
      break;
    case IOInterposeObserver::OpWrite:
      {
        observers = &sObserverLists->mWriteObservers;
      }
      break;
    case IOInterposeObserver::OpFSync:
      {
        observers = &sObserverLists->mFSyncObservers;
      }
      break;
    case IOInterposeObserver::OpStat:
      {
        observers = &sObserverLists->mStatObservers;
      }
      break;
    case IOInterposeObserver::OpClose:
      {
        observers = &sObserverLists->mCloseObservers;
      }
      break;
    default:
      {
        // Invalid IO operation, see documentation comment for Report()
        MOZ_ASSERT(false);
        // Just ignore is in non-debug builds.
        return;
      }
  }
  MOZ_ASSERT(observers);

  // Inform observers
  uint32_t nObservers = observers->size();
  for (uint32_t i = 0; i < nObservers; ++i) {
    (*observers)[i]->Observe(aObservation);
  }
}

/* static */ void IOInterposer::Register(IOInterposeObserver::Operation aOp,
                                         IOInterposeObserver* aObserver)
{
  // IOInterposer::Init most be called before this method
  MOZ_ASSERT(sObserverLists);
  // We should never register NULL as observer
  MOZ_ASSERT(aObserver);
  if (!sObserverLists || !aObserver) {
    return;
  }

  AutoPRLock listLock(sObserverLists->mObserverListsLock);

  // You can register to observe multiple types of observations
  // but you'll never be registered twice for the same observations.
  if (aOp & IOInterposeObserver::OpCreateOrOpen &&
      !VectorContains(sObserverLists->mCreateObservers, aObserver)) {
    sObserverLists->mCreateObservers.push_back(aObserver);
  }
  if (aOp & IOInterposeObserver::OpRead &&
      !VectorContains(sObserverLists->mReadObservers, aObserver)) {
    sObserverLists->mReadObservers.push_back(aObserver);
  }
  if (aOp & IOInterposeObserver::OpWrite &&
      !VectorContains(sObserverLists->mWriteObservers, aObserver)) {
    sObserverLists->mWriteObservers.push_back(aObserver);
  }
  if (aOp & IOInterposeObserver::OpFSync &&
      !VectorContains(sObserverLists->mFSyncObservers, aObserver)) {
    sObserverLists->mFSyncObservers.push_back(aObserver);
  }
  if (aOp & IOInterposeObserver::OpStat &&
      !VectorContains(sObserverLists->mStatObservers, aObserver)) {
    sObserverLists->mStatObservers.push_back(aObserver);
  }
  if (aOp & IOInterposeObserver::OpClose &&
      !VectorContains(sObserverLists->mCloseObservers, aObserver)) {
    sObserverLists->mCloseObservers.push_back(aObserver);
  }

  // Update field of observed operation with the operations that the new
  // observer is observing.
  sObservedOperations = (IOInterposeObserver::Operation)
                        (sObservedOperations | aOp);
}

/* static */ void IOInterposer::Unregister(IOInterposeObserver::Operation aOp,
                                           IOInterposeObserver* aObserver)
{
  // IOInterposer::Init most be called before this method.
  MOZ_ASSERT(sObserverLists);
  if (!sObserverLists) {
    return;
  }

  AutoPRLock listLock(sObserverLists->mObserverListsLock);

  if (aOp & IOInterposeObserver::OpRead) {
    VectorRemove(sObserverLists->mReadObservers, aObserver);
    if (sObserverLists->mReadObservers.empty()) {
      sObservedOperations = (IOInterposeObserver::Operation)
                       (sObservedOperations & ~IOInterposeObserver::OpRead);
    }
  }
  if (aOp & IOInterposeObserver::OpWrite) {
    VectorRemove(sObserverLists->mWriteObservers, aObserver);
    if (sObserverLists->mWriteObservers.empty()) {
      sObservedOperations = (IOInterposeObserver::Operation)
                       (sObservedOperations & ~IOInterposeObserver::OpWrite);
    }
  }
  if (aOp & IOInterposeObserver::OpFSync) {
    VectorRemove(sObserverLists->mFSyncObservers, aObserver);
    if (sObserverLists->mFSyncObservers.empty()) {
      sObservedOperations = (IOInterposeObserver::Operation)
                       (sObservedOperations & ~IOInterposeObserver::OpFSync);
    }
  }
}
