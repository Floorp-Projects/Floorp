/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@netscape.com>
 */

#include "nsTLSSocketProvider.h"
#include "nsNSSIOLayer.h"

nsTLSSocketProvider::nsTLSSocketProvider()
{
}

nsTLSSocketProvider::~nsTLSSocketProvider()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsTLSSocketProvider, nsISocketProvider,
                              nsISSLSocketProvider);

NS_IMETHODIMP
nsTLSSocketProvider::NewSocket(const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRFileDesc **_result,
                               nsISupports **securityInfo)
{
  nsresult rv = nsSSLIOLayerNewSocket(host,
                                      port,
                                      proxyHost,
                                      proxyPort,
                                      _result,
                                      securityInfo,
                                      PR_TRUE);
  
  return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}

// Add the SSL IO layer to an existing socket
NS_IMETHODIMP
nsTLSSocketProvider::AddToSocket(const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc *aSocket,
                                 nsISupports **securityInfo)
{
  nsresult rv = nsSSLIOLayerAddToSocket(host,
                                        port,
                                        proxyHost,
                                        proxyPort,
                                        aSocket,
                                        securityInfo,
                                        PR_TRUE);
  
  return (NS_FAILED(rv)) ? NS_ERROR_SOCKET_CREATE_FAILED : NS_OK;
}
