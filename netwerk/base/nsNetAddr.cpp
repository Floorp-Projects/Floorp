/* vim: et ts=2 sw=2 tw=80
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetAddr.h"
#include "nsString.h"
#include "mozilla/net/DNS.h"

using namespace mozilla::net;

NS_IMPL_ISUPPORTS(nsNetAddr, nsINetAddr)

/* Makes a copy of |addr| */
nsNetAddr::nsNetAddr(NetAddr* addr) {
  NS_ASSERTION(addr, "null addr");
  mAddr = *addr;
}

NS_IMETHODIMP nsNetAddr::GetFamily(uint16_t* aFamily) {
  switch (mAddr.raw.family) {
    case AF_INET:
      *aFamily = nsINetAddr::FAMILY_INET;
      break;
    case AF_INET6:
      *aFamily = nsINetAddr::FAMILY_INET6;
      break;
#if defined(XP_UNIX)
    case AF_LOCAL:
      *aFamily = nsINetAddr::FAMILY_LOCAL;
      break;
#endif
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNetAddr::GetAddress(nsACString& aAddress) {
  switch (mAddr.raw.family) {
    /* PR_NetAddrToString can handle INET and INET6, but not LOCAL. */
    case AF_INET:
      aAddress.SetLength(kIPv4CStrBufSize);
      NetAddrToString(&mAddr, aAddress.BeginWriting(), kIPv4CStrBufSize);
      aAddress.SetLength(strlen(aAddress.BeginReading()));
      break;
    case AF_INET6:
      aAddress.SetLength(kIPv6CStrBufSize);
      NetAddrToString(&mAddr, aAddress.BeginWriting(), kIPv6CStrBufSize);
      aAddress.SetLength(strlen(aAddress.BeginReading()));
      break;
#if defined(XP_UNIX)
    case AF_LOCAL:
      aAddress.Assign(mAddr.local.path);
      break;
#endif
    // PR_AF_LOCAL falls through to default when not XP_UNIX
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNetAddr::GetPort(uint16_t* aPort) {
  switch (mAddr.raw.family) {
    case AF_INET:
      *aPort = ntohs(mAddr.inet.port);
      break;
    case AF_INET6:
      *aPort = ntohs(mAddr.inet6.port);
      break;
#if defined(XP_UNIX)
    case AF_LOCAL:
      // There is no port number for local / connections.
      return NS_ERROR_NOT_AVAILABLE;
#endif
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNetAddr::GetFlow(uint32_t* aFlow) {
  switch (mAddr.raw.family) {
    case AF_INET6:
      *aFlow = ntohl(mAddr.inet6.flowinfo);
      break;
    case AF_INET:
#if defined(XP_UNIX)
    case AF_LOCAL:
#endif
      // only for IPv6
      return NS_ERROR_NOT_AVAILABLE;
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNetAddr::GetScope(uint32_t* aScope) {
  switch (mAddr.raw.family) {
    case AF_INET6:
      *aScope = ntohl(mAddr.inet6.scope_id);
      break;
    case AF_INET:
#if defined(XP_UNIX)
    case AF_LOCAL:
#endif
      // only for IPv6
      return NS_ERROR_NOT_AVAILABLE;
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNetAddr::GetIsV4Mapped(bool* aIsV4Mapped) {
  switch (mAddr.raw.family) {
    case AF_INET6:
      *aIsV4Mapped = IPv6ADDR_IS_V4MAPPED(&mAddr.inet6.ip);
      break;
    case AF_INET:
#if defined(XP_UNIX)
    case AF_LOCAL:
#endif
      // only for IPv6
      return NS_ERROR_NOT_AVAILABLE;
    default:
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNetAddr::GetNetAddr(NetAddr* aResult) {
  memcpy(aResult, &mAddr, sizeof(mAddr));
  return NS_OK;
}
