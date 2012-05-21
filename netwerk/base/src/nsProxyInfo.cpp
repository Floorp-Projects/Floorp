/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyInfo.h"
#include "nsCOMPtr.h"

// Yes, we support QI to nsProxyInfo
NS_IMPL_THREADSAFE_ISUPPORTS2(nsProxyInfo, nsProxyInfo, nsIProxyInfo) 

NS_IMETHODIMP
nsProxyInfo::GetHost(nsACString &result)
{
  result = mHost;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetPort(PRInt32 *result)
{
  *result = mPort;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetType(nsACString &result)
{
  result = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFlags(PRUint32 *result)
{
  *result = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetResolveFlags(PRUint32 *result)
{
  *result = mResolveFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFailoverTimeout(PRUint32 *result)
{
  *result = mTimeout;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFailoverProxy(nsIProxyInfo **result)
{
  NS_IF_ADDREF(*result = mNext);
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::SetFailoverProxy(nsIProxyInfo *proxy)
{
  nsCOMPtr<nsProxyInfo> pi = do_QueryInterface(proxy);
  NS_ENSURE_ARG(pi);

  pi.swap(mNext);
  return NS_OK;
}
