/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ClipboardWriteRequestParent_h
#define mozilla_ClipboardWriteRequestParent_h

#include "mozilla/PClipboardWriteRequestParent.h"
#include "nsIClipboard.h"

namespace mozilla {

namespace dom {
class ContentParent;
}

class ClipboardWriteRequestParent final
    : public PClipboardWriteRequestParent,
      public nsIAsyncClipboardRequestCallback {
  using IPCResult = mozilla::ipc::IPCResult;
  using ContentParent = mozilla::dom::ContentParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCCLIPBOARDREQUESTCALLBACK

  explicit ClipboardWriteRequestParent(ContentParent* aManager);

  nsresult Init(const int32_t& aClipboardType);

  IPCResult RecvSetData(const IPCTransferable& aTransferable);
  IPCResult Recv__delete__(nsresult aReason);

  void ActorDestroy(ActorDestroyReason aReason) override final;

 private:
  ~ClipboardWriteRequestParent();

  RefPtr<ContentParent> mManager;
  nsCOMPtr<nsIAsyncSetClipboardData> mAsyncSetClipboardData;
};

}  // namespace mozilla

#endif  // mozilla_ClipboardWriteRequestParent_h
