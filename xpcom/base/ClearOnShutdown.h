/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ClearOnShutdown_h
#define mozilla_ClearOnShutdown_h

#include "mozilla/LinkedList.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Array.h"
#include "ShutdownPhase.h"
#include "MainThreadUtils.h"

#include <functional>

/*
 * This header exports two public methods in the mozilla namespace:
 *
 *   template<class SmartPtr>
 *   void ClearOnShutdown(SmartPtr *aPtr, aPhase=ShutdownPhase::ShutdownFinal)
 *
 * This function takes a pointer to a smart pointer and nulls the smart pointer
 * on shutdown (and a particular phase of shutdown as needed).  If a phase
 * is specified, the ptr will be cleared at the start of that phase.  Also,
 * if a phase has already occurred when ClearOnShutdown() is called it will
 * cause a MOZ_ASSERT.  In case a phase is not explicitly cleared we will
 * clear it on the next phase that occurs.
 *
 * This is useful if you have a global smart pointer object which you don't
 * want to "leak" on shutdown.
 *
 * Although ClearOnShutdown will work with any smart pointer (i.e., nsCOMPtr,
 * RefPtr, StaticRefPtr, and StaticAutoPtr), you probably want to
 * use it only with StaticRefPtr and StaticAutoPtr.  There is no way to undo a
 * call to ClearOnShutdown, so you can call it only on smart pointers which you
 * know will live until the program shuts down.  In practice, these are likely
 * global variables, which should be Static{Ref,Auto}Ptr.
 *
 *   template <typename CallableT>
 *   void RunOnShutdown(CallableT&& aCallable,
 *                      aPhase = ShutdownPhase::ShutdownFinal)
 *
 * This function takes a callable and executes it upon shutdown at the start of
 * the specified phase. If the phase has already occurred when RunOnShutdown()
 * is called, it will cause a MOZ_ASSERT. In case a phase is not explicitly
 * cleared, we will clear it on the next phase that occurs.
 *
 * ClearOnShutdown and RunOnShutdown are both currently main-thread only because
 * we don't want to accidentally free an object from a different thread than the
 * one it was created on.
 */

namespace mozilla {

namespace ClearOnShutdown_Internal {

class ShutdownObserver : public LinkedListElement<ShutdownObserver> {
 public:
  virtual void Shutdown() = 0;
  virtual ~ShutdownObserver() = default;
};

template <class SmartPtr>
class PointerClearer : public ShutdownObserver {
 public:
  explicit PointerClearer(SmartPtr* aPtr) : mPtr(aPtr) {}

  virtual void Shutdown() override {
    if (mPtr) {
      *mPtr = nullptr;
    }
  }

 private:
  SmartPtr* mPtr;
};

class FunctionInvoker : public ShutdownObserver {
 public:
  template <typename CallableT>
  explicit FunctionInvoker(CallableT&& aCallable)
      : mCallable(std::forward<CallableT>(aCallable)) {}

  virtual void Shutdown() override {
    if (!mCallable) {
      return;
    }

    mCallable();
  }

 private:
  std::function<void()> mCallable;
};

void InsertIntoShutdownList(ShutdownObserver* aShutdownObserver,
                            ShutdownPhase aPhase);

typedef LinkedList<ShutdownObserver> ShutdownList;
extern Array<StaticAutoPtr<ShutdownList>,
             static_cast<size_t>(ShutdownPhase::ShutdownPhase_Length)>
    sShutdownObservers;
extern ShutdownPhase sCurrentShutdownPhase;

}  // namespace ClearOnShutdown_Internal

template <class SmartPtr>
inline void ClearOnShutdown(
    SmartPtr* aPtr, ShutdownPhase aPhase = ShutdownPhase::ShutdownFinal) {
  using namespace ClearOnShutdown_Internal;

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPhase != ShutdownPhase::ShutdownPhase_Length);

  InsertIntoShutdownList(new PointerClearer<SmartPtr>(aPtr), aPhase);
}

template <typename CallableT>
inline void RunOnShutdown(CallableT&& aCallable,
                          ShutdownPhase aPhase = ShutdownPhase::ShutdownFinal) {
  using namespace ClearOnShutdown_Internal;

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPhase != ShutdownPhase::ShutdownPhase_Length);

  InsertIntoShutdownList(
      new FunctionInvoker(std::forward<CallableT>(aCallable)), aPhase);
}

// Called when XPCOM is shutting down, after all shutdown notifications have
// been sent and after all threads' event loops have been purged.
void KillClearOnShutdown(ShutdownPhase aPhase);

}  // namespace mozilla

#endif
