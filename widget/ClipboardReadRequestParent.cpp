/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClipboardReadRequestParent.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/net/CookieJarSettings.h"
#include "nsComponentManagerUtils.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsWidgetsCID.h"

using mozilla::dom::ContentParent;
using mozilla::ipc::IPCResult;

namespace mozilla {

namespace {

class ClipboardGetDataCallback final : public nsIAsyncClipboardRequestCallback {
 public:
  explicit ClipboardGetDataCallback(std::function<void(nsresult)>&& aCallback)
      : mCallback(std::move(aCallback)) {}

  // This object will never be held by a cycle-collected object, so it doesn't
  // need to be cycle-collected despite holding alive cycle-collected objects.
  NS_DECL_ISUPPORTS

  // nsIAsyncClipboardRequestCallback
  NS_IMETHOD OnComplete(nsresult aResult) override {
    mCallback(aResult);
    return NS_OK;
  }

 protected:
  ~ClipboardGetDataCallback() = default;

  std::function<void(nsresult)> mCallback;
};

NS_IMPL_ISUPPORTS(ClipboardGetDataCallback, nsIAsyncClipboardRequestCallback)

static Result<nsCOMPtr<nsITransferable>, nsresult> CreateTransferable(
    const nsTArray<nsCString>& aTypes) {
  nsresult rv;
  nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1", &rv);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  MOZ_TRY(trans->Init(nullptr));
  // The private flag is only used to prevent the data from being cached to the
  // disk. The flag is not exported to the IPCDataTransfer object.
  // The flag is set because we are not sure whether the clipboard data is used
  // in a private browsing context. The transferable is only used in this scope,
  // so the cache would not reduce memory consumption anyway.
  trans->SetIsPrivateData(true);
  // Fill out flavors for transferable
  for (uint32_t t = 0; t < aTypes.Length(); t++) {
    MOZ_TRY(trans->AddDataFlavor(aTypes[t].get()));
  }

  return std::move(trans);
}

}  // namespace

IPCResult ClipboardReadRequestParent::RecvGetData(
    const nsTArray<nsCString>& aFlavors, GetDataResolver&& aResolver) {
  bool valid = false;
  if (NS_FAILED(mAsyncGetClipboardData->GetValid(&valid)) || !valid) {
    Unused << PClipboardReadRequestParent::Send__delete__(this);
    aResolver(NS_ERROR_FAILURE);
    return IPC_OK();
  }

  // Create transferable
  auto result = CreateTransferable(aFlavors);
  if (result.isErr()) {
    aResolver(result.unwrapErr());
    return IPC_OK();
  }

  nsCOMPtr<nsITransferable> trans = result.unwrap();
  RefPtr<ClipboardGetDataCallback> callback =
      MakeRefPtr<ClipboardGetDataCallback>([self = RefPtr{this},
                                            resolver = std::move(aResolver),
                                            trans,
                                            manager = mManager](nsresult aRv) {
        if (NS_FAILED(aRv)) {
          bool valid = false;
          if (NS_FAILED(self->mAsyncGetClipboardData->GetValid(&valid)) ||
              !valid) {
            Unused << PClipboardReadRequestParent::Send__delete__(self);
          }
          resolver(aRv);
          return;
        }

        dom::IPCTransferableData ipcTransferableData;
        nsContentUtils::TransferableToIPCTransferableData(
            trans, &ipcTransferableData, false /* aInSyncMessage */, manager);
        resolver(std::move(ipcTransferableData));
      });
  nsresult rv = mAsyncGetClipboardData->GetData(trans, callback);
  if (NS_FAILED(rv)) {
    callback->OnComplete(rv);
  }
  return IPC_OK();
}

}  // namespace mozilla
