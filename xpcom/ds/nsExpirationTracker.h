/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   robert@ocallahan.org
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSEXPIRATIONTRACKER_H_
#define NSEXPIRATIONTRACKER_H_

#include "prlog.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"

/**
 * Data used to track the expiration state of an object. We promise that this
 * is 32 bits so that objects that includes this as a field can pad and align
 * efficiently.
 */
struct nsExpirationState {
  enum { NOT_TRACKED = (1U << 4) - 1,
         MAX_INDEX_IN_GENERATION = (1U << 28) - 1 };

  nsExpirationState() : mGeneration(NOT_TRACKED) {}
  PRBool IsTracked() { return mGeneration != NOT_TRACKED; }

  /**
   * The generation that this object belongs to, or NOT_TRACKED.
   */
  PRUint32 mGeneration:4;
  PRUint32 mIndexInGeneration:28;
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
template <class T, PRUint32 K> class nsExpirationTracker {
  public:
    /**
     * Initialize the tracker.
     * @param aTimerPeriod the timer period in milliseconds. The guarantees
     * provided by the tracker are defined in terms of this period. If the
     * period is zero, then we don't use a timer and rely on someone calling
     * AgeOneGeneration explicitly.
     */
    nsExpirationTracker(PRUint32 aTimerPeriod)
      : mTimerPeriod(aTimerPeriod), mNewestGeneration(0),
        mInAgeOneGeneration(PR_FALSE) {
      PR_STATIC_ASSERT(K >= 2 && K <= nsExpirationState::NOT_TRACKED);
    }
    ~nsExpirationTracker() {
      if (mTimer) {
        mTimer->Cancel();
      }
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
      PRUint32 index = generation.Length();
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
      PRUint32 index = state->mIndexInGeneration;
      NS_ASSERTION(generation.Length() > index &&
                   generation[index] == aObj, "Object is lying about its index");
      // Move the last object to fill the hole created by removing aObj
      PRUint32 last = generation.Length() - 1;
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
      
      mInAgeOneGeneration = PR_TRUE;
      PRUint32 reapGeneration = 
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
      PRUint32 index = generation.Length();
      for (;;) {
        // Objects could have been removed so index could be outside
        // the array
        index = PR_MIN(index, generation.Length());
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
      mInAgeOneGeneration = PR_FALSE;
    }

    /**
     * This just calls AgeOneGeneration K times. Under normal circumstances this
     * will result in all objects getting NotifyExpired called on them, but
     * if NotifyExpired itself marks some objects as used, then those objects
     * might not expire. This would be a good thing to call if we get into
     * a critically-low memory situation.
     */
    void AgeAllGenerations() {
      PRUint32 i;
      for (i = 0; i < K; ++i) {
        AgeOneGeneration();
      }
    }
    
    class Iterator {
    private:
      nsExpirationTracker<T,K>* mTracker;
      PRUint32                  mGeneration;
      PRUint32                  mIndex;
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
        return nsnull;
      }
    };
    
    friend class Iterator;

  protected:
    /**
     * This must be overridden to catch notifications. It is called whenever
     * we detect that an object has not been used for at least (K-1)*mTimerPeriod
     * seconds. If timer events are not delayed, it will be called within
     * roughly K*mTimerPeriod seconds after the last use. (Unless AgeOneGeneration
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
    nsTArray<T*>       mGenerations[K];
    nsCOMPtr<nsITimer> mTimer;
    PRUint32           mTimerPeriod;
    PRUint32           mNewestGeneration;
    PRPackedBool       mInAgeOneGeneration;

    static void TimerCallback(nsITimer* aTimer, void* aThis) {
      nsExpirationTracker* tracker = static_cast<nsExpirationTracker*>(aThis);
      tracker->AgeOneGeneration();
      // Cancel the timer if we have no objects to track
      PRUint32 i;
      for (i = 0; i < K; ++i) {
        if (!tracker->mGenerations[i].IsEmpty())
          return;
      }
      tracker->mTimer->Cancel();
      tracker->mTimer = nsnull;
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

#endif /*NSEXPIRATIONTRACKER_H_*/
