/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WeakCryptoOverride.h"

#include "MainThreadUtils.h"
#include "SharedSSLState.h"

using namespace mozilla;
using namespace mozilla::psm;

NS_IMPL_ISUPPORTS(WeakCryptoOverride,
                  nsIWeakCryptoOverride)

WeakCryptoOverride::WeakCryptoOverride()
{
}

WeakCryptoOverride::~WeakCryptoOverride()
{
}

NS_IMETHODIMP
WeakCryptoOverride::AddWeakCryptoOverride(const nsACString& aHostName,
                                          bool aPrivate, bool aTemporary)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  SharedSSLState* sharedState = aPrivate ? PrivateSSLState()
                                         : PublicSSLState();
  if (!sharedState) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const nsPromiseFlatCString& host = PromiseFlatCString(aHostName);
  sharedState->IOLayerHelpers().addInsecureFallbackSite(host, aTemporary);

  return NS_OK;
}

NS_IMETHODIMP
WeakCryptoOverride::RemoveWeakCryptoOverride(const nsACString& aHostName,
                                             int32_t aPort, bool aPrivate)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_SAME_THREAD;
  }

  SharedSSLState* sharedState = aPrivate ? PrivateSSLState()
                                         : PublicSSLState();
  if (!sharedState) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  const nsPromiseFlatCString& host = PromiseFlatCString(aHostName);
  sharedState->IOLayerHelpers().removeInsecureFallbackSite(host, aPort);

  return NS_OK;
}
