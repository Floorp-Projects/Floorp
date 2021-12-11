/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSEXPIRATIONTRACKER_H_
#define NSEXPIRATIONTRACKER_H_

#include <cstring>
#include "MainThreadUtils.h"
#include "nsAlgorithm.h"
#include "nsDebug.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsISupports.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nscore.h"
#include "mozilla/Assertions.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefCountType.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Services.h"

/**
 * Data used to track the expiration state of an object. We promise that this
 * is 32 bits so that objects that includes this as a field can pad and align
 * efficiently.
 */
struct nsExpirationState {
  enum {
    NOT_TRACKED = (1U << 4) - 1,
    MAX_INDEX_IN_GENERATION = (1U << 28) - 1
  };

  nsExpirationState() : mGeneration(NOT_TRACKED), mIndexInGeneration(0) {}
  bool IsTracked() { return mGeneration != NOT_TRACKED; }

  /**
   * The generation that this object belongs to, or NOT_TRACKED.
   */
  uint32_t mGeneration : 4;
  uint32_t mIndexInGeneration : 28;
};

/**
 * ExpirationTracker classes:
 * - ExpirationTrackerImpl (Thread-safe class)
 * - nsExpirationTracker (Main-thread only class)
 *
 * These classes can track the lifetimes and usage of a large number of
 * objects, and send a notification some window of time after a live object was
 * last used. This is very useful when you manage a large number of objects
 * and want to flush some after they haven't been used for a while.
 * nsExpirationTracker is designed to be very space and time efficient.
 *
 * The type parameter T is the object type that we will track pointers to. T
 * must include an accessible method GetExpirationState() that returns a
 * pointer to an nsExpirationState associated with the object (preferably,
 * stored in a field of the object).
 *
 * The parameter K is the number of generations that will be used. Increasing
 * the number of generations narrows the window within which we promise
 * to fire notifications, at a slight increase in space cost for the tracker.
 * We require 2 <= K <= nsExpirationState::NOT_TRACKED (currently 15).
 *
 * To use this class, you need to inherit from it and override the
 * NotifyExpired() method.
 *
 * The approach is to track objects in K generations. When an object is accessed
 * it moves from its current generation to the newest generation. Generations
 * are stored in a cyclic array; when a timer interrupt fires, we advance
 * the current generation pointer to effectively age all objects very
 * efficiently. By storing information in each object about its generation and
 * index within its generation array, we make removal of objects from a
 * generation very cheap.
 *
 * Future work:
 * -- Add a method to change the timer period?
 */

/**
 * Base class for ExiprationTracker implementations.
 *
 * nsExpirationTracker class below is a specialized class to be inherited by the
 * instances to be accessed only on main-thread.
 *
 * For creating a thread-safe tracker, you can define a subclass inheriting this
 * base class and specialize the Mutex and AutoLock to be used.
 *
 * For an example of using ExpirationTrackerImpl with a DataMutex
 * @see mozilla::gfx::GradientCache.
 *
 */
template <typename T, uint32_t K, typename Mutex, typename AutoLock>
class ExpirationTrackerImpl {
 public:
  /**
   * Initialize the tracker.
   * @param aTimerPeriod the timer period in milliseconds. The guarantees
   * provided by the tracker are defined in terms of this period. If the
   * period is zero, then we don't use a timer and rely on someone calling
   * AgeOneGenerationLocked explicitly.
   * @param aName the name of the subclass for telemetry.
   * @param aEventTarget the optional event target on main thread to label the
   * runnable of the asynchronous invocation to NotifyExpired().

   */
  ExpirationTrackerImpl(uint32_t aTimerPeriod, const char* aName,
                        nsIEventTarget* aEventTarget = nullptr)
      : mTimerPeriod(aTimerPeriod),
        mNewestGeneration(0),
        mInAgeOneGeneration(false),
        mName(aName),
        mEventTarget(aEventTarget) {
    static_assert(K >= 2 && K <= nsExpirationState::NOT_TRACKED,
                  "Unsupported number of generations (must be 2 <= K <= 15)");
    MOZ_ASSERT(NS_IsMainThread());
    if (mEventTarget) {
      bool current = false;
      // NOTE: The following check+crash could be condensed into a
      // MOZ_RELEASE_ASSERT, but that triggers a segfault during compilation in
      // clang 3.8. Once we don't have to care about clang 3.8 anymore, though,
      // we can convert to MOZ_RELEASE_ASSERT here.
      if (MOZ_UNLIKELY(NS_FAILED(mEventTarget->IsOnCurrentThread(&current)) ||
                       !current)) {
        MOZ_CRASH("Provided event target must be on the main thread");
      }
    }
    mObserver = new ExpirationTrackerObserver();
    mObserver->Init(this);
  }

