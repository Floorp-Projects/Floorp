/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClipboardWriteRequestChild.h"

#if defined(ACCESSIBILITY) && defined(XP_WIN)
#  include "mozilla/a11y/Compatibility.h"
#endif
#include "mozilla/dom/ContentChild.h"
#include "mozilla/net/CookieJarSettings.h"
#include "nsITransferable.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(ClipboardWriteRequestChild, nsIAsyncSetClipboardData)

NS_IMETHODIMP
ClipboardWriteRequestChild::SetData(nsITransferable* aTransferable,
                                    nsIClipboardOwner* aOwner) {
  MOZ_ASSERT(aTransferable);
  // Callback should be notified if actor is destroyed.
  MOZ_ASSERT_IF(!CanSend(), !mIsValid && !mCallback);

  if (!mIsValid) {
    return NS_ERROR_FAILURE;
  }

#if defined(ACCESSIBILITY) && defined(XP_WIN)
  a11y::Compatibility::SuppressA11yForClipboardCopy();
#endif

  mIsValid = false;
  IPCTransferable ipcTransferable;
  nsContentUtils::TransferableToIPCTransferable(aTransferable, &ipcTransferable,
                                                false, nullptr);
  SendSetData(std::move(ipcTransferable));
  return NS_OK;
}

NS_IMETHODIMP ClipboardWriteRequestChild::Abort(nsresult aReason) {
  // Callback should be notified if actor is destroyed.
  MOZ_ASSERT_IF(!CanSend(), !mIsValid && !mCallback);

  if (!mIsValid || !NS_FAILED(aReason)) {
    return NS_ERROR_FAILURE;
  }

  // Need to notify callback first to propagate reason properly.
  MaybeNotifyCallback(aReason);
  Unused << PClipboardWriteRequestChild::Send__delete__(this, aReason);
  return NS_OK;
}

ipc::IPCResult ClipboardWriteRequestChild::Recv__delete__(nsresult aResult) {
  MaybeNotifyCallback(aResult);
  return IPC_OK();
}

void ClipboardWriteRequestChild::ActorDestroy(ActorDestroyReason aReason) {
  MaybeNotifyCallback(NS_ERROR_ABORT);
}

void ClipboardWriteRequestChild::MaybeNotifyCallback(nsresult aResult) {
  mIsValid = false;
  if (nsCOMPtr<nsIAsyncClipboardRequestCallback> callback =
          mCallback.forget()) {
    callback->OnComplete(aResult);
  }
}

}  // namespace mozilla
