/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUDPSocketProvider.h"

#include "nspr.h"

using mozilla::OriginAttributes;

NS_IMPL_ISUPPORTS(nsUDPSocketProvider, nsISocketProvider)

NS_IMETHODIMP
nsUDPSocketProvider::NewSocket(int32_t aFamily, const char* aHost,
                               int32_t aPort, nsIProxyInfo* aProxy,
                               const OriginAttributes& originAttributes,
                               uint32_t aFlags, uint32_t aTlsFlags,
                               PRFileDesc** aFileDesc,
                               nsISSLSocketControl** aTLSSocketControl) {
  NS_ENSURE_ARG_POINTER(aFileDesc);

  PRFileDesc* udpFD = PR_OpenUDPSocket(aFamily);
  if (!udpFD) return NS_ERROR_FAILURE;

  *aFileDesc = udpFD;
  return NS_OK;
}

NS_IMETHODIMP
nsUDPSocketProvider::AddToSocket(int32_t aFamily, const char* aHost,
                                 int32_t aPort, nsIProxyInfo* aProxy,
                                 const OriginAttributes& originAttributes,
                                 uint32_t aFlags, uint32_t aTlsFlags,
                                 struct PRFileDesc* aFileDesc,
                                 nsISSLSocketControl** aTLSSocketControl) {
  // does not make sense to strap a UDP socket onto an existing socket
  MOZ_ASSERT_UNREACHABLE("Cannot layer UDP socket on an existing socket");
  return NS_ERROR_UNEXPECTED;
}
