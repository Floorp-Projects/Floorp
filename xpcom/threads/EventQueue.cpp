/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventQueue.h"
#include "nsIRunnable.h"

using namespace mozilla;

EventQueue::EventQueue()
{
}

void
EventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                     EventPriority aPriority,
                     const MutexAutoLock& aProofOfLock)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  mQueue.Push(Move(event));
}

already_AddRefed<nsIRunnable>
EventQueue::GetEvent(EventPriority* aPriority,
                     const MutexAutoLock& aProofOfLock)
{
  if (mQueue.IsEmpty()) {
    return nullptr;
  }

  if (aPriority) {
    *aPriority = EventPriority::Normal;
  }

  nsCOMPtr<nsIRunnable> result = mQueue.Pop();
  return result.forget();
}

bool
EventQueue::IsEmpty(const MutexAutoLock& aProofOfLock)
{
  return mQueue.IsEmpty();
}

bool
EventQueue::HasReadyEvent(const MutexAutoLock& aProofOfLock)
{
  return !IsEmpty(aProofOfLock);
}

already_AddRefed<nsIRunnable>
EventQueue::PeekEvent(const MutexAutoLock& aProofOfLock)
{
  if (mQueue.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> result = mQueue.FirstElement();
  return result.forget();
}

size_t
EventQueue::Count(const MutexAutoLock& aProofOfLock) const
{
  return mQueue.Count();
}
