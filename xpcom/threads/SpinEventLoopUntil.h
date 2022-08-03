/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcom_threads_SpinEventLoopUntil_h__
#define xpcom_threads_SpinEventLoopUntil_h__

#include "MainThreadUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/StaticMutex.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

class nsIThread;

// A wrapper for nested event loops.
//
// This function is intended to make code more obvious (do you remember
// what NS_ProcessNextEvent(nullptr, true) means?) and slightly more
// efficient, as people often pass nullptr or NS_GetCurrentThread to
// NS_ProcessNextEvent, which results in needless querying of the current
// thread every time through the loop.
//
// You should use this function in preference to NS_ProcessNextEvent inside
// a loop unless one of the following is true:
//
// * You need to pass `false` to NS_ProcessNextEvent; or
// * You need to do unusual things around the call to NS_ProcessNextEvent,
//   such as unlocking mutexes that you are holding.
//
// If you *do* need to call NS_ProcessNextEvent manually, please do call
// NS_GetCurrentThread() outside of your loop and pass the returned pointer
// into NS_ProcessNextEvent for a tiny efficiency win.
namespace mozilla {

// You should normally not need to deal with this template parameter.  If
// you enjoy esoteric event loop details, read on.
//
// If you specify that NS_ProcessNextEvent wait for an event, it is possible
// for NS_ProcessNextEvent to return false, i.e. to indicate that an event
// was not processed.  This can only happen when the thread has been shut
// down by another thread, but is still attempting to process events outside
// of a nested event loop.
//
// This behavior is admittedly strange.  The scenario it deals with is the
// following:
//
// * The current thread has been shut down by some owner thread.
// * The current thread is spinning an event loop waiting for some condition
//   to become true.
// * Said condition is actually being fulfilled by another thread, so there
//   are timing issues in play.
//
// Thus, there is a small window where the current thread's event loop
// spinning can check the condition, find it false, and call
// NS_ProcessNextEvent to wait for another event.  But we don't actually
// want it to wait indefinitely, because there might not be any other events
// in the event loop, and the current thread can't accept dispatched events
// because it's being shut down.  Thus, actually blocking would hang the
// thread, which is bad.  The solution, then, is to detect such a scenario
// and not actually block inside NS_ProcessNextEvent.
//
// But this is a problem, because we want to return the status of
// NS_ProcessNextEvent to the caller of SpinEventLoopUntil if possible.  In
// the above scenario, however, we'd stop spinning prematurely and cause
// all sorts of havoc.  We therefore have this template parameter to
// control whether errors are ignored or passed out to the caller of
// SpinEventLoopUntil.  The latter is the default; if you find yourself
// wanting to use the former, you should think long and hard before doing
// so, and write a comment like this defending your choice.

enum class ProcessFailureBehavior {
  IgnoreAndContinue,
  ReportToCaller,
};

// SpinEventLoopUntil is a dangerous operation that can result in hangs.
// In particular during shutdown we want to know if we are hanging
// inside a nested event loop on the main thread.
// This is a helper annotation class to keep track of this.
struct MOZ_STACK_CLASS AutoNestedEventLoopAnnotation {
  explicit AutoNestedEventLoopAnnotation(const nsACString& aEntry)
      : mPrev(nullptr) {
    if (NS_IsMainThread()) {
      StaticMutexAutoLock lock(sStackMutex);
      mPrev = sCurrent;
      sCurrent = this;
      if (mPrev) {
        mStack = mPrev->mStack + "|"_ns + aEntry;
      } else {
        mStack = aEntry;
      }
      AnnotateXPCOMSpinEventLoopStack(mStack);
    }
  }

  ~AutoNestedEventLoopAnnotation() {
    if (NS_IsMainThread()) {
      StaticMutexAutoLock lock(sStackMutex);
      MOZ_ASSERT(sCurrent == this);
      sCurrent = mPrev;
      if (mPrev) {
        AnnotateXPCOMSpinEventLoopStack(mPrev->mStack);
      } else {
        AnnotateXPCOMSpinEventLoopStack(""_ns);
      }
    }
  }

  static void CopyCurrentStack(nsCString& aNestedSpinStack) {
    // We need to copy this behind a mutex as the
    // memory for our instances is stack-bound and
    // can go away at any time.
    StaticMutexAutoLock lock(sStackMutex);
    if (sCurrent) {
      aNestedSpinStack = sCurrent->mStack;
    } else {
      aNestedSpinStack = "(no nested event loop active)"_ns;
    }
  }

 private:
  AutoNestedEventLoopAnnotation(const AutoNestedEventLoopAnnotation&) = delete;
  AutoNestedEventLoopAnnotation& operator=(
      const AutoNestedEventLoopAnnotation&) = delete;

  // The declarations of these statics live in nsThreadManager.cpp.
  static AutoNestedEventLoopAnnotation* sCurrent MOZ_GUARDED_BY(sStackMutex);
  static StaticMutex sStackMutex;

  // We need this to avoid the inclusion of nsExceptionHandler.h here
  // which can include windows.h which disturbs some dom/media/gtest.
  // The implementation lives in nsThreadManager.cpp.
  static void AnnotateXPCOMSpinEventLoopStack(const nsACString& aStack);

  AutoNestedEventLoopAnnotation* mPrev MOZ_GUARDED_BY(sStackMutex);
  nsCString mStack MOZ_GUARDED_BY(sStackMutex);
};

// Please see the above notes for the Behavior template parameter.
//
// aVeryGoodReasonToDoThis is usually a literal string unique to each
//   caller that can be recognized in the XPCOMSpinEventLoopStack
//   annotation.
// aPredicate is the condition we wait for.
// aThread can be used to specify a thread, see the above introduction.
//   It defaults to the current thread.
template <
    ProcessFailureBehavior Behavior = ProcessFailureBehavior::ReportToCaller,
    typename Pred>
bool SpinEventLoopUntil(const nsACString& aVeryGoodReasonToDoThis,
                        Pred&& aPredicate, nsIThread* aThread = nullptr) {
  // Prepare the annotations
  AutoNestedEventLoopAnnotation annotation(aVeryGoodReasonToDoThis);
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_NONSENSITIVE(
      "SpinEventLoopUntil", OTHER, aVeryGoodReasonToDoThis);
  AUTO_PROFILER_MARKER_TEXT("SpinEventLoop", OTHER, MarkerStack::Capture(),
                            aVeryGoodReasonToDoThis);

  nsIThread* thread = aThread ? aThread : NS_GetCurrentThread();

  // From a latency perspective, spinning the event loop is like leaving script
  // and returning to the event loop. Tell the watchdog we stopped running
  // script (until we return).
  mozilla::Maybe<xpc::AutoScriptActivity> asa;
  if (NS_IsMainThread()) {
    asa.emplace(false);
  }

  while (!aPredicate()) {
    bool didSomething = NS_ProcessNextEvent(thread, true);

    if (Behavior == ProcessFailureBehavior::IgnoreAndContinue) {
      // Don't care what happened, continue on.
      continue;
    } else if (!didSomething) {
      return false;
    }
  }

  return true;
}

}  // namespace mozilla

#endif  // xpcom_threads_SpinEventLoopUntil_h__
