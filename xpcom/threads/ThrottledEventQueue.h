/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsIEventTarget wrapper for throttling event dispatch.

#ifndef mozilla_ThrottledEventQueue_h
#define mozilla_ThrottledEventQueue_h

#include "nsISerialEventTarget.h"

#define NS_THROTTLEDEVENTQUEUE_IID \
{ 0x8f3cf7dc, 0xfc14, 0x4ad5, \
  { 0x9f, 0xd5, 0xdb, 0x79, 0xbc, 0xe6, 0xd5, 0x08 } }

namespace mozilla {

// A ThrottledEventQueue is an event target that can be used to throttle
// events being dispatched to another base target.  It maintains its
// own queue of events and only dispatches one at a time to the wrapped
// target.  This can be used to avoid flooding the base target.
//
// Flooding is avoided via a very simply principal.  Runnables dispatched
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
// ThrottledEventQueue also implements an automatic shutdown mechanism.
// De-referencing the queue or browser shutdown will automatically begin
// shutdown.
//
// Once shutdown begins all events will bypass the queue and be dispatched
// straight to the underlying base target.
class ThrottledEventQueue final : public nsISerialEventTarget
{
  class Inner;
  RefPtr<Inner> mInner;

  explicit ThrottledEventQueue(already_AddRefed<Inner> aInner);
  ~ThrottledEventQueue();

  // Begin shutdown of the event queue.  This has no effect if shutdown
  // is already in process.  After this is called nsIEventTarget methods
  // will bypass the queue and operate directly on the base target.
  // Note, this could be made public if code needs to explicitly shutdown
  // for some reason.
  void MaybeStartShutdown();

public:
  // Attempt to create a ThrottledEventQueue for the given target.  This
  // may return nullptr if the browser is already shutting down.
  static already_AddRefed<ThrottledEventQueue>
  Create(nsISerialEventTarget* aBaseTarget);

  // Determine if there are any events pending in the queue.
  bool IsEmpty() const;

  // Determine how many events are pending in the queue.
  uint32_t Length() const;

  // Block the current thread until the queue is empty.  This may not
  // be called on the main thread or the base target.
  void AwaitIdle() const;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_THROTTLEDEVENTQUEUE_IID);
};

NS_DEFINE_STATIC_IID_ACCESSOR(ThrottledEventQueue, NS_THROTTLEDEVENTQUEUE_IID);

} // namespace mozilla

#endif // mozilla_ThrottledEventQueue_h
