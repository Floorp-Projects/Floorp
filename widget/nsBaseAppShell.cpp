/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/message_loop.h"

#include "nsBaseAppShell.h"
#if defined(MOZ_CRASHREPORTER)
#include "nsExceptionHandler.h"
#endif
#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"

// When processing the next thread event, the appshell may process native
// events (if not in performance mode), which can result in suppressing the
// next thread event for at most this many ticks:
#define THREAD_EVENT_STARVATION_LIMIT PR_MillisecondsToInterval(20)

NS_IMPL_ISUPPORTS(nsBaseAppShell, nsIAppShell, nsIThreadObserver, nsIObserver)

nsBaseAppShell::nsBaseAppShell()
  : mSuspendNativeCount(0)
  , mEventloopNestingLevel(0)
  , mBlockedWait(nullptr)
  , mNativeEventPending(false)
  , mEventloopNestingState(eEventloopNone)
  , mRunning(false)
  , mExiting(false)
  , mBlockNativeEvent(false)
{
}

nsBaseAppShell::~nsBaseAppShell()
{
  NS_ASSERTION(mSyncSections.IsEmpty(), "Must have run all sync sections");
}

nsresult
nsBaseAppShell::Init()
{
  // Configure ourselves as an observer for the current thread:

  nsCOMPtr<nsIThreadInternal> threadInt =
      do_QueryInterface(NS_GetCurrentThread());
  NS_ENSURE_STATE(threadInt);

  threadInt->SetObserver(this);

  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  if (obsSvc)
    obsSvc->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  return NS_OK;
}

// Called by nsAppShell's native event callback
void
nsBaseAppShell::NativeEventCallback()
{
  if (!mNativeEventPending.exchange(false))
    return;

  // If DoProcessNextNativeEvent is on the stack, then we assume that we can
  // just unwind and let nsThread::ProcessNextEvent process the next event.
  // However, if we are called from a nested native event loop (maybe via some
  // plug-in or library function), then go ahead and process Gecko events now.
  if (mEventloopNestingState == eEventloopXPCOM) {
    mEventloopNestingState = eEventloopOther;
    // XXX there is a tiny risk we will never get a new NativeEventCallback,
    // XXX see discussion in bug 389931.
    return;
  }

  // nsBaseAppShell::Run is not being used to pump events, so this may be
  // our only opportunity to process pending gecko events.

  nsIThread *thread = NS_GetCurrentThread();
  bool prevBlockNativeEvent = mBlockNativeEvent;
  if (mEventloopNestingState == eEventloopOther) {
    if (!NS_HasPendingEvents(thread))
      return;
    // We're in a nested native event loop and have some gecko events to
    // process.  While doing that we block processing native events from the
    // appshell - instead, we want to get back to the nested native event
    // loop ASAP (bug 420148).
    mBlockNativeEvent = true;
  }

  IncrementEventloopNestingLevel();
  EventloopNestingState prevVal = mEventloopNestingState;
  NS_ProcessPendingEvents(thread, THREAD_EVENT_STARVATION_LIMIT);
  mProcessedGeckoEvents = true;
  mEventloopNestingState = prevVal;
  mBlockNativeEvent = prevBlockNativeEvent;

  // Continue processing pending events later (we don't want to starve the
  // embedders event loop).
  if (NS_HasPendingEvents(thread))
    DoProcessMoreGeckoEvents();

  DecrementEventloopNestingLevel();
}

// Note, this is currently overidden on windows, see comments in nsAppShell for
// details.
void
nsBaseAppShell::DoProcessMoreGeckoEvents()
{
  OnDispatchedEvent(nullptr);
}


// Main thread via OnProcessNextEvent below
bool
nsBaseAppShell::DoProcessNextNativeEvent(bool mayWait, uint32_t recursionDepth)
{
  // The next native event to be processed may trigger our NativeEventCallback,
  // in which case we do not want it to process any thread events since we'll
  // do that when this function returns.
  //
  // If the next native event is not our NativeEventCallback, then we may end
  // up recursing into this function.
  //
  // However, if the next native event is not our NativeEventCallback, but it
  // results in another native event loop, then our NativeEventCallback could
  // fire and it will see mEventloopNestingState as eEventloopOther.
  //
  EventloopNestingState prevVal = mEventloopNestingState;
  mEventloopNestingState = eEventloopXPCOM;

  IncrementEventloopNestingLevel();

  bool result = ProcessNextNativeEvent(mayWait);

  // Make sure that any sync sections registered during this most recent event
  // are run now. This is not considered a stable state because we're not back
  // to the event loop yet.
  RunSyncSections(false, recursionDepth);

  DecrementEventloopNestingLevel();

  mEventloopNestingState = prevVal;
  return result;
}

