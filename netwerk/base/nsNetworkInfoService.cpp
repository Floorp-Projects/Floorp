/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_MACOSX) || defined(XP_LINUX)
#include <unistd.h>
#elif defined(XP_WIN)
#include <winsock2.h>
#endif

#include "nsNetworkInfoService.h"
#include "mozilla/ScopeExit.h"

#if defined(XP_MACOSX) || defined(XP_WIN) || defined(XP_LINUX)
#include "NetworkInfoServiceImpl.h"
#else
#error "Unsupported platform for nsNetworkInfoService!  Check moz.build"
#endif

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(nsNetworkInfoService,
                  nsINetworkInfoService)

nsNetworkInfoService::nsNetworkInfoService()
{
}

nsresult
nsNetworkInfoService::Init()
{
  return NS_OK;
}

nsresult
nsNetworkInfoService::ListNetworkAddresses(nsIListNetworkAddressesListener* aListener)
{
  nsresult rv;

  AddrMapType addrMap;
  rv = DoListAddresses(addrMap);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aListener->OnListNetworkAddressesFailed();
    return NS_OK;
  }

  uint32_t addrCount = addrMap.Count();
  const char** addrStrings = (const char**) malloc(sizeof(*addrStrings) * addrCount);
  if (!addrStrings) {
    aListener->OnListNetworkAddressesFailed();
    return NS_OK;
  }
  auto autoFreeAddrStrings = MakeScopeExit([&] {
    free(addrStrings);
  });

  uint32_t idx = 0;
  for (auto iter = addrMap.Iter(); !iter.Done(); iter.Next()) {
    addrStrings[idx++] = iter.Data().get();
  }
  aListener->OnListedNetworkAddresses(addrStrings, addrCount);
  return NS_OK;
}

// TODO: Bug 1275373: https://bugzilla.mozilla.org/show_bug.cgi?id=1275373
// Use platform-specific implementation of DoGetHostname on Cocoa and Windows.
static nsresult
DoGetHostname(nsACString& aHostname)
{
  char hostnameBuf[256];
  int result = gethostname(hostnameBuf, 256);
  if (result == -1) {
    return NS_ERROR_FAILURE;
  }

  // Ensure that there is always a terminating NUL byte.
  hostnameBuf[255] = '\0';

  // Find the first '.', terminate string there.
  char* dotLocation = strchr(hostnameBuf, '.');
  if (dotLocation) {
    *dotLocation = '\0';
  }

  if (strlen(hostnameBuf) == 0) {
    return NS_ERROR_FAILURE;
  }

  aHostname.AssignASCII(hostnameBuf);
  return NS_OK;
}

nsresult
nsNetworkInfoService::GetHostname(nsIGetHostnameListener* aListener)
{
  nsresult rv;
  nsCString hostnameStr;
  rv = DoGetHostname(hostnameStr);
  if (NS_FAILED(rv)) {
    aListener->OnGetHostnameFailed();
    return NS_OK;
  }

  aListener->OnGotHostname(hostnameStr);

  return NS_OK;
}

} // namespace net
} // namespace mozilla
