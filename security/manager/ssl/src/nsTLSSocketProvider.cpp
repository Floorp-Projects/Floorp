/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTLSSocketProvider.h"
#include "nsNSSIOLayer.h"
#include "nsError.h"

nsTLSSocketProvider::nsTLSSocketProvider()
{
}

nsTLSSocketProvider::~nsTLSSocketProvider()
{
}

NS_IMPL_ISUPPORTS(nsTLSSocketProvider, nsISocketProvider)

NS_IMETHODIMP
nsTLSSocketProvider::NewSocket(int32_t family,
                               const char *host,
                               int32_t port,
                               const char *proxyHost,
                               int32_t proxyPort,
                               uint32_t flags,
                               PRFileDesc **_result,
                               nsISupports **securityInfo)
{
  nsresult rv = nsSSLIOLayerNewSocket(family,
                                      host,
                                      port,
                                      proxyHost,
                                      proxyPort,
                                      _result,
                                      securityInfo,
                                      true,
                                      flags);
  
  return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

// Add the SSL IO layer to an existing socket
NS_IMETHODIMP
nsTLSSocketProvider::AddToSocket(int32_t family,
                                 const char *host,
                                 int32_t port,
                                 const char *proxyHost,
                                 int32_t proxyPort,
                                 uint32_t flags,
                                 PRFileDesc *aSocket,
                                 nsISupports **securityInfo)
{
  nsresult rv = nsSSLIOLayerAddToSocket(family,
                                        host,
                                        port,
                                        proxyHost,
                                        proxyPort,
                                        aSocket,
                                        securityInfo,
                                        true,
                                        flags);
  
  return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
