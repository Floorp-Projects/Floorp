/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinHandleWatcher_h__
#define WinHandleWatcher_h__

#include <minwindef.h>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

namespace mozilla {
///////////////////////////////////////////////////////////////////////
// HandleWatcher
//
// Enqueues a task onto an event target when a watched Win32 synchronization
// object [1] enters the signaled state.
//
// The HandleWatcher must be stopped before either it or the synchronization
// object is destroyed.
//
//////
//
// Example of use:
//
// ```
// class MyClass {
//   /* ... */
//
//   HANDLE CreateThing();
//   void OnComplete();
//  public:
//   void Fire() {
//     mHandle.set(CreateThing());
//     mWatcher.Watch(
//         mHandle.get(), NS_GetCurrentThread(),  // (or any other thread)
//         NS_NewRunnableFunction("OnComplete", [this] { OnComplete(); }));
//   }
//
//   ~MyClass() { mWatcher.Stop(); }
//
//   HandleWatcher mWatcher;
//   HandlePtr mHandle;  // calls ::CloseHandle() on destruction
// };
// ```
//
// Note: this example demonstrates why an explicit `Stop()` is necessary in
// MyClass's destructor. Without it, the `HandlePtr` would destroy the HANDLE --
// and possibly whatever other data `OnComplete()` depends on -- before the
// watch was stopped!
//
// Rather than make code correctness silently dependent on member object order,
// we require that HandleWatcher already be stopped at its destruction time.
// (This does not guarantee correctness, as the task may still reference a
// partially-destroyed transitive owner; but, short of RIIR, a guarantee of
// correctness is probably not possible here.)
//
//////
//
// [1]https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-objects
class HandleWatcher {
 public:
  class Impl;

  HandleWatcher();
  ~HandleWatcher();

  HandleWatcher(HandleWatcher const&) = delete;
  HandleWatcher& operator=(HandleWatcher const&) = delete;

  HandleWatcher(HandleWatcher&&) noexcept;
  HandleWatcher& operator=(HandleWatcher&&) noexcept;

  // Watches the given Win32 HANDLE, which must be a synchronization object. As
  // soon as the HANDLE is signaled, posts `aRunnable` to `aTarget`.
  //
  // `aHandle` is merely borrowed for the duration of the watch: the
  // HandleWatcher does not attempt to close it, and its lifetime must exceed
  // that of the watch.
  //
  // If the watch is stopped for any reason other than completion, `aRunnable`
  // is released immediately, on the same thread from which the Watch was
  // stopped.
  //
  // The watch is stopped when any of the following occurs:
  //   * `Stop()` is called.
  //   * `Watch()` is called again, even without an intervening `Stop()`.
  //   * This object is destroyed.
  //   * `aTarget` shuts down.
  //   * `aHandle` becomes signaled.
  //
  void Watch(HANDLE aHandle, nsIEventTarget* aTarget,
             already_AddRefed<nsIRunnable> aRunnable);

  // Cancels the current watch, if any.
  //
  // Idempotent. Thread-safe with respect to other calls of `Stop()`.
  void Stop();

  // Potentially racy. Only intended for tests.
  bool IsStopped();

 private:
  RefPtr<Impl> mImpl;
};
}  // namespace mozilla

#endif  // WinHandleWatcher_h__
