/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SocketProcessBridgeParent.h"
#include "SocketProcessLogging.h"

#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "SocketProcessChild.h"

namespace mozilla {
namespace net {

SocketProcessBridgeParent::SocketProcessBridgeParent(ProcessId aId)
    : mId(aId), mClosed(false) {
  LOG(
      ("CONSTRUCT SocketProcessBridgeParent::SocketProcessBridgeParent "
       "mId=%" PRIPID "\n",
       mId));
  MOZ_COUNT_CTOR(SocketProcessBridgeParent);
}

SocketProcessBridgeParent::~SocketProcessBridgeParent() {
  LOG(
      ("DESTRUCT SocketProcessBridgeParent::SocketProcessBridgeParent "
       "mId=%" PRIPID "\n",
       mId));
  MOZ_COUNT_DTOR(SocketProcessBridgeParent);
}

mozilla::ipc::IPCResult SocketProcessBridgeParent::RecvTest() {
  LOG(("SocketProcessBridgeParent::RecvTest\n"));
  Unused << SendTest();
  return IPC_OK();
}

mozilla::ipc::IPCResult SocketProcessBridgeParent::RecvInitBackground(
    Endpoint<PBackgroundStarterParent>&& aEndpoint) {
  LOG(("SocketProcessBridgeParent::RecvInitBackground mId=%" PRIPID "\n", mId));
  if (!ipc::BackgroundParent::AllocStarter(nullptr, std::move(aEndpoint))) {
    return IPC_FAIL(this, "BackgroundParent::Alloc failed");
  }

  return IPC_OK();
}

void SocketProcessBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("SocketProcessBridgeParent::ActorDestroy mId=%" PRIPID "\n", mId));

  mClosed = true;
  GetCurrentSerialEventTarget()->Dispatch(
      NewRunnableMethod("net::SocketProcessBridgeParent::DeferredDestroy", this,
                        &SocketProcessBridgeParent::DeferredDestroy));
}

void SocketProcessBridgeParent::DeferredDestroy() {
  if (SocketProcessChild* child = SocketProcessChild::GetSingleton()) {
    child->DestroySocketProcessBridgeParent(mId);
  }
}

}  // namespace net
}  // namespace mozilla