  virtual ~ExpirationTrackerImpl() {
    MOZ_ASSERT(NS_IsMainThread());
    if (mTimer) {
      mTimer->Cancel();
    }
    mObserver->Destroy();
  }

  /**
   * Add an object to be tracked. It must not already be tracked. It will
   * be added to the newest generation, i.e., as if it was just used.
   * @return an error on out-of-memory
   */
  nsresult AddObjectLocked(T* aObj, const AutoLock& aAutoLock) {
    if (NS_WARN_IF(!aObj)) {
      MOZ_DIAGNOSTIC_ASSERT(false, "Invalid object to add");
      return NS_ERROR_UNEXPECTED;
    }
    nsExpirationState* state = aObj->GetExpirationState();
    if (NS_WARN_IF(state->IsTracked())) {
      MOZ_DIAGNOSTIC_ASSERT(false,
                            "Tried to add an object that's already tracked");
      return NS_ERROR_UNEXPECTED;
    }
    nsTArray<T*>& generation = mGenerations[mNewestGeneration];
    uint32_t index = generation.Length();
    if (index > nsExpirationState::MAX_INDEX_IN_GENERATION) {
      NS_WARNING("More than 256M elements tracked, this is probably a problem");
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (index == 0) {
      // We might need to start the timer
      nsresult rv = CheckStartTimerLocked(aAutoLock);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    generation.AppendElement(aObj);
    state->mGeneration = mNewestGeneration;
    state->mIndexInGeneration = index;
    return NS_OK;
  }

  /**
   * Remove an object from the tracker. It must currently be tracked.
   */
  void RemoveObjectLocked(T* aObj, const AutoLock& aAutoLock) {
    if (NS_WARN_IF(!aObj)) {
      MOZ_DIAGNOSTIC_ASSERT(false, "Invalid object to remove");
      return;
    }
    nsExpirationState* state = aObj->GetExpirationState();
    if (NS_WARN_IF(!state->IsTracked())) {
      MOZ_DIAGNOSTIC_ASSERT(false,
                            "Tried to remove an object that's not tracked");
      return;
    }
    nsTArray<T*>& generation = mGenerations[state->mGeneration];
    uint32_t index = state->mIndexInGeneration;
    MOZ_ASSERT(generation.Length() > index && generation[index] == aObj,
               "Object is lying about its index");
    // Move the last object to fill the hole created by removing aObj
    T* lastObj = generation.PopLastElement();
    // XXX It looks weird that index might point to the element that was just
    // removed. Is that really correct?
    if (index < generation.Length()) {
      generation[index] = lastObj;
    }
    lastObj->GetExpirationState()->mIndexInGeneration = index;
    state->mGeneration = nsExpirationState::NOT_TRACKED;
    // We do not check whether we need to stop the timer here. The timer
    // will check that itself next time it fires. Checking here would not
    // be efficient since we'd need to track all generations. Also we could
    // thrash by incessantly creating and destroying timers if someone
    // kept adding and removing an object from the tracker.
  }

  /**
   * Notify that an object has been used.
   * @return an error if we lost the object from the tracker...
   */
  nsresult MarkUsedLocked(T* aObj, const AutoLock& aAutoLock) {
    nsExpirationState* state = aObj->GetExpirationState();
    if (mNewestGeneration == state->mGeneration) {
      return NS_OK;
    }
    RemoveObjectLocked(aObj, aAutoLock);
    return AddObjectLocked(aObj, aAutoLock);
  }

  /**
   * The timer calls this, but it can also be manually called if you want
   * to age objects "artifically". This can result in calls to
   * NotifyExpiredLocked.
   */
  void AgeOneGenerationLocked(const AutoLock& aAutoLock) {
    if (mInAgeOneGeneration) {
      NS_WARNING("Can't reenter AgeOneGeneration from NotifyExpired");
      return;
    }

    mInAgeOneGeneration = true;
    uint32_t reapGeneration =
        mNewestGeneration > 0 ? mNewestGeneration - 1 : K - 1;
    nsTArray<T*>& generation = mGenerations[reapGeneration];
    // The following is rather tricky. We have to cope with objects being
    // removed from this generation either because of a call to RemoveObject
    // (or indirectly via MarkUsedLocked) inside NotifyExpiredLocked.
    // Fortunately no objects can be added to this generation because it's not
    // the newest generation. We depend on the fact that RemoveObject can only
    // cause the indexes of objects in this generation to *decrease*, not
    // increase. So if we start from the end and work our way backwards we are
    // guaranteed to see each object at least once.
    size_t index = generation.Length();
    for (;;) {
      // Objects could have been removed so index could be outside
      // the array
      index = XPCOM_MIN(index, generation.Length());
      if (index == 0) {
        break;
      }
      --index;
      NotifyExpiredLocked(generation[index], aAutoLock);
    }
    // Any leftover objects from reapGeneration just end up in the new
    // newest-generation. This is bad form, though, so warn if there are any.
    if (!generation.IsEmpty()) {
      NS_WARNING("Expired objects were not removed or marked used");
    }
    // Free excess memory used by the generation array, since we probably
    // just removed most or all of its elements.
    generation.Compact();
    mNewestGeneration = reapGeneration;
    mInAgeOneGeneration = false;
  }

  /**
   * This just calls AgeOneGenerationLocked K times. Under normal circumstances
   * this will result in all objects getting NotifyExpiredLocked called on them,
   * but if NotifyExpiredLocked itself marks some objects as used, then those
   * objects might not expire. This would be a good thing to call if we get into
   * a critically-low memory situation.
   */
  void AgeAllGenerationsLocked(const AutoLock& aAutoLock) {
    uint32_t i;
    for (i = 0; i < K; ++i) {
      AgeOneGenerationLocked(aAutoLock);
    }
  }

  class Iterator {
   private:
    ExpirationTrackerImpl<T, K, Mutex, AutoLock>* mTracker;
    uint32_t mGeneration;
    uint32_t mIndex;

   public:
    Iterator(ExpirationTrackerImpl<T, K, Mutex, AutoLock>* aTracker,
             AutoLock& aAutoLock)
        : mTracker(aTracker), mGeneration(0), mIndex(0) {}

    T* Next() {
      while (mGeneration < K) {
        nsTArray<T*>* generation = &mTracker->mGenerations[mGeneration];
        if (mIndex < generation->Length()) {
          ++mIndex;
          return (*generation)[mIndex - 1];
        }
        ++mGeneration;
        mIndex = 0;
      }
      return nullptr;
    }
  };

  friend class Iterator;

  bool IsEmptyLocked(const AutoLock& aAutoLock) const {
    for (uint32_t i = 0; i < K; ++i) {
      if (!mGenerations[i].IsEmpty()) {
        return false;
      }
    }
    return true;
  }

  size_t Length(const AutoLock& aAutoLock) const {
    size_t len = 0;
    for (uint32_t i = 0; i < K; ++i) {
      len += mGenerations[i].Length();
    }
    return len;
  }

  // @return The amount of memory used by this ExpirationTrackerImpl, excluding
  // sizeof(*this). If you want to measure anything hanging off the mGenerations
  // array, you must iterate over the elements and measure them individually;
  // hence the "Shallow" prefix.
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t bytes = 0;
    for (uint32_t i = 0; i < K; ++i) {
      bytes += mGenerations[i].ShallowSizeOfExcludingThis(aMallocSizeOf);
    }
    return bytes;
  }

 protected:
  /**
   * This must be overridden to catch notifications. It is called whenever
   * we detect that an object has not been used for at least (K-1)*mTimerPeriod
   * milliseconds. If timer events are not delayed, it will be called within
   * roughly K*mTimerPeriod milliseconds after the last use.
   * (Unless AgeOneGenerationLocked or AgeAllGenerationsLocked have been called
   * to accelerate the aging process.)
   *
   * NOTE: These bounds ignore delays in timer firings due to actual work being
   * performed by the browser. We use a slack timer so there is always at least
   * mTimerPeriod milliseconds between firings, which gives us
   * (K-1)*mTimerPeriod as a pretty solid lower bound. The upper bound is rather
   * loose, however. If the maximum amount by which any given timer firing is
   * delayed is D, then the upper bound before NotifyExpiredLocked is called is
   * K*(mTimerPeriod + D).
   *
   * The NotifyExpiredLocked call is expected to remove the object from the
   * tracker, but it need not. The object (or other objects) could be
   * "resurrected" by calling MarkUsedLocked() on them, or they might just not
   * be removed. Any objects left over that have not been resurrected or removed
   * are placed in the new newest-generation, but this is considered "bad form"
   * and should be avoided (we'll issue a warning). (This recycling counts
   * as "a use" for the purposes of the expiry guarantee above...)
   *
   * For robustness and simplicity, we allow objects to be notified more than
   * once here in the same timer tick.
   */
  virtual void NotifyExpiredLocked(T*, const AutoLock&) = 0;

  /**
   * This may be overridden to perform any post-aging work that needs to be
   * done while still holding the lock. It will be called once after each timer
   * event, and each low memory event has been handled.
   */
  virtual void NotifyHandlerEndLocked(const AutoLock&){};

  /**
   * This may be overridden to perform any post-aging work that needs to be
   * done outside the lock. It will be called once after each
   * NotifyEndTransactionLocked call.
   */
  virtual void NotifyHandlerEnd(){};

  virtual Mutex& GetMutex() = 0;

 private:
  class ExpirationTrackerObserver;
  RefPtr<ExpirationTrackerObserver> mObserver;
  nsTArray<T*> mGenerations[K];
  nsCOMPtr<nsITimer> mTimer;
  uint32_t mTimerPeriod;
  uint32_t mNewestGeneration;
  bool mInAgeOneGeneration;
  const char* const mName;  // Used for timer firing profiling.
  const nsCOMPtr<nsIEventTarget> mEventTarget;

  /**
   * Whenever "memory-pressure" is observed, it calls AgeAllGenerationsLocked()
   * to minimize memory usage.
   */
  class ExpirationTrackerObserver final : public nsIObserver {
   public:
    void Init(ExpirationTrackerImpl<T, K, Mutex, AutoLock>* aObj) {
      mOwner = aObj;
      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      if (obs) {
        obs->AddObserver(this, "memory-pressure", false);
      }
    }
    void Destroy() {
      mOwner = nullptr;
      nsCOMPtr<nsIObserverService> obs =
          mozilla::services::GetObserverService();
      if (obs) {
        obs->RemoveObserver(this, "memory-pressure");
      }
    }
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
   private:
    ExpirationTrackerImpl<T, K, Mutex, AutoLock>* mOwner;
  };

  void HandleLowMemory() {
    {
      AutoLock lock(GetMutex());
      AgeAllGenerationsLocked(lock);
      NotifyHandlerEndLocked(lock);
    }
    NotifyHandlerEnd();
  }

  void HandleTimeout() {
    {
      AutoLock lock(GetMutex());
      AgeOneGenerationLocked(lock);
      // Cancel the timer if we have no objects to track
      if (IsEmptyLocked(lock)) {
        mTimer->Cancel();
        mTimer = nullptr;
      }
      NotifyHandlerEndLocked(lock);
    }
    NotifyHandlerEnd();
  }

  static void TimerCallback(nsITimer* aTimer, void* aThis) {
    ExpirationTrackerImpl* tracker = static_cast<ExpirationTrackerImpl*>(aThis);
    tracker->HandleTimeout();
  }

  nsresult CheckStartTimerLocked(const AutoLock& aAutoLock) {
    if (mTimer || !mTimerPeriod) {
      return NS_OK;
    }
    nsCOMPtr<nsIEventTarget> target = mEventTarget;
    if (!target && !NS_IsMainThread()) {
      // TimerCallback should always be run on the main thread to prevent races
      // to the destruction of the tracker.
      target = do_GetMainThread();
      NS_ENSURE_STATE(target);
    }

    return NS_NewTimerWithFuncCallback(
        getter_AddRefs(mTimer), TimerCallback, this, mTimerPeriod,
        nsITimer::TYPE_REPEATING_SLACK_LOW_PRIORITY, mName, target);
  }
};

namespace detail {

class PlaceholderLock {
 public:
  void Lock() {}
  void Unlock() {}
};

class PlaceholderAutoLock {
 public:
  explicit PlaceholderAutoLock(PlaceholderLock&) {}
  ~PlaceholderAutoLock() = default;
};

template <typename T, uint32_t K>
using SingleThreadedExpirationTracker =
    ExpirationTrackerImpl<T, K, PlaceholderLock, PlaceholderAutoLock>;

}  // namespace detail

template <typename T, uint32_t K>
class nsExpirationTracker
    : protected ::detail::SingleThreadedExpirationTracker<T, K> {
  typedef ::detail::PlaceholderLock Lock;
  typedef ::detail::PlaceholderAutoLock AutoLock;

  Lock mLock;

  AutoLock FakeLock() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    return AutoLock(mLock);
  }

