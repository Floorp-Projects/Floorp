/* vim: et ts=2 sw=2 tw=80 
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code
 *
 * The Initial Developer of the Original Code is
 *   Derrick Rice <derrick.rice@gmail.com>
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
NS_IMETHODIMP nsNetAddr::GetFamily(PRUint16 *aFamily)
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
    if(!aAddress.SetCapacity(16))
      return NS_ERROR_OUT_OF_MEMORY;
    PR_NetAddrToString(&mAddr, aAddress.BeginWriting(), 16);
    aAddress.SetLength(strlen(aAddress.BeginReading()));
    break;
  case PR_AF_INET6:
    if(!aAddress.SetCapacity(46))
      return NS_ERROR_OUT_OF_MEMORY;
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
NS_IMETHODIMP nsNetAddr::GetPort(PRUint16 *aPort)
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
NS_IMETHODIMP nsNetAddr::GetFlow(PRUint32 *aFlow)
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
NS_IMETHODIMP nsNetAddr::GetScope(PRUint32 *aScope)
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

