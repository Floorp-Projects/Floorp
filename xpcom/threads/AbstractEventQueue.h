/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AbstractEventQueue_h
#define mozilla_AbstractEventQueue_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Mutex.h"

class nsIRunnable;

namespace mozilla {

enum class EventPriority
{
  High,
  Input,
  Normal,
  Idle
};

// AbstractEventQueue is an abstract base class for all our unsynchronized event
// queue implementations:
// - EventQueue: A queue of runnables. Used for non-main threads.
// - PrioritizedEventQueue: Contains a queue for each priority level.
//       Has heuristics to decide which queue to pop from. Events are
//       pushed into the queue corresponding to their priority.
//       Used for the main thread.
//
// Since AbstractEventQueue implementations are unsynchronized, they should be
// wrapped in an outer SynchronizedEventQueue implementation (like
// ThreadEventQueue).
class AbstractEventQueue
{
public:
  // Add an event to the end of the queue. Implementors are free to use
  // aPriority however they wish. They may ignore it if the runnable has its own
  // intrinsic priority (via nsIRunnablePriority).
  virtual void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                        EventPriority aPriority,
                        const MutexAutoLock& aProofOfLock) = 0;

  // Get an event from the front of the queue. aPriority is an out param. If the
  // implementation supports priorities, then this should be the same priority
  // that the event was pushed with. aPriority may be null. This should return
  // null if the queue is non-empty but the event in front is not ready to run.
  virtual already_AddRefed<nsIRunnable> GetEvent(EventPriority* aPriority,
                                                 const MutexAutoLock& aProofOfLock) = 0;

  // Returns true if the queue is empty. Implies !HasReadyEvent().
  virtual bool IsEmpty(const MutexAutoLock& aProofOfLock) = 0;

  // Returns true if the queue is non-empty and if the event in front is ready
  // to run. Implies !IsEmpty(). This should return true iff GetEvent returns a
  // non-null value.
  virtual bool HasReadyEvent(const MutexAutoLock& aProofOfLock) = 0;

  // Returns the number of events in the queue.
  virtual size_t Count(const MutexAutoLock& aProofOfLock) const = 0;

  virtual void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) = 0;
  virtual void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) = 0;
  virtual void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) = 0;
  virtual void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) = 0;

  virtual ~AbstractEventQueue() {}
};

} // namespace mozilla

#endif // mozilla_AbstractEventQueue_h
