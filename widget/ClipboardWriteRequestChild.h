/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ClipboardWriteRequestChild_h
#define mozilla_ClipboardWriteRequestChild_h

#include "mozilla/PClipboardWriteRequestChild.h"
#include "nsIClipboard.h"

class nsITransferable;

namespace mozilla {

class ClipboardWriteRequestChild : public PClipboardWriteRequestChild,
                                   public nsIAsyncSetClipboardData {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIASYNCSETCLIPBOARDDATA

  explicit ClipboardWriteRequestChild(
      nsIAsyncClipboardRequestCallback* aCallback)
      : mCallback(aCallback) {}

  ipc::IPCResult Recv__delete__(nsresult aResult);
  void ActorDestroy(ActorDestroyReason aReason) override final;

 protected:
  virtual ~ClipboardWriteRequestChild() = default;

  void MaybeNotifyCallback(nsresult aResult);

  bool mIsValid = true;
  nsCOMPtr<nsIAsyncClipboardRequestCallback> mCallback;
};

}  // namespace mozilla

#endif  // mozilla_ClipboardWriteRequestChild_h