  Lock& GetMutex() override {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());
    return mLock;
  }

  void NotifyExpiredLocked(T* aObject, const AutoLock&) override {
    NotifyExpired(aObject);
  }

  /**
   * Since there are no users of these callbacks in the single threaded case,
   * we mark them as final with the hope that the compiler can optimize the
   * method calls out entirely.
   */
  void NotifyHandlerEndLocked(const AutoLock&) final {}
  void NotifyHandlerEnd() final {}

 protected:
  virtual void NotifyExpired(T* aObj) = 0;

 public:
  nsExpirationTracker(uint32_t aTimerPeriod, const char* aName,
                      nsIEventTarget* aEventTarget = nullptr)
      : ::detail::SingleThreadedExpirationTracker<T, K>(aTimerPeriod, aName,
                                                        aEventTarget) {}

  virtual ~nsExpirationTracker() = default;

  nsresult AddObject(T* aObj) {
    return this->AddObjectLocked(aObj, FakeLock());
  }

  void RemoveObject(T* aObj) { this->RemoveObjectLocked(aObj, FakeLock()); }

  nsresult MarkUsed(T* aObj) { return this->MarkUsedLocked(aObj, FakeLock()); }

  void AgeOneGeneration() { this->AgeOneGenerationLocked(FakeLock()); }

  void AgeAllGenerations() { this->AgeAllGenerationsLocked(FakeLock()); }

  class Iterator {
   private:
    AutoLock mAutoLock;
    typename ExpirationTrackerImpl<T, K, Lock, AutoLock>::Iterator mIterator;

   public:
    explicit Iterator(nsExpirationTracker<T, K>* aTracker)
        : mAutoLock(aTracker->GetMutex()), mIterator(aTracker, mAutoLock) {}

    T* Next() { return mIterator.Next(); }
  };

  friend class Iterator;

  bool IsEmpty() { return this->IsEmptyLocked(FakeLock()); }
};