//-------------------------------------------------------------------------
// nsIAppShell methods:

NS_IMETHODIMP
nsBaseAppShell::Run(void)
{
  NS_ENSURE_STATE(!mRunning);  // should not call Run twice
  mRunning = true;

  nsIThread *thread = NS_GetCurrentThread();

  MessageLoop::current()->Run();

  NS_ProcessPendingEvents(thread);

  mRunning = false;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::Exit(void)
{
  if (mRunning && !mExiting) {
    MessageLoop::current()->Quit();
  }
  mExiting = true;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::SuspendNative()
{
  ++mSuspendNativeCount;
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::ResumeNative()
{
  --mSuspendNativeCount;
  NS_ASSERTION(mSuspendNativeCount >= 0, "Unbalanced call to nsBaseAppShell::ResumeNative!");
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::GetEventloopNestingLevel(uint32_t* aNestingLevelResult)
{
  NS_ENSURE_ARG_POINTER(aNestingLevelResult);

  *aNestingLevelResult = mEventloopNestingLevel;

  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIThreadObserver methods:

// Called from any thread
NS_IMETHODIMP
nsBaseAppShell::OnDispatchedEvent(nsIThreadInternal *thr)
{
  if (mBlockNativeEvent)
    return NS_OK;

  if (mNativeEventPending.exchange(true))
    return NS_OK;

  // Returns on the main thread in NativeEventCallback above
  ScheduleNativeEventCallback();
  return NS_OK;
}

// Called from the main thread
NS_IMETHODIMP
nsBaseAppShell::OnProcessNextEvent(nsIThreadInternal *thr, bool mayWait,
                                   uint32_t recursionDepth)
{
  if (mBlockNativeEvent) {
    if (!mayWait)
      return NS_OK;
    // Hmm, we're in a nested native event loop and would like to get
    // back to it ASAP, but it seems a gecko event has caused us to
    // spin up a nested XPCOM event loop (eg. modal window), so we
    // really must start processing native events here again.
    mBlockNativeEvent = false;
    if (NS_HasPendingEvents(thr))
      OnDispatchedEvent(thr); // in case we blocked it earlier
  }

  // Unblock outer nested wait loop (below).
  if (mBlockedWait)
    *mBlockedWait = false;

  bool *oldBlockedWait = mBlockedWait;
  mBlockedWait = &mayWait;

  // When mayWait is true, we need to make sure that there is an event in the
  // thread's event queue before we return.  Otherwise, the thread will block
  // on its event queue waiting for an event.
  bool needEvent = mayWait;
  // Reset prior to invoking DoProcessNextNativeEvent which might cause
  // NativeEventCallback to process gecko events.
  mProcessedGeckoEvents = false;

  DoProcessNextNativeEvent(false, recursionDepth);

  while (!NS_HasPendingEvents(thr) && !mProcessedGeckoEvents) {
    // If we have been asked to exit from Run, then we should not wait for
    // events to process.  Note that an inner nested event loop causes
    // 'mayWait' to become false too, through 'mBlockedWait'.
    if (mExiting)
      mayWait = false;

    if (!DoProcessNextNativeEvent(mayWait, recursionDepth) || !mayWait)
      break;
  }

  mBlockedWait = oldBlockedWait;

  // Make sure that the thread event queue does not block on its monitor, as
  // it normally would do if it did not have any pending events.  To avoid
  // that, we simply insert a dummy event into its queue during shutdown.
  if (needEvent && !mExiting && !NS_HasPendingEvents(thr)) {
    DispatchDummyEvent(thr);
  }

  // We're about to run an event, so we're in a stable state.
  RunSyncSections(true, recursionDepth);

  return NS_OK;
}

bool
nsBaseAppShell::DispatchDummyEvent(nsIThread* aTarget)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mDummyEvent)
    mDummyEvent = new nsRunnable();

  return NS_SUCCEEDED(aTarget->Dispatch(mDummyEvent, NS_DISPATCH_NORMAL));
}

void
nsBaseAppShell::IncrementEventloopNestingLevel()
{
  ++mEventloopNestingLevel;
#if defined(MOZ_CRASHREPORTER)
  CrashReporter::SetEventloopNestingLevel(mEventloopNestingLevel);
#endif
}

void
nsBaseAppShell::DecrementEventloopNestingLevel()
{
  --mEventloopNestingLevel;
#if defined(MOZ_CRASHREPORTER)
  CrashReporter::SetEventloopNestingLevel(mEventloopNestingLevel);
#endif
}

void
nsBaseAppShell::RunSyncSectionsInternal(bool aStable,
                                        uint32_t aThreadRecursionLevel)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mSyncSections.IsEmpty(), "Nothing to do!");

  // We've got synchronous sections. Run all of them that are are awaiting a
  // stable state if aStable is true (i.e. we really are in a stable state).
  // Also run the synchronous sections that are simply waiting for the right
  // combination of event loop nesting level and thread recursion level.
  // Note that a synchronous section could add another synchronous section, so
  // we don't remove elements from mSyncSections until all sections have been
  // run, or else we'll screw up our iteration. Any sync sections that are not
  // ready to be run are saved for later.

  nsTArray<SyncSection> pendingSyncSections;

  for (uint32_t i = 0; i < mSyncSections.Length(); i++) {
    SyncSection& section = mSyncSections[i];
    if ((aStable && section.mStable) ||
        (!section.mStable &&
         section.mEventloopNestingLevel == mEventloopNestingLevel &&
         section.mThreadRecursionLevel == aThreadRecursionLevel)) {
      section.mRunnable->Run();
    }
    else {
      // Add to pending list.
      SyncSection* pending = pendingSyncSections.AppendElement();
      section.Forget(pending);
    }
  }

  mSyncSections.SwapElements(pendingSyncSections);
}

void
nsBaseAppShell::ScheduleSyncSection(nsIRunnable* aRunnable, bool aStable)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  nsIThread* thread = NS_GetCurrentThread();

  // Add this runnable to our list of synchronous sections.
  SyncSection* section = mSyncSections.AppendElement();
  section->mStable = aStable;
  section->mRunnable = aRunnable;

  // If aStable is false then this synchronous section is supposed to run before
  // the next event at the current nesting level. Record the event loop nesting
  // level and the thread recursion level so that the synchronous section will
  // run at the proper time.
  if (!aStable) {
    section->mEventloopNestingLevel = mEventloopNestingLevel;

    nsCOMPtr<nsIThreadInternal> threadInternal = do_QueryInterface(thread);
    NS_ASSERTION(threadInternal, "This should never fail!");

    uint32_t recursionLevel;
    if (NS_FAILED(threadInternal->GetRecursionDepth(&recursionLevel))) {
      NS_ERROR("This should never fail!");
    }

    // Due to the weird way that the thread recursion counter is implemented we
    // subtract one from the recursion level if we have one.
    section->mThreadRecursionLevel = recursionLevel ? recursionLevel - 1 : 0;
  }

  // Ensure we've got a pending event, else the callbacks will never run.
  if (!NS_HasPendingEvents(thread) && !DispatchDummyEvent(thread)) {
    RunSyncSections(true, 0);
  }
}

// Called from the main thread
NS_IMETHODIMP
nsBaseAppShell::AfterProcessNextEvent(nsIThreadInternal *thr,
                                      uint32_t recursionDepth,
                                      bool eventWasProcessed)
{
  // We've just finished running an event, so we're in a stable state.
  RunSyncSections(true, recursionDepth);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::Observe(nsISupports *subject, const char *topic,
                        const char16_t *data)
{
  NS_ASSERTION(!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID), "oops");
  Exit();
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::RunInStableState(nsIRunnable* aRunnable)
{
  ScheduleSyncSection(aRunnable, true);
  return NS_OK;
}

NS_IMETHODIMP
nsBaseAppShell::RunBeforeNextEvent(nsIRunnable* aRunnable)
{
  ScheduleSyncSection(aRunnable, false);
  return NS_OK;
}
