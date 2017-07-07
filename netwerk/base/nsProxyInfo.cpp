/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyInfo.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace net {

// Yes, we support QI to nsProxyInfo
NS_IMPL_ISUPPORTS(nsProxyInfo, nsProxyInfo, nsIProxyInfo)

NS_IMETHODIMP
nsProxyInfo::GetHost(nsACString &result)
{
  result = mHost;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetPort(int32_t *result)
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
nsProxyInfo::GetFlags(uint32_t *result)
{
  *result = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetResolveFlags(uint32_t *result)
{
  *result = mResolveFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetUsername(nsACString &result)
{
  result = mUsername;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetPassword(nsACString &result)
{
  result = mPassword;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFailoverTimeout(uint32_t *result)
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

// These pointers are declared in nsProtocolProxyService.cpp and
// comparison of mType by string pointer is valid within necko
  extern const char kProxyType_HTTP[];
  extern const char kProxyType_HTTPS[];
  extern const char kProxyType_SOCKS[];
  extern const char kProxyType_SOCKS4[];
  extern const char kProxyType_SOCKS5[];
  extern const char kProxyType_DIRECT[];

bool
nsProxyInfo::IsDirect()
{
  if (!mType)
    return true;
  return mType == kProxyType_DIRECT;
}

bool
nsProxyInfo::IsHTTP()
{
  return mType == kProxyType_HTTP;
}

bool
nsProxyInfo::IsHTTPS()
{
  return mType == kProxyType_HTTPS;
}

bool
nsProxyInfo::IsSOCKS()
{
  return mType == kProxyType_SOCKS ||
    mType == kProxyType_SOCKS4 || mType == kProxyType_SOCKS5;
}

} // namespace net
} // namespace mozilla