template <typename T, uint32_t K, typename Mutex, typename AutoLock>
NS_IMETHODIMP ExpirationTrackerImpl<T, K, Mutex, AutoLock>::
    ExpirationTrackerObserver::Observe(nsISupports* aSubject,
                                       const char* aTopic,
                                       const char16_t* aData) {
  if (!strcmp(aTopic, "memory-pressure") && mOwner) {
    mOwner->HandleLowMemory();
  }
  return NS_OK;
}

template <class T, uint32_t K, typename Mutex, typename AutoLock>
NS_IMETHODIMP_(MozExternalRefCountType)
ExpirationTrackerImpl<T, K, Mutex, AutoLock>::ExpirationTrackerObserver::AddRef(
    void) {
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  NS_ASSERT_OWNINGTHREAD(ExpirationTrackerObserver);
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "ExpirationTrackerObserver", sizeof(*this));
  return mRefCnt;
}

template <class T, uint32_t K, typename Mutex, typename AutoLock>
NS_IMETHODIMP_(MozExternalRefCountType)
ExpirationTrackerImpl<T, K, Mutex,
                      AutoLock>::ExpirationTrackerObserver::Release(void) {
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  NS_ASSERT_OWNINGTHREAD(ExpirationTrackerObserver);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "ExpirationTrackerObserver");
  if (mRefCnt == 0) {
    NS_ASSERT_OWNINGTHREAD(ExpirationTrackerObserver);
    mRefCnt = 1; /* stabilize */
    delete (this);
    return 0;
  }
  return mRefCnt;
}

template <class T, uint32_t K, typename Mutex, typename AutoLock>
NS_IMETHODIMP ExpirationTrackerImpl<T, K, Mutex, AutoLock>::
    ExpirationTrackerObserver::QueryInterface(REFNSIID aIID,
                                              void** aInstancePtr) {
  NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
  nsresult rv = NS_ERROR_FAILURE;
  NS_INTERFACE_TABLE(ExpirationTrackerObserver, nsIObserver)
  return rv;
}

#endif /*NSEXPIRATIONTRACKER_H_*/
