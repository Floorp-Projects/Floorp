/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSSLSocketProvider.h"
#include "nsNSSIOLayer.h"
#include "nsNetError.h"

nsSSLSocketProvider::nsSSLSocketProvider()
{
}

nsSSLSocketProvider::~nsSSLSocketProvider()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSSLSocketProvider, nsISocketProvider)

NS_IMETHODIMP
nsSSLSocketProvider::NewSocket(PRInt32 family,
                               const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRUint32 flags,
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
                                      false,
                                      flags & ANONYMOUS_CONNECT);
  return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

// Add the SSL IO layer to an existing socket
NS_IMETHODIMP
nsSSLSocketProvider::AddToSocket(PRInt32 family,
                                 const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRUint32 flags,
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
                                        false,
                                        flags & ANONYMOUS_CONNECT);
  
  return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
