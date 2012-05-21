/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
template <class T>
class Observer
{
public:
  virtual ~Observer() { }
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
template <class T>
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
  void AddObserver(Observer<T>* aObserver) {
    mObservers.AppendElement(aObserver);
  }

  /**
   * Remove the observer from the observer list.
   * @return Whether the observer has been found in the list.
   */
  bool RemoveObserver(Observer<T>* aObserver) {
    return mObservers.RemoveElement(aObserver);
  }

  PRUint32 Length() {
    return mObservers.Length();
  }

  void Broadcast(const T& aParam) {
    PRUint32 size = mObservers.Length();
    for (PRUint32 i=0; i<size; ++i) {
      mObservers[i]->Notify(aParam);
    }
  }

protected:
  nsTArray<Observer<T>*> mObservers;
};

} // namespace mozilla

#endif // mozilla_Observer_h
