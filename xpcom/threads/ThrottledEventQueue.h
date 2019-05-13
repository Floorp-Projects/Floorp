/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsIEventTarget wrapper for throttling event dispatch.

#ifndef mozilla_ThrottledEventQueue_h
#define mozilla_ThrottledEventQueue_h

#include "nsISerialEventTarget.h"

#define NS_THROTTLEDEVENTQUEUE_IID                   \
  {                                                  \
    0x8f3cf7dc, 0xfc14, 0x4ad5, {                    \
      0x9f, 0xd5, 0xdb, 0x79, 0xbc, 0xe6, 0xd5, 0x08 \
    }                                                \
  }

namespace mozilla {

// A ThrottledEventQueue is an event target that can be used to throttle
// events being dispatched to another base target.  It maintains its
// own queue of events and only dispatches one at a time to the wrapped
// target.  This can be used to avoid flooding the base target.
//
// Flooding is avoided via a very simple principle.  Runnables dispatched
// to the ThrottledEventQueue are only dispatched to the base target
// one at a time.  Only once that runnable has executed will we dispatch
// the next runnable to the base target.  This in effect makes all
// runnables passing through the ThrottledEventQueue yield to other work
// on the base target.
//
// ThrottledEventQueue keeps runnables waiting to be dispatched to the
// base in its own internal queue.  Code can query the length of this
// queue using IsEmpty() and Length().  Further, code implement back
// pressure by checking the depth of the queue and deciding to stop
// issuing runnables if they see the ThrottledEventQueue is backed up.
// Code running on other threads could even use AwaitIdle() to block
// all operation until the ThrottledEventQueue drains.
//
// Note, this class is similar to TaskQueue, but also differs in a few
// ways.  First, it is a very simple nsIEventTarget implementation.  It
// does not use the AbstractThread API.
//
// In addition, ThrottledEventQueue currently dispatches its next
// runnable to the base target *before* running the current event.  This
// allows the event code to spin the event loop without stalling the
// ThrottledEventQueue.  In contrast, TaskQueue only dispatches its next
// runnable after running the current event.  That approach is necessary
// for TaskQueue in order to work with thread pool targets.
//
// So, if you are targeting a thread pool you probably want a TaskQueue.
// If you are targeting a single thread or other non-concurrent event
// target, you probably want a ThrottledEventQueue.
//
// If you drop a ThrottledEventQueue while its queue still has events to be run,
// they will continue to be dispatched as usual to the base. Only once the last
// event has run will all the ThrottledEventQueue's memory be freed.
class ThrottledEventQueue final : public nsISerialEventTarget {
  class Inner;
  RefPtr<Inner> mInner;

  explicit ThrottledEventQueue(already_AddRefed<Inner> aInner);
  ~ThrottledEventQueue() = default;

 public:
  // Create a ThrottledEventQueue for the given target.
  static already_AddRefed<ThrottledEventQueue> Create(
      nsISerialEventTarget* aBaseTarget,
      const char* aName,
      uint32_t aPriority = nsIRunnablePriority::PRIORITY_NORMAL);

  // Determine if there are any events pending in the queue.
  bool IsEmpty() const;

  // Determine how many events are pending in the queue.
  uint32_t Length() const;

  // Block the current thread until the queue is empty. This may not be called
  // on the main thread or the base target. The ThrottledEventQueue must not be
  // paused.
  void AwaitIdle() const;

  // If |aIsPaused| is true, pause execution of events from this queue. No
  // events from this queue will be run until this is called with |aIsPaused|
  // false.
  //
  // To un-pause a ThrottledEventQueue, we need to dispatch a runnable to the
  // underlying event target. That operation may fail, so this method is
  // fallible as well.
  //
  // Note that, although ThrottledEventQueue's behavior is descibed as queueing
  // events on the base target, an event queued on a TEQ is never actually moved
  // to any other queue. What is actually dispatched to the base is an
  // "executor" event which, when run, removes an event from the TEQ and runs it
  // immediately. This means that you can pause a TEQ even after the executor
  // has been queued on the base target, and even so, no events from the TEQ
  // will run. When the base target gets around to running the executor, the
  // executor will see that the TEQ is paused, and do nothing.
  MOZ_MUST_USE nsresult SetIsPaused(bool aIsPaused);

  // Return true if this ThrottledEventQueue is paused.
  bool IsPaused() const;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_THROTTLEDEVENTQUEUE_IID);
};

NS_DEFINE_STATIC_IID_ACCESSOR(ThrottledEventQueue, NS_THROTTLEDEVENTQUEUE_IID);

}  // namespace mozilla

#endif  // mozilla_ThrottledEventQueue_h
