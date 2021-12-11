/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/AltDataOutputStreamParent.h"
#include "mozilla/Unused.h"
#include "nsIAsyncOutputStream.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS0(AltDataOutputStreamParent)

AltDataOutputStreamParent::AltDataOutputStreamParent(nsIOutputStream* aStream)
    : mOutputStream(aStream), mStatus(NS_OK), mIPCOpen(true) {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
}

AltDataOutputStreamParent::~AltDataOutputStreamParent() {
  MOZ_ASSERT(NS_IsMainThread(), "Main thread only");
}

mozilla::ipc::IPCResult AltDataOutputStreamParent::RecvWriteData(
    const nsCString& data) {
  if (NS_FAILED(mStatus)) {
    if (mIPCOpen) {
      Unused << SendError(mStatus);
    }
    return IPC_OK();
  }
  nsresult rv;
  uint32_t n;
  if (mOutputStream) {
    rv = mOutputStream->Write(data.BeginReading(), data.Length(), &n);
    MOZ_ASSERT(n == data.Length() || NS_FAILED(rv));
    if (NS_FAILED(rv) && mIPCOpen) {
      Unused << SendError(rv);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult AltDataOutputStreamParent::RecvClose(
    const nsresult& aStatus) {
  if (NS_FAILED(mStatus)) {
    if (mIPCOpen) {
      Unused << SendError(mStatus);
    }
    return IPC_OK();
  }

  if (!mOutputStream) {
    return IPC_OK();
  }

  nsCOMPtr<nsIAsyncOutputStream> asyncOutputStream =
      do_QueryInterface(mOutputStream);
  MOZ_ASSERT(asyncOutputStream);

  nsresult rv = asyncOutputStream->CloseWithStatus(aStatus);
  if (NS_FAILED(rv) && mIPCOpen) {
    Unused << SendError(rv);
  }

  mOutputStream = nullptr;
  return IPC_OK();
}

void AltDataOutputStreamParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

mozilla::ipc::IPCResult AltDataOutputStreamParent::RecvDeleteSelf() {
  mIPCOpen = false;
  Unused << SendDeleteSelf();
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
