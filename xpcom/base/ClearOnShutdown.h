/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 * This function takes a pointer to a smart pointer and nulls the smart pointer
 * on shutdown.
 *
 * This is useful if you have a global smart pointer object which you don't
 * want to "leak" on shutdown.
 *
 * Although ClearOnShutdown will work with any smart pointer (i.e., nsCOMPtr,
 * nsRefPtr, nsAutoPtr, StaticRefPtr, and StaticAutoPtr), you probably want to
 * use it only with StaticRefPtr and StaticAutoPtr.  There is no way to undo a
 * call to ClearOnShutdown, so you can call it only on smart pointers which you
 * know will live until the program shuts down.  In practice, these are likely
 * global variables, which should be Static{Ref,Auto}Ptr.
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
  virtual ~ShutdownObserver() {}
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
