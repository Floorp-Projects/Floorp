/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_ClearOnShutdown_h
#define mozilla_ClearOnShutdown_h

#include "mozilla/LinkedList.h"
#include "nsThreadUtils.h"

/*
 * This header exports one public method in the mozilla namespace:
 *
 *   template<class SmartPtr>
 *   void ClearOnShutdown(SmartPtr *aPtr)
 *
 * This function takes a pointer to a smart pointer (i.e., nsCOMPtr<T>*,
 * nsRefPtr<T>*, or nsAutoPtr<T>*) and nulls the smart pointer on shutdown.
 *
 * This is useful if you have a global smart pointer object which you don't
 * want to "leak" on shutdown.
 *
 * There is no way to undo a call to ClearOnShutdown, so you can call it only
 * on smart pointers which you know will live until the program shuts down.
 *
 * ClearOnShutdown is currently main-thread only because we don't want to
 * accidentally free an object from a different thread than the one it was
 * created on.
 */

namespace mozilla {
namespace ClearOnShutdown_Internal {

class ShutdownObserver : public LinkedListElement<ShutdownObserver>
{
public:
  virtual void Shutdown() = 0;
};

template<class SmartPtr>
class PointerClearer : public ShutdownObserver
{
public:
  PointerClearer(SmartPtr *aPtr)
    : mPtr(aPtr)
  {}

  virtual void Shutdown()
  {
    if (mPtr) {
      *mPtr = NULL;
    }
  }

private:
  SmartPtr *mPtr;
};

extern bool sHasShutDown;
extern LinkedList<ShutdownObserver> sShutdownObservers;

} // namespace ClearOnShutdown_Internal

template<class SmartPtr>
inline void ClearOnShutdown(SmartPtr *aPtr)
{
  using namespace ClearOnShutdown_Internal;

  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!sHasShutDown);
  ShutdownObserver *observer = new PointerClearer<SmartPtr>(aPtr);
  sShutdownObservers.insertBack(observer);
}

// Called when XPCOM is shutting down, after all shutdown notifications have
// been sent and after all threads' event loops have been purged.
inline void KillClearOnShutdown()
{
  using namespace ClearOnShutdown_Internal;

  MOZ_ASSERT(NS_IsMainThread());

  ShutdownObserver *observer;
  while ((observer = sShutdownObservers.popFirst())) {
    observer->Shutdown();
    delete observer;
  }

  sHasShutDown = true;
}

} // namespace mozilla

#endif
