/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRedirectHistoryEntry.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(nsRedirectHistoryEntry, nsIRedirectHistoryEntry)

nsRedirectHistoryEntry::nsRedirectHistoryEntry(nsIPrincipal* aPrincipal,
                                               nsIURI* aReferrer,
                                               const nsACString& aRemoteAddress)
  : mPrincipal(aPrincipal)
  , mReferrer(aReferrer)
  , mRemoteAddress(aRemoteAddress)
{
}

NS_IMETHODIMP
nsRedirectHistoryEntry::GetRemoteAddress(nsACString &result)
{
  result = mRemoteAddress;
  return NS_OK;
}

NS_IMETHODIMP
nsRedirectHistoryEntry::GetReferrerURI(nsIURI** referrer)
{
  NS_IF_ADDREF(*referrer = mReferrer);
  return NS_OK;
}

NS_IMETHODIMP
nsRedirectHistoryEntry::GetPrincipal(nsIPrincipal** principal)
{
  NS_IF_ADDREF(*principal = mPrincipal);
  return NS_OK;
}

} // namespace net
} // namespace mozilla
