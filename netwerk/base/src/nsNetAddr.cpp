/* vim: et ts=2 sw=2 tw=80 
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetAddr.h"
#include "nsString.h"

#include "prnetdb.h"

NS_IMPL_ISUPPORTS1(nsNetAddr, nsINetAddr)

/* Makes a copy of |addr| */
nsNetAddr::nsNetAddr(PRNetAddr* addr)
{
  NS_ASSERTION(addr, "null addr");
  mAddr = *addr;
}

/* readonly attribute unsigned short family; */
NS_IMETHODIMP nsNetAddr::GetFamily(uint16_t *aFamily)
{
  switch(mAddr.raw.family) {
  case PR_AF_INET: 
    *aFamily = nsINetAddr::FAMILY_INET;
    break;
  case PR_AF_INET6:
    *aFamily = nsINetAddr::FAMILY_INET6;
    break;
  case PR_AF_LOCAL:
    *aFamily = nsINetAddr::FAMILY_LOCAL;
    break;
  default:
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

/* readonly attribute AUTF8String address; */
NS_IMETHODIMP nsNetAddr::GetAddress(nsACString & aAddress)
{
  switch(mAddr.raw.family) {
  /* PR_NetAddrToString can handle INET and INET6, but not LOCAL. */
  case PR_AF_INET: 
    aAddress.SetCapacity(16);
    PR_NetAddrToString(&mAddr, aAddress.BeginWriting(), 16);
    aAddress.SetLength(strlen(aAddress.BeginReading()));
    break;
  case PR_AF_INET6:
    aAddress.SetCapacity(46);
    PR_NetAddrToString(&mAddr, aAddress.BeginWriting(), 46);
    aAddress.SetLength(strlen(aAddress.BeginReading()));
    break;
#if defined(XP_UNIX) || defined(XP_OS2)
  case PR_AF_LOCAL:
    aAddress.Assign(mAddr.local.path);
    break;
#endif
  // PR_AF_LOCAL falls through to default when not XP_UNIX || XP_OS2
  default:
    return NS_ERROR_UNEXPECTED;
  }
  
  return NS_OK;
}

/* readonly attribute unsigned short port; */
NS_IMETHODIMP nsNetAddr::GetPort(uint16_t *aPort)
{
  switch(mAddr.raw.family) {
  case PR_AF_INET: 
    *aPort = PR_ntohs(mAddr.inet.port);
    break;
  case PR_AF_INET6:
    *aPort = PR_ntohs(mAddr.ipv6.port);
    break;
  case PR_AF_LOCAL:
    // There is no port number for local / connections.
    return NS_ERROR_NOT_AVAILABLE;
  default:
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

/* readonly attribute unsigned long flow; */
NS_IMETHODIMP nsNetAddr::GetFlow(uint32_t *aFlow)
{
  switch(mAddr.raw.family) {
  case PR_AF_INET6:
    *aFlow = PR_ntohl(mAddr.ipv6.flowinfo);
    break;
  case PR_AF_INET: 
  case PR_AF_LOCAL:
    // only for IPv6
    return NS_ERROR_NOT_AVAILABLE;
  default:
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

/* readonly attribute unsigned long scope; */
NS_IMETHODIMP nsNetAddr::GetScope(uint32_t *aScope)
{
  switch(mAddr.raw.family) {
  case PR_AF_INET6:
    *aScope = PR_ntohl(mAddr.ipv6.scope_id);
    break;
  case PR_AF_INET: 
  case PR_AF_LOCAL:
    // only for IPv6
    return NS_ERROR_NOT_AVAILABLE;
  default:
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

