/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventQueue_h
#define mozilla_EventQueue_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/Queue.h"
#include "nsCOMPtr.h"

class nsIRunnable;

namespace mozilla {

class EventQueue final : public AbstractEventQueue
{
public:
  EventQueue() {}
  explicit EventQueue(EventPriority aPriority);

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventPriority aPriority,
                const MutexAutoLock& aProofOfLock) final override;
  already_AddRefed<nsIRunnable> GetEvent(EventPriority* aPriority,
                                         const MutexAutoLock& aProofOfLock) final override;

  bool IsEmpty(const MutexAutoLock& aProofOfLock) final override;
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock) final override;

  size_t Count(const MutexAutoLock& aProofOfLock) const final override;
  already_AddRefed<nsIRunnable> PeekEvent(const MutexAutoLock& aProofOfLock);

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final override {}
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final override {}
  void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) final override {}
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final override {}

private:
  mozilla::Queue<nsCOMPtr<nsIRunnable>> mQueue;
};

} // namespace mozilla

#endif // mozilla_EventQueue_h
