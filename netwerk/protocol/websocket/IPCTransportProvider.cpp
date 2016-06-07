/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/IPCTransportProvider.h"

#include "nsISocketTransport.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(TransportProviderParent,
                  nsITransportProvider,
                  nsIHttpUpgradeListener)

TransportProviderParent::TransportProviderParent()
{
  MOZ_COUNT_CTOR(TransportProviderParent);
}

TransportProviderParent::~TransportProviderParent()
{
  MOZ_COUNT_DTOR(TransportProviderParent);
}

NS_IMETHODIMP
TransportProviderParent::SetListener(nsIHttpUpgradeListener* aListener)
{
  MOZ_ASSERT(aListener);
  mListener = aListener;

  MaybeNotify();

  return NS_OK;
}

NS_IMETHODIMP_(mozilla::net::PTransportProviderChild*)
TransportProviderParent::GetIPCChild()
{
  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE();
  return nullptr;
}

NS_IMETHODIMP
TransportProviderParent::OnTransportAvailable(nsISocketTransport* aTransport,
                                              nsIAsyncInputStream* aSocketIn,
                                              nsIAsyncOutputStream* aSocketOut)
{
  MOZ_ASSERT(aTransport && aSocketOut && aSocketOut);
  mTransport = aTransport;
  mSocketIn = aSocketIn;
  mSocketOut = aSocketOut;

  MaybeNotify();

  return NS_OK;
}

void
TransportProviderParent::MaybeNotify()
{
  if (!mListener || !mTransport) {
    return;
  }

  mListener->OnTransportAvailable(mTransport, mSocketIn, mSocketOut);
}


NS_IMPL_ISUPPORTS(TransportProviderChild,
                  nsITransportProvider)

TransportProviderChild::TransportProviderChild()
{
  MOZ_COUNT_CTOR(TransportProviderChild);
}

TransportProviderChild::~TransportProviderChild()
{
  MOZ_COUNT_DTOR(TransportProviderChild);
  Send__delete__(this);
}

NS_IMETHODIMP
TransportProviderChild::SetListener(nsIHttpUpgradeListener* aListener)
{
  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE();
  return NS_OK;
}

NS_IMETHODIMP_(mozilla::net::PTransportProviderChild*)
TransportProviderChild::GetIPCChild()
{
  return this;
}

}
}
