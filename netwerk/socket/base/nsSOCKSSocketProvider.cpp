/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Bradford <jab@atdot.org>
 *   Darin Fisher <darin@meer.net>
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

#include "nsIServiceManager.h"
#include "nsSOCKSSocketProvider.h"
#include "nsSOCKSIOLayer.h"
#include "nsCOMPtr.h"
#include "nsNetError.h"

//////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSOCKSSocketProvider, nsISocketProvider)

NS_METHOD
nsSOCKSSocketProvider::CreateV4(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsCOMPtr<nsISocketProvider> inst =
            new nsSOCKSSocketProvider(NS_SOCKS_VERSION_4);
    if (!inst)
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        rv = inst->QueryInterface(aIID, aResult); 
    return rv;
}

NS_METHOD
nsSOCKSSocketProvider::CreateV5(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsCOMPtr<nsISocketProvider> inst =
            new nsSOCKSSocketProvider(NS_SOCKS_VERSION_5);
    if (!inst)
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        rv = inst->QueryInterface(aIID, aResult); 
    return rv;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::NewSocket(PRInt32 family,
                                 const char *host, 
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc **result, 
                                 nsISupports **socksInfo)
{
    PRFileDesc *sock;
    
    sock = PR_OpenTCPSocket(family);
    if (!sock)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = nsSOCKSIOLayerAddToSocket(family,
                                            host, 
                                            port,
                                            proxyHost,
                                            proxyPort,
                                            mVersion,
                                            sock, 
                                            socksInfo);
    if (NS_SUCCEEDED(rv)) {
        *result = sock;
        return NS_OK;
    }

    return NS_ERROR_SOCKET_CREATE_FAILED;
}

NS_IMETHODIMP
nsSOCKSSocketProvider::AddToSocket(PRInt32 family,
                                   const char *host,
                                   PRInt32 port,
                                   const char *proxyHost,
                                   PRInt32 proxyPort,
                                   PRFileDesc *sock, 
                                   nsISupports **socksInfo)
{
    nsresult rv = nsSOCKSIOLayerAddToSocket(family,
                                            host, 
                                            port,
                                            proxyHost,
                                            proxyPort,
                                            mVersion,
                                            sock, 
                                            socksInfo);
    
    if (NS_FAILED(rv))
        rv = NS_ERROR_SOCKET_CREATE_FAILED;
    return rv;
}
