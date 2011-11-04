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
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
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

  void RemoveObserver(Observer<T>* aObserver) {
    mObservers.RemoveElement(aObserver);
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
