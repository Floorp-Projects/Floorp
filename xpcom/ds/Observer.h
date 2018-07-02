/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Observer_h
#define mozilla_Observer_h

#include "nsTArray.h"

namespace mozilla {

/**
 * Observer<T> provides a way for a class to observe something.
 * When an event has to be broadcasted to all Observer<T>, Notify() method
 * is called.
 * T represents the type of the object passed in argument to Notify().
 *
 * @see ObserverList.
 */
template<class T>
class Observer
{
public:
  virtual ~Observer() {}
  virtual void Notify(const T& aParam) = 0;
};

/**
 * ObserverList<T> tracks Observer<T> and can notify them when Broadcast() is
 * called.
 * T represents the type of the object passed in argument to Broadcast() and
 * sent to Observer<T> objects through Notify().
 *
 * @see Observer.
 */
template<class T>
class ObserverList
{
public:
  /**
   * Note: When calling AddObserver, it's up to the caller to make sure the
   * object isn't going to be release as long as RemoveObserver hasn't been
   * called.
   *
   * @see RemoveObserver()
   */
  void AddObserver(Observer<T>* aObserver)
  {
    mObservers.AppendElement(aObserver);
  }

  /**
   * Remove the observer from the observer list.
   * @return Whether the observer has been found in the list.
   */
  bool RemoveObserver(Observer<T>* aObserver)
  {
    if (mObservers.RemoveElement(aObserver)) {
      if (!mBroadcastCopy.IsEmpty()) {
        // Annoyingly, someone could RemoveObserver() an item on the list
        // while we're in a Broadcast()'s Notify() call.
        auto i = mBroadcastCopy.IndexOf(aObserver);
        MOZ_ASSERT(i != mBroadcastCopy.NoIndex);
        mBroadcastCopy[i] = nullptr;
      }
      return true;
    }
    return false;
  }

  uint32_t Length()
  {
    return mObservers.Length();
  }

  /**
   * Call Notify() on each item in the list.
   * Handles the case of Notify() calling RemoveObserver()
   */
  void Broadcast(const T& aParam)
  {
    MOZ_ASSERT(mBroadcastCopy.IsEmpty());
    mBroadcastCopy = mObservers;
    uint32_t size = mBroadcastCopy.Length();
    for (uint32_t i = 0; i < size; ++i) {
      // nulled if Removed during Broadcast
      if (mBroadcastCopy[i]) {
        mBroadcastCopy[i]->Notify(aParam);
      }
    }
    mBroadcastCopy.Clear();
  }

protected:
  nsTArray<Observer<T>*> mObservers;
  nsTArray<Observer<T>*> mBroadcastCopy;
};

} // namespace mozilla

#endif // mozilla_Observer_h
