/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessBridgeParent.h"
#include "SocketProcessLogging.h"

#ifdef MOZ_WEBRTC
#  include "mozilla/dom/MediaTransportParent.h"
#endif
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "SocketProcessChild.h"
#include "mozilla/net/BackgroundDataBridgeParent.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

SocketProcessBridgeParent::SocketProcessBridgeParent(ProcessId aId) : mId(aId) {
  LOG(
      ("CONSTRUCT SocketProcessBridgeParent::SocketProcessBridgeParent "
       "mId=%" PRIPID "\n",
       mId));
}

SocketProcessBridgeParent::~SocketProcessBridgeParent() {
  LOG(("DESTRUCT SocketProcessBridgeParent::SocketProcessBridgeParent\n"));
}

mozilla::ipc::IPCResult SocketProcessBridgeParent::RecvInitBackgroundDataBridge(
    mozilla::ipc::Endpoint<PBackgroundDataBridgeParent>&& aEndpoint,
    uint64_t aChannelID) {
  LOG(("SocketProcessBridgeParent::RecvInitBackgroundDataBridge\n"));

  nsCOMPtr<nsISerialEventTarget> transportQueue;
  if (NS_FAILED(NS_CreateBackgroundTaskQueue("BackgroundDataBridge",
                                             getter_AddRefs(transportQueue)))) {
    return IPC_FAIL(this, "NS_CreateBackgroundTaskQueue failed");
  }

  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "Invalid endpoint");
  }

  transportQueue->Dispatch(NS_NewRunnableFunction(
      "BackgroundDataBridgeParent::Bind",
      [endpoint = std::move(aEndpoint), aChannelID]() mutable {
        RefPtr<net::BackgroundDataBridgeParent> actor =
            new net::BackgroundDataBridgeParent(aChannelID);
        endpoint.Bind(actor);
      }));
  return IPC_OK();
}

#ifdef MOZ_WEBRTC
mozilla::ipc::IPCResult SocketProcessBridgeParent::RecvInitMediaTransport(
    mozilla::ipc::Endpoint<mozilla::dom::PMediaTransportParent>&& aEndpoint) {
  LOG(("SocketProcessBridgeParent::RecvInitMediaTransport\n"));

  if (!aEndpoint.IsValid()) {
    return IPC_FAIL(this, "Invalid endpoint");
  }

  if (!mMediaTransportTaskQueue) {
    nsCOMPtr<nsISerialEventTarget> transportQueue;
    if (NS_FAILED(NS_CreateBackgroundTaskQueue(
            "MediaTransport", getter_AddRefs(transportQueue)))) {
      return IPC_FAIL(this, "NS_CreateBackgroundTaskQueue failed");
    }

    mMediaTransportTaskQueue = std::move(transportQueue);
  }

  mMediaTransportTaskQueue->Dispatch(NS_NewRunnableFunction(
      "BackgroundDataBridgeParent::Bind",
      [endpoint = std::move(aEndpoint)]() mutable {
        RefPtr<MediaTransportParent> actor = new MediaTransportParent();
        endpoint.Bind(actor);
      }));
  return IPC_OK();
}
#endif

void SocketProcessBridgeParent::ActorDestroy(ActorDestroyReason aReason) {
  // See bug 1846478. We might be able to remove this dispatch.
  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction(
      "SocketProcessBridgeParent::ActorDestroy", [id = mId] {
        if (SocketProcessChild* child = SocketProcessChild::GetSingleton()) {
          child->DestroySocketProcessBridgeParent(id);
        }
      }));
}

}  // namespace net
}  // namespace mozilla
