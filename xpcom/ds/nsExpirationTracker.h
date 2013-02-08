/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSEXPIRATIONTRACKER_H_
#define NSEXPIRATIONTRACKER_H_

#include "mozilla/Attributes.h"

#include "prlog.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/Attributes.h"

/**
 * Data used to track the expiration state of an object. We promise that this
 * is 32 bits so that objects that includes this as a field can pad and align
 * efficiently.
 */
struct nsExpirationState {
  enum { NOT_TRACKED = (1U << 4) - 1,
         MAX_INDEX_IN_GENERATION = (1U << 28) - 1 };

  nsExpirationState() : mGeneration(NOT_TRACKED) {}
  bool IsTracked() { return mGeneration != NOT_TRACKED; }

  /**
   * The generation that this object belongs to, or NOT_TRACKED.
   */
  uint32_t mGeneration:4;
  uint32_t mIndexInGeneration:28;
};

/**
 * nsExpirationTracker can track the lifetimes and usage of a large number of
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
 * the current generation pointer to effectively age all objects very efficiently.
 * By storing information in each object about its generation and index within its
 * generation array, we make removal of objects from a generation very cheap.
 * 
 * Future work:
 * -- Add a method to change the timer period?
 */
template <class T, uint32_t K> class nsExpirationTracker {
  public:
    /**
     * Initialize the tracker.
     * @param aTimerPeriod the timer period in milliseconds. The guarantees
     * provided by the tracker are defined in terms of this period. If the
     * period is zero, then we don't use a timer and rely on someone calling
     * AgeOneGeneration explicitly.
     */
    nsExpirationTracker(uint32_t aTimerPeriod)
      : mTimerPeriod(aTimerPeriod), mNewestGeneration(0),
        mInAgeOneGeneration(false) {
      MOZ_STATIC_ASSERT(K >= 2 && K <= nsExpirationState::NOT_TRACKED,
                        "Unsupported number of generations (must be 2 <= K <= 15)");
      mObserver = new ExpirationTrackerObserver();
      mObserver->Init(this);
    }
    ~nsExpirationTracker() {
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
    nsresult AddObject(T* aObj) {
      nsExpirationState* state = aObj->GetExpirationState();
      NS_ASSERTION(!state->IsTracked(), "Tried to add an object that's already tracked");
      nsTArray<T*>& generation = mGenerations[mNewestGeneration];
      uint32_t index = generation.Length();
      if (index > nsExpirationState::MAX_INDEX_IN_GENERATION) {
        NS_WARNING("More than 256M elements tracked, this is probably a problem");
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (index == 0) {
        // We might need to start the timer
        nsresult rv = CheckStartTimer();
        if (NS_FAILED(rv))
          return rv;
      }
      if (!generation.AppendElement(aObj))
        return NS_ERROR_OUT_OF_MEMORY;
      state->mGeneration = mNewestGeneration;
      state->mIndexInGeneration = index;
      return NS_OK;
    }

    /**
     * Remove an object from the tracker. It must currently be tracked.
     */
    void RemoveObject(T* aObj) {
      nsExpirationState* state = aObj->GetExpirationState();
      NS_ASSERTION(state->IsTracked(), "Tried to remove an object that's not tracked");
      nsTArray<T*>& generation = mGenerations[state->mGeneration];
      uint32_t index = state->mIndexInGeneration;
      NS_ASSERTION(generation.Length() > index &&
                   generation[index] == aObj, "Object is lying about its index");
      // Move the last object to fill the hole created by removing aObj
      uint32_t last = generation.Length() - 1;
      T* lastObj = generation[last];
      generation[index] = lastObj;
      lastObj->GetExpirationState()->mIndexInGeneration = index;
      generation.RemoveElementAt(last);
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
    nsresult MarkUsed(T* aObj) {
      nsExpirationState* state = aObj->GetExpirationState();
      if (mNewestGeneration == state->mGeneration)
        return NS_OK;
      RemoveObject(aObj);
      return AddObject(aObj);
    }

    /**
     * The timer calls this, but it can also be manually called if you want
     * to age objects "artifically". This can result in calls to NotifyExpired.
     */
    void AgeOneGeneration() {
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
      // (or indirectly via MarkUsed) inside NotifyExpired. Fortunately no
      // objects can be added to this generation because it's not the newest
      // generation. We depend on the fact that RemoveObject can only cause
      // the indexes of objects in this generation to *decrease*, not increase.
      // So if we start from the end and work our way backwards we are guaranteed
      // to see each object at least once.
      uint32_t index = generation.Length();
      for (;;) {
        // Objects could have been removed so index could be outside
        // the array
        index = XPCOM_MIN(index, generation.Length());
        if (index == 0)
          break;
        --index;
        NotifyExpired(generation[index]);
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
     * This just calls AgeOneGeneration K times. Under normal circumstances this
     * will result in all objects getting NotifyExpired called on them, but
     * if NotifyExpired itself marks some objects as used, then those objects
     * might not expire. This would be a good thing to call if we get into
     * a critically-low memory situation.
     */
    void AgeAllGenerations() {
      uint32_t i;
      for (i = 0; i < K; ++i) {
        AgeOneGeneration();
      }
    }
    
    class Iterator {
    private:
      nsExpirationTracker<T,K>* mTracker;
      uint32_t                  mGeneration;
      uint32_t                  mIndex;
    public:
      Iterator(nsExpirationTracker<T,K>* aTracker)
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

    bool IsEmpty() {
      for (uint32_t i = 0; i < K; ++i) {
        if (!mGenerations[i].IsEmpty())
          return false;
      }
      return true;
    }

  protected:
    /**
     * This must be overridden to catch notifications. It is called whenever
     * we detect that an object has not been used for at least (K-1)*mTimerPeriod
     * milliseconds. If timer events are not delayed, it will be called within
     * roughly K*mTimerPeriod milliseconds after the last use. (Unless AgeOneGeneration
     * or AgeAllGenerations have been called to accelerate the aging process.)
     * 
     * NOTE: These bounds ignore delays in timer firings due to actual work being
     * performed by the browser. We use a slack timer so there is always at least
     * mTimerPeriod milliseconds between firings, which gives us (K-1)*mTimerPeriod
     * as a pretty solid lower bound. The upper bound is rather loose, however.
     * If the maximum amount by which any given timer firing is delayed is D, then
     * the upper bound before NotifyExpired is called is K*(mTimerPeriod + D).
     * 
     * The NotifyExpired call is expected to remove the object from the tracker,
     * but it need not. The object (or other objects) could be "resurrected"
     * by calling MarkUsed() on them, or they might just not be removed.
     * Any objects left over that have not been resurrected or removed
     * are placed in the new newest-generation, but this is considered "bad form"
     * and should be avoided (we'll issue a warning). (This recycling counts
     * as "a use" for the purposes of the expiry guarantee above...)
     * 
     * For robustness and simplicity, we allow objects to be notified more than
     * once here in the same timer tick.
     */
    virtual void NotifyExpired(T* aObj) = 0;

  private:
    class ExpirationTrackerObserver;
    nsRefPtr<ExpirationTrackerObserver> mObserver;
    nsTArray<T*>       mGenerations[K];
    nsCOMPtr<nsITimer> mTimer;
    uint32_t           mTimerPeriod;
    uint32_t           mNewestGeneration;
    bool               mInAgeOneGeneration;

    /**
     * Whenever "memory-pressure" is observed, it calls AgeAllGenerations()
     * to minimize memory usage.
     */
    class ExpirationTrackerObserver MOZ_FINAL : public nsIObserver {
    public:
      void Init(nsExpirationTracker<T,K> *obj) {
        mOwner = obj;
        nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
        if (obs) {
          obs->AddObserver(this, "memory-pressure", false);
        }
      }
      void Destroy() {
        nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
        if (obs)
          obs->RemoveObserver(this, "memory-pressure");
      }
      NS_DECL_ISUPPORTS
      NS_DECL_NSIOBSERVER
    private:
      nsExpirationTracker<T,K> *mOwner;
    };
  
    static void TimerCallback(nsITimer* aTimer, void* aThis) {
      nsExpirationTracker* tracker = static_cast<nsExpirationTracker*>(aThis);
      tracker->AgeOneGeneration();
      // Cancel the timer if we have no objects to track
      if (tracker->IsEmpty()) {
        tracker->mTimer->Cancel();
        tracker->mTimer = nullptr;
      }
    }

    nsresult CheckStartTimer() {
      if (mTimer || !mTimerPeriod)
        return NS_OK;
      mTimer = do_CreateInstance("@mozilla.org/timer;1");
      if (!mTimer)
        return NS_ERROR_OUT_OF_MEMORY;
      mTimer->InitWithFuncCallback(TimerCallback, this, mTimerPeriod,
                                   nsITimer::TYPE_REPEATING_SLACK);
      return NS_OK;
    }
};

template<class T, uint32_t K>
NS_IMETHODIMP
nsExpirationTracker<T, K>::ExpirationTrackerObserver::Observe(nsISupports     *aSubject,
                                                              const char      *aTopic,
                                                              const PRUnichar *aData)
{
  if (!strcmp(aTopic, "memory-pressure"))
    mOwner->AgeAllGenerations();
  return NS_OK;
}

template <class T, uint32_t K>
NS_IMETHODIMP_(nsrefcnt)
nsExpirationTracker<T,K>::ExpirationTrackerObserver::AddRef(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  NS_ASSERT_OWNINGTHREAD_AND_NOT_CCTHREAD(ExpirationTrackerObserver);
  ++mRefCnt;
  NS_LOG_ADDREF(this, mRefCnt, "ExpirationTrackerObserver", sizeof(*this));
  return mRefCnt;
}

template <class T, uint32_t K>
NS_IMETHODIMP_(nsrefcnt)
nsExpirationTracker<T,K>::ExpirationTrackerObserver::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  NS_ASSERT_OWNINGTHREAD_AND_NOT_CCTHREAD(ExpirationTrackerObserver);
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

template <class T, uint32_t K>
NS_IMETHODIMP
nsExpirationTracker<T,K>::ExpirationTrackerObserver::QueryInterface(REFNSIID aIID, 
                                                                    void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr,
               "QueryInterface requires a non-NULL destination!");            
  nsresult rv = NS_ERROR_FAILURE;
  NS_INTERFACE_TABLE1(ExpirationTrackerObserver, nsIObserver)
  return rv;
}

#endif /*NSEXPIRATIONTRACKER_H_*/
