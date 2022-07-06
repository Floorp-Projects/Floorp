/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/IPCTransportProvider.h"

#include "IPCTransportProvider.h"
#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(TransportProviderParent, nsITransportProvider,
                  nsIHttpUpgradeListener)

NS_IMETHODIMP
TransportProviderParent::SetListener(nsIHttpUpgradeListener* aListener) {
  MOZ_ASSERT(aListener);
  mListener = aListener;

  MaybeNotify();

  return NS_OK;
}

NS_IMETHODIMP
TransportProviderParent::GetIPCChild(
    mozilla::net::PTransportProviderChild** aChild) {
  MOZ_CRASH("Don't call this in parent process");
  *aChild = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
TransportProviderParent::OnTransportAvailable(
    nsISocketTransport* aTransport, nsIAsyncInputStream* aSocketIn,
    nsIAsyncOutputStream* aSocketOut) {
  MOZ_ASSERT(aTransport && aSocketOut && aSocketOut);
  mTransport = aTransport;
  mSocketIn = aSocketIn;
  mSocketOut = aSocketOut;

  MaybeNotify();

  return NS_OK;
}

NS_IMETHODIMP
TransportProviderParent::OnUpgradeFailed(nsresult aErrorCode) { return NS_OK; }

NS_IMETHODIMP
TransportProviderParent::OnWebSocketConnectionAvailable(
    WebSocketConnectionBase* aConnection) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void TransportProviderParent::MaybeNotify() {
  if (!mListener || !mTransport) {
    return;
  }

  DebugOnly<nsresult> rv =
      mListener->OnTransportAvailable(mTransport, mSocketIn, mSocketOut);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

NS_IMPL_ISUPPORTS(TransportProviderChild, nsITransportProvider)

TransportProviderChild::~TransportProviderChild() { Send__delete__(this); }

NS_IMETHODIMP
TransportProviderChild::SetListener(nsIHttpUpgradeListener* aListener) {
  MOZ_CRASH("Don't call this in child process");
  return NS_OK;
}

NS_IMETHODIMP
TransportProviderChild::GetIPCChild(
    mozilla::net::PTransportProviderChild** aChild) {
  *aChild = this;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
