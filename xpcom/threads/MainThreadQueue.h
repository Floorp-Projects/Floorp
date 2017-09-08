/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MainThreadQueue_h
#define mozilla_MainThreadQueue_h

#include "mozilla/Attributes.h"
#include "mozilla/EventQueue.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsThread.h"
#include "PrioritizedEventQueue.h"

namespace mozilla {

template<typename SynchronizedQueueT, typename InnerQueueT>
inline already_AddRefed<nsThread>
CreateMainThread(nsIIdlePeriod* aIdlePeriod, SynchronizedQueueT** aSynchronizedQueue = nullptr)
{
  using MainThreadQueueT = PrioritizedEventQueue<InnerQueueT>;

  auto queue = MakeUnique<MainThreadQueueT>(
    MakeUnique<InnerQueueT>(),
    MakeUnique<InnerQueueT>(),
    MakeUnique<InnerQueueT>(),
    MakeUnique<InnerQueueT>(),
    do_AddRef(aIdlePeriod));

  MainThreadQueueT* prioritized = queue.get();

  RefPtr<SynchronizedQueueT> synchronizedQueue = new SynchronizedQueueT(Move(queue));

  prioritized->SetMutexRef(synchronizedQueue->MutexRef());

  // Setup "main" thread
  RefPtr<nsThread> mainThread = new nsThread(WrapNotNull(synchronizedQueue), nsThread::MAIN_THREAD, 0);

#ifndef RELEASE_OR_BETA
  prioritized->SetNextIdleDeadlineRef(mainThread->NextIdleDeadlineRef());
#endif

  if (aSynchronizedQueue) {
    synchronizedQueue.forget(aSynchronizedQueue);
  }
  return mainThread.forget();
}

} // namespace mozilla

#endif // mozilla_MainThreadQueue_h
