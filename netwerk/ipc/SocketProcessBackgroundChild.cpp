/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessBackgroundChild.h"
#include "SocketProcessLogging.h"

#include "mozilla/ipc/Endpoint.h"
#include "nsThreadUtils.h"

namespace mozilla::net {

StaticMutex SocketProcessBackgroundChild::sMutex;
StaticRefPtr<SocketProcessBackgroundChild>
    SocketProcessBackgroundChild::sInstance;
StaticRefPtr<nsISerialEventTarget> SocketProcessBackgroundChild::sTaskQueue;

// static
RefPtr<SocketProcessBackgroundChild>
SocketProcessBackgroundChild::GetSingleton() {
  StaticMutexAutoLock lock(sMutex);
  return sInstance;
}

// static
void SocketProcessBackgroundChild::Create(
    ipc::Endpoint<PSocketProcessBackgroundChild>&& aEndpoint) {
  if (NS_WARN_IF(!aEndpoint.IsValid())) {
    MOZ_ASSERT_UNREACHABLE(
        "Can't create SocketProcessBackgroundChild with invalid endpoint");
    return;
  }

  nsCOMPtr<nsISerialEventTarget> transportQueue;
  if (NS_WARN_IF(NS_FAILED(NS_CreateBackgroundTaskQueue(
          "SocketBackgroundChildQueue", getter_AddRefs(transportQueue))))) {
    return;
  }

  RefPtr<SocketProcessBackgroundChild> actor =
      new SocketProcessBackgroundChild();

  transportQueue->Dispatch(NS_NewRunnableFunction(
      "BindSocketBackgroundChild",
      [endpoint = std::move(aEndpoint), actor]() mutable {
        // We checked endpoint validity before the dispatch, so this cannot
        // fail.
        MOZ_ALWAYS_TRUE(endpoint.Bind(actor));
      }));

  // Immediately store the actor and queue into the global.
  // Any messages dispatched to the queue will arrive after it has been bound.
  LOG(("SocketProcessBackgroundChild::Create"));
  StaticMutexAutoLock lock(sMutex);
  MOZ_ASSERT(!sInstance && !sTaskQueue,
             "Cannot initialize SocketProcessBackgroundChild twice!");
  sInstance = actor;
  sTaskQueue = transportQueue;
}

// static
void SocketProcessBackgroundChild::Shutdown() {
  nsCOMPtr<nsISerialEventTarget> taskQueue = TaskQueue();
  if (!taskQueue) {
    return;
  }

  taskQueue->Dispatch(
      NS_NewRunnableFunction("SocketProcessBackgroundChild::Shutdown", []() {
        LOG(("SocketProcessBackgroundChild::Shutdown"));
        StaticMutexAutoLock lock(sMutex);
        sInstance->Close();
        sInstance = nullptr;
        sTaskQueue = nullptr;
      }));
}

// static
already_AddRefed<nsISerialEventTarget>
SocketProcessBackgroundChild::TaskQueue() {
  StaticMutexAutoLock lock(sMutex);
  return do_AddRef(sTaskQueue);
}

// static
nsresult SocketProcessBackgroundChild::WithActor(
    const char* aName,
    MoveOnlyFunction<void(SocketProcessBackgroundChild*)> aCallback) {
  nsCOMPtr<nsISerialEventTarget> taskQueue = TaskQueue();
  if (!taskQueue) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return taskQueue->Dispatch(NS_NewRunnableFunction(
      aName, [callback = std::move(aCallback)]() mutable {
        RefPtr<SocketProcessBackgroundChild> actor =
            SocketProcessBackgroundChild::GetSingleton();
        if (actor) {
          callback(actor);
        }
      }));
}

SocketProcessBackgroundChild::SocketProcessBackgroundChild() {
  LOG(("SocketProcessBackgroundChild ctor"));
}

SocketProcessBackgroundChild::~SocketProcessBackgroundChild() {
  LOG(("SocketProcessBackgroundChild dtor"));
}

}  // namespace mozilla::net
