/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5StreamListener.h"

#include "nsHtml5StreamParserReleaser.h"

NS_IMPL_ADDREF(nsHtml5StreamListener)
NS_IMPL_RELEASE(nsHtml5StreamListener)

NS_INTERFACE_MAP_BEGIN(nsHtml5StreamListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableStreamListener)
NS_INTERFACE_MAP_END

nsHtml5StreamListener::nsHtml5StreamListener(nsHtml5StreamParser* aDelegate)
    : mDelegateMutex("nsHtml5StreamListener mDelegateMutex"),
      mDelegate(aDelegate) {
  MOZ_ASSERT(aDelegate, "Must have delegate");
  aDelegate->AddRef();
}

nsHtml5StreamListener::~nsHtml5StreamListener() { DropDelegateImpl(); }

void nsHtml5StreamListener::DropDelegate() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Must not call DropDelegate from non-main threads.");
  DropDelegateImpl();
}

void nsHtml5StreamListener::DropDelegateImpl() {
  mozilla::MutexAutoLock autoLock(mDelegateMutex);
  nsCOMPtr<nsIRunnable> releaser = new nsHtml5StreamParserReleaser(mDelegate);
  if (NS_FAILED(mDelegate->DispatchToMain(releaser.forget()))) {
    NS_WARNING("Failed to dispatch releaser event.");
  }
  mDelegate = nullptr;
}

nsHtml5StreamParser* nsHtml5StreamListener::GetDelegate() {
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  // Let's acquire the mutex in order to always access mDelegate
  // with mutex held. Since this can be called only on the main
  // thread and DropDelegate() can only be called on the main thread
  // it's OK that the mutex here doesn't protect the use of the
  // return value.
  mozilla::MutexAutoLock autoLock(mDelegateMutex);
  return mDelegate;
}

NS_IMETHODIMP
nsHtml5StreamListener::CheckListenerChain() {
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5StreamListener::OnStartRequest(nsIRequest* aRequest) {
  mozilla::MutexAutoLock autoLock(mDelegateMutex);
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mDelegate->OnStartRequest(aRequest);
}

NS_IMETHODIMP
nsHtml5StreamListener::OnStopRequest(nsIRequest* aRequest, nsresult aStatus) {
  mozilla::MutexAutoLock autoLock(mDelegateMutex);
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mDelegate->OnStopRequest(aRequest, aStatus);
}

NS_IMETHODIMP
nsHtml5StreamListener::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInStream,
                                       uint64_t aSourceOffset,
                                       uint32_t aLength) {
  mozilla::MutexAutoLock autoLock(mDelegateMutex);
  if (MOZ_UNLIKELY(!mDelegate)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mDelegate->OnDataAvailable(aRequest, aInStream, aSourceOffset,
                                    aLength);
}
